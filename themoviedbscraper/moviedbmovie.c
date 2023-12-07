#include <string>
#include "moviedbmovie.h"

cMovieDbMovie::cMovieDbMovie(cTVScraperDB *db, cMovieDBScraper *movieDBScraper):
  m_db(db),
  m_movieDBScraper(movieDBScraper) { }

bool cMovieDbMovie::ReadAndStore(int id) {
  const char *lang = config.GetDefaultLanguage()->m_themoviedb;
  cToSvConcat url(m_baseURL, "/movie/", id, "?api_key=", m_movieDBScraper->GetApiKey(), "&language=", lang, "&append_to_response=credits");
  cJsonDocumentFromUrl document;
  document.set_enableDebug(config.enableDebug);
  if (!document.download_and_parse(url.c_str())) return false;
//  if (!jsonCallRest(document, buffer, url.c_str(), config.enableDebug)) return false;
  if (!ReadMovie(document, id)) {
    esyslog("tvscraper: ERROR reading movie \"%i\" ", id);
    return false;
  }

// actors =======================
  rapidjson::Value::ConstMemberIterator credits_it = document.FindMember("credits");
  if (credits_it != document.MemberEnd() && credits_it->value.IsObject() ) {
    std::string director;
    std::string writer;
    getDirectorWriter(director, writer, credits_it->value);
    if (!director.empty() || ! writer.empty() )
      m_db->exec("UPDATE movie_runtime2 SET movie_director = ?, movie_writer = ? WHERE movie_id = ?", director, writer, id);
// add actors to database
    readAndStoreMovieDbActors(m_db, credits_it->value, id, true);
  }
  return true;
}
bool cMovieDbMovie::ReadMovie(const rapidjson::Value &movie, int id) {
  const char *title = getValueCharS(movie, "title");
  const char *originalTitle = getValueCharS(movie, "original_title");
  const char *tagline = getValueCharS(movie, "tagline");
  const char *overview = getValueCharS(movie, "overview");
  bool adult = getValueBool(movie, "adult");
// collection
  rapidjson::Value::ConstMemberIterator collection_it = movie.FindMember("belongs_to_collection");
  bool hasCol = collection_it != movie.MemberEnd() && collection_it->value.IsObject();
  int collectionId = hasCol?getValueInt(collection_it->value, "id"):0;
  const char *collectionName = hasCol?getValueCharS(collection_it->value, "name"):nullptr;
  const char *collectionPosterPath = hasCol?getValueCharS(collection_it->value, "poster_path"):nullptr;
  const char *collectionBackdropPath = hasCol?getValueCharS(collection_it->value, "backdrop_path"):nullptr;
// budget, revenue
  int budget  = getValueInt(movie, "budget");
  int revenue  = getValueInt(movie, "revenue");
// genres, productionCountries, imdb_id
  std::string genres = getValueArrayConcatenated(movie, "genres", "name");
  std::string productionCountries = getValueArrayConcatenated(movie, "production_countries", "name");
  const char *imdb_id = getValueCharS(movie, "imdb_id");
// homepage, releaseDate, runtime
  const char *homepage = getValueCharS(movie, "homepage");
  const char *releaseDate = getValueCharS(movie, "release_date");
  int runtime = getValueInt(movie, "runtime");
// popularity, voteAverage
  float popularity = getValueDouble(movie, "popularity");
  float voteAverage = getValueDouble(movie, "vote_average");
  int voteCount = getValueInt(movie, "vote_count");

// backdropPath, posterPath
  const char *backdropPath = getValueCharS(movie, "backdrop_path");
  const char *posterPath = getValueCharS(movie, "poster_path");
// store in db
  const char *poster;
  const char *fanart;
  if (posterPath && *posterPath    ) poster = posterPath;   else poster = collectionPosterPath;
  if (backdropPath && *backdropPath) fanart = backdropPath; else fanart = collectionBackdropPath;
  m_db->InsertMovie(id, title, originalTitle, tagline, overview, adult, collectionId, collectionName, budget, revenue, genres.c_str(), homepage, releaseDate, runtime, popularity, voteAverage, voteCount, productionCountries.c_str(), poster, fanart, imdb_id);

// store media in db, for later download
  cSql stmt(m_db, "INSERT OR REPLACE INTO tv_media (tv_id, media_path, media_type, media_number) VALUES (?, ?, ?, ?);");
  if (posterPath && *posterPath)      stmt.resetBindStep(id, posterPath,   mediaPoster, -100);
  if (backdropPath && * backdropPath) stmt.resetBindStep(id, backdropPath, mediaFanart, -100);
  if (collectionPosterPath && *collectionPosterPath)     stmt.resetBindStep(id, collectionPosterPath,   mediaPosterCollection, collectionId * -1);
  if (collectionBackdropPath && *collectionBackdropPath) stmt.resetBindStep(id, collectionBackdropPath, mediaFanartCollection, collectionId * -1);
//  m_db->insertTvMediaSeasonPoster (id, posterPath,   mediaPoster, -100);
//  m_db->insertTvMediaSeasonPoster (id, backdropPath, mediaFanart, -100);
//  m_db->insertTvMediaSeasonPoster (id, collectionPosterPath,   mediaPosterCollection, collectionId * -1);
//  m_db->insertTvMediaSeasonPoster (id, collectionBackdropPath, mediaFanartCollection, collectionId * -1);
  return true;
}

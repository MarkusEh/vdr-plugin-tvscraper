#include <string>
#include "moviedbmovie.h"

cMovieDbMovie::cMovieDbMovie(cTVScraperDB *db, cMovieDBScraper *movieDBScraper):
  m_db(db),
  m_movieDBScraper(movieDBScraper) { }

bool cMovieDbMovie::ReadAndStore(int id) {
  const char *lang = config.GetDefaultLanguage()->m_themoviedb;
  cToSvConcat url(m_baseURL, "/movie/", id, "?api_key=", m_movieDBScraper->GetApiKey(), "&language=", lang, "&append_to_response=credits,alternative_titles,translations");
  cJsonDocumentFromUrl document(m_movieDBScraper->m_curl);
  document.set_enableDebug(config.enableDebug);
  if (!document.download_and_parse(url.c_str())) return false;
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

// alternative_titles =======================
  cSql sql_titles(m_db, "INSERT OR REPLACE INTO alternative_titles (external_database, id, alternative_title, iso_3166_1) VALUES (?, ?, ?, ?);");
  rapidjson::Value::ConstMemberIterator alternative_titles_it = document.FindMember("alternative_titles");
  if (alternative_titles_it != document.MemberEnd() && alternative_titles_it->value.IsObject() ) {
    for (const rapidjson::Value &title: cJsonArrayIterator(alternative_titles_it->value, "titles")) {
      sql_titles.resetBindStep(1, id, getValueCharS(title, "title"), getValueCharS(title, "iso_3166_1"));
    }
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
  int collectionId = 0;
  const char *collectionName = nullptr;
  const char *collectionPosterPath = nullptr;
  const char *collectionBackdropPath = nullptr;
  rapidjson::Value::ConstMemberIterator collection_it = movie.FindMember("belongs_to_collection");
  if (collection_it != movie.MemberEnd() && collection_it->value.IsObject() ) {
    collectionId = getValueInt(collection_it->value, "id");
    collectionName = getValueCharS(collection_it->value, "name");
    if (!config.m_disable_images) {
      collectionPosterPath = getValueCharS(collection_it->value, "poster_path");
      collectionBackdropPath = getValueCharS(collection_it->value, "backdrop_path");
    }
  }
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

// languages
  cToSvConcat l_languages;
  bool first = true;
  rapidjson::Value::ConstMemberIterator translations_it = movie.FindMember("translations");
  if (translations_it != movie.MemberEnd() && translations_it->value.IsObject() ) {
    for (const rapidjson::Value &translation: cJsonArrayIterator(translations_it->value, "translations")) {
      if(first) {
        l_languages.append("|");
        first = false;
      }
      l_languages.append(getValueCharS(translation, "iso_639_1"));
      l_languages.append("-");
      l_languages.append(getValueCharS(translation, "iso_3166_1"));
      l_languages.append("|");
    }
  }
// backdropPath, posterPath
  const char *backdropPath = config.m_disable_images?nullptr:getValueCharS(movie, "backdrop_path");
  const char *posterPath = config.m_disable_images?nullptr:getValueCharS(movie, "poster_path");
// store in db
  const char *poster = (posterPath && *posterPath    )?posterPath:collectionPosterPath;
  const char *fanart = (backdropPath && *backdropPath)?backdropPath:collectionBackdropPath;
  m_db->InsertMovie(id, title, originalTitle, tagline, overview, adult, collectionId, collectionName, budget, revenue, genres.c_str(), homepage, releaseDate, runtime, popularity, voteAverage, voteCount, productionCountries.c_str(), poster, fanart, imdb_id, l_languages.c_str() );

// store media in db, for later download
  cSql stmt(m_db, "INSERT OR REPLACE INTO tv_media (tv_id, media_path, media_type, media_number) VALUES (?, ?, ?, ?);");
  if (posterPath && *posterPath)      stmt.resetBindStep(id, posterPath,   mediaPoster, -100);
  if (backdropPath && * backdropPath) stmt.resetBindStep(id, backdropPath, mediaFanart, -100);
  if (collectionPosterPath && *collectionPosterPath)     stmt.resetBindStep(id, collectionPosterPath,   mediaPosterCollection, collectionId * -1);
  if (collectionBackdropPath && *collectionBackdropPath) stmt.resetBindStep(id, collectionBackdropPath, mediaFanartCollection, collectionId * -1);
  return true;
}

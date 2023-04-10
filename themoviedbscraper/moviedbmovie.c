#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include "moviedbmovie.h"

using namespace std;

cMovieDbMovie::cMovieDbMovie(cTVScraperDB *db, cMovieDBScraper *movieDBScraper):
  m_db(db),
  m_movieDBScraper(movieDBScraper),
  m_buffer("cMovieDbMovie::ReadMovie", 10000)
{
    id = 0;
    adult = false;
    collectionId = 0;
    budget = 0;
    revenue = 0;
    genres = "";
    runtime = 0;
    popularity = 0.0;
    voteAverage = 0.0;
    voteCount = 0;
}

cMovieDbMovie::~cMovieDbMovie() {
}

bool cMovieDbMovie::ReadMovie(void) {
  const char *lang = config.GetDefaultLanguage()->m_themoviedb;
  std::string url = concatenate(m_baseURL, "/movie/", id, "?api_key=", m_movieDBScraper->GetApiKey(), "&language=", lang);
  if (!jsonCallRest(m_document, m_buffer, url.c_str(), config.enableDebug)) return false;
  return ReadMovie(m_document);
}
bool cMovieDbMovie::ReadMovie(const rapidjson::Value &movie) {
  title = getValueCharS(movie, "title");
  originalTitle = getValueCharS(movie, "original_title");
  tagline = getValueCharS(movie, "tagline");
  overview = getValueCharS(movie, "overview");
  adult = getValueBool(movie, "adult");
// collection
  rapidjson::Value::ConstMemberIterator collection_it = movie.FindMember("belongs_to_collection");
  if (collection_it != movie.MemberEnd() && collection_it->value.IsObject() ) {
    collectionId = getValueInt(collection_it->value, "id");
    collectionName = getValueCharS(collection_it->value, "name");
    collectionPosterPath = getValueCharS(collection_it->value, "poster_path");
    collectionBackdropPath = getValueCharS(collection_it->value, "backdrop_path");
  }
// budget, revenue
  budget  = getValueInt(movie, "budget");
  revenue  = getValueInt(movie, "revenue");
// genres, productionCountries, imdb_id
  genres = getValueArrayConcatenated(movie, "genres", "name");
  productionCountries = getValueArrayConcatenated(movie, "production_countries", "name");
  imdb_id = getValueCharS(movie, "imdb_id");
// homepage, releaseDate, runtime
  homepage = getValueCharS(movie, "homepage");
  releaseDate = getValueCharS(movie, "release_date");
  runtime = getValueInt(movie, "runtime");
// popularity, voteAverage
  popularity = getValueDouble(movie, "popularity");
  voteAverage = getValueDouble(movie, "vote_average");
  voteCount = getValueInt(movie, "vote_count");

// backdropPath, posterPath
  backdropPath = getValueCharS(movie, "backdrop_path");
  posterPath = getValueCharS(movie, "poster_path");
  return true;
}

void cMovieDbMovie::StoreMedia() {
  m_db->insertTvMediaSeasonPoster (id, posterPath,   mediaPoster, -100);
  m_db->insertTvMediaSeasonPoster (id, backdropPath, mediaFanart, -100);
  m_db->insertTvMediaSeasonPoster (id, collectionPosterPath,   mediaPosterCollection, collectionId * -1);
  m_db->insertTvMediaSeasonPoster (id, collectionBackdropPath, mediaFanartCollection, collectionId * -1);
}

void cMovieDbMovie::StoreDB(void) {
    const char *poster;
    const char *fanart;
    if (posterPath && *posterPath    ) poster = posterPath;   else poster = collectionPosterPath;
    if (backdropPath && *backdropPath) fanart = backdropPath; else fanart = collectionBackdropPath;
    m_db->InsertMovie(id, title, originalTitle, tagline, overview, adult, collectionId, collectionName, budget, revenue, genres.c_str(), homepage, releaseDate, runtime, popularity, voteAverage, voteCount, productionCountries.c_str(), poster, fanart, imdb_id);
}

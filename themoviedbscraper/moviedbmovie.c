#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <jansson.h>
#include "moviedbmovie.h"

using namespace std;

cMovieDbMovie::cMovieDbMovie(cTVScraperDB *db, cMovieDBScraper *movieDBScraper):
  m_db(db),
  m_movieDBScraper(movieDBScraper)
{
    id = 0;
    title = "";
    originalTitle = "";
    tagline = "";
    overview = "";
    adult = false;
    collectionId = 0;
    collectionName = "";
    collectionPosterPath = "";
    collectionBackdropPath = "";
    budget = 0;
    revenue = 0;
    genres = "";
    homepage = "";
    releaseDate = "";
    runtime = 0;
    popularity = 0.0;
    voteAverage = 0.0;
    voteCount = 0;
    backdropPath = "";
    posterPath = "";
}

cMovieDbMovie::~cMovieDbMovie() {
}

bool cMovieDbMovie::ReadMovie(void) {
    cLargeString json("cMovieDbMovie::ReadMovie", 10000);
    stringstream url;
    const char *lang = config.GetDefaultLanguage()->m_themoviedb;
    url << m_baseURL << "/movie/" << id << "?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << lang;
    if (!CurlGetUrl(url.str().c_str(), json)) return false;

    json_t *movie;
    movie = json_loads(json.c_str(), 0, NULL);
    if (!movie) return false;
    bool ret = ReadMovie(movie);
    json_decref(movie);
    return ret;
}
bool cMovieDbMovie::ReadMovie(json_t *movie) {
    if(!json_is_object(movie)) return false;
    json_t *jTitle = json_object_get(movie, "title");
    if(json_is_string(jTitle)) {
        title = json_string_value(jTitle);;
    }
    json_t *jOriginalTitle = json_object_get(movie, "original_title");
    if(json_is_string(jOriginalTitle)) originalTitle = json_string_value(jOriginalTitle);
    json_t *jTagline = json_object_get(movie, "tagline");
    if(json_is_string(jTagline)) tagline = json_string_value(jTagline);
    json_t *jOverview = json_object_get(movie, "overview");
    if(json_is_string(jOverview)) {
        overview = json_string_value(jOverview);
    }
    json_t *jAdult = json_object_get(movie, "adult");
    if(json_is_boolean(jAdult)) adult = json_boolean_value(jAdult);
// collection
    json_t *jCollection = json_object_get(movie, "belongs_to_collection");
    if(json_is_object(jCollection)) {
      json_t *jCollectionId = json_object_get(jCollection, "id");
      if(json_is_integer(jCollectionId)) collectionId = json_integer_value(jCollectionId);
      json_t *jCollectionName = json_object_get(jCollection, "name");
      if(json_is_string(jCollectionName)) collectionName = json_string_value(jCollectionName);
      json_t *jCollectionPosterPath = json_object_get(jCollection, "poster_path");
      if(json_is_string(jCollectionPosterPath)) collectionPosterPath = json_string_value(jCollectionPosterPath);
      json_t *jCollectionBackdropPath = json_object_get(jCollection, "backdrop_path");
      if(json_is_string(jCollectionBackdropPath)) collectionBackdropPath = json_string_value(jCollectionBackdropPath);
    }
// budget, revenue
    json_t *jBudget = json_object_get(movie, "budget");
    if(json_is_integer(jBudget)) budget = json_integer_value(jBudget);
    json_t *jRevenue = json_object_get(movie, "revenue");
    if(json_is_integer(jRevenue)) revenue = json_integer_value(jRevenue);
// genres, productionCountries, imdb_id
    genres = json_concatenate_array(movie, "genres", "name");
    productionCountries = json_concatenate_array(movie, "production_countries", "name");
    imdb_id = json_string_value_validated(movie, "imdb_id");
// homepage, releaseDate, runtime
    json_t *jHomepage = json_object_get(movie, "homepage");
    if(json_is_string(jHomepage)) homepage = json_string_value(jHomepage);
    json_t *jReleaseDate = json_object_get(movie, "release_date");
    if(json_is_string(jReleaseDate)) releaseDate = json_string_value(jReleaseDate);
    json_t *jRuntime = json_object_get(movie, "runtime");
    if(json_is_integer(jRuntime)) runtime = json_integer_value(jRuntime);
// popularity, voteAverage
    json_t *jPopularity = json_object_get(movie, "popularity");
    if(json_is_number(jPopularity)) popularity = json_number_value(jPopularity);
    json_t *jVoteAverage = json_object_get(movie, "vote_average");
    if(json_is_number(jVoteAverage)) voteAverage = json_number_value(jVoteAverage);
    voteCount = json_integer_value_validated(movie, "vote_count");

// backdropPath, posterPath
    json_t *jBackdrop = json_object_get(movie, "backdrop_path");
    if(json_is_string(jBackdrop)) {
        backdropPath = json_string_value(jBackdrop);
    }
    json_t *jPoster = json_object_get(movie, "poster_path");
    if(json_is_string(jPoster)) {
        posterPath = json_string_value(jPoster);
    }
    return true;
}

void cMovieDbMovie::AddMovieResults(vector<searchResultTvMovie> &resultSet, std::string_view SearchString, const char *description, const vector<int> &years, const cLanguage *lang){
  std::vector<cNormedString> normedStrings;
  normedStrings.push_back(cNormedString(0, normString(SearchString)));
  string_view searchString1 = SecondPart(SearchString, ": ");
  string_view searchString2 = SecondPart(SearchString, "'s ");
  if (!searchString1.empty() ) normedStrings.push_back(cNormedString(50, normString(searchString1)));
  if (!searchString2.empty() ) normedStrings.push_back(cNormedString(50, normString(searchString2)));
  string_view searchString3, searchString4;
  bool split = splitString(SearchString, " - ", 4, searchString3, searchString4);
  if (split) {
    normedStrings.push_back(cNormedString(70, normString(searchString3)));
    normedStrings.push_back(cNormedString(70, normString(searchString4)));
  }
  size_t size0 = resultSet.size();
  AddMovieResults(resultSet, SearchString, normedStrings, description, true, years, lang);
  if (resultSet.size() > size0) return;
  if (!searchString1.empty() ) AddMovieResults(resultSet, searchString1, normedStrings, description, false, years, lang);
  if (resultSet.size() > size0) return;
  if (!searchString2.empty() ) AddMovieResults(resultSet, searchString2, normedStrings, description, false, years, lang);
  if (resultSet.size() > size0) return;
  if (!split) return;
  AddMovieResults(resultSet, searchString3, normedStrings, description, false, years, lang);
  AddMovieResults(resultSet, searchString4, normedStrings, description, false, years, lang);
}

void cMovieDbMovie::AddMovieResults(vector<searchResultTvMovie> &resultSet, std::string_view SearchString, const std::vector<cNormedString> &normedStrings, const char *description, bool setMinTextMatch, const vector<int> &years, const cLanguage *lang){
    if (SearchString.empty() ) return;
    stringstream url;
    string t = config.GetThemoviedbSearchOption();
    string SearchString_ext_rom = removeRomanNum(SearchString.data(), SearchString.length());
    url << m_baseURL << "/search/movie?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << lang->m_themoviedb << t << "&query=" << CurlEscape(SearchString_ext_rom.c_str());
    size_t num_pages = AddMovieResultsForUrl(url.str(), resultSet, normedStrings, description, setMinTextMatch);
    bool found = false;
    if (num_pages > 3 && years.size() + 1 < num_pages) {
// several pages, restrict with years
      for (const int &year: years) {
        stringstream url;
        url << m_baseURL << "/search/movie?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << lang->m_themoviedb << t << "&year=" << abs(year) << "&query=" << CurlEscape(SearchString_ext_rom.c_str());
        if (AddMovieResultsForUrl(url.str(), resultSet, normedStrings, description, setMinTextMatch) > 0) found = true;
      }
    }
    if (! found) {
// no years avilable, or not found with the available year. Check all results in all pages
      if (num_pages > 10) num_pages = 10;
      for (size_t page = 2; page <= num_pages; page ++) {
        stringstream url;
        url << m_baseURL << "/search/movie?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << lang->m_themoviedb << t << "&page=" << page << "&query=" << CurlEscape(SearchString_ext_rom.c_str());
        AddMovieResultsForUrl(url.str(), resultSet, normedStrings, description, setMinTextMatch);
      }
    }
}
int cMovieDbMovie::AddMovieResultsForUrl(const string &url, vector<searchResultTvMovie> &resultSet, const std::vector<cNormedString> &normedStrings, const char *description, bool setMinTextMatch) {
// return 0 if no results where found (calling the URL shows no results). Otherwise number of pages
// add search results from URL to resultSet
  cLargeString json("cMovieDbMovie::AddMovieResultsForUrl", 10000);
  json_error_t error;
  if (config.enableDebug) esyslog("tvscraper: calling %s", url.c_str());
  if (!CurlGetUrl(url.c_str(), json)) return 0;
  json_t *root = json_loads(json.c_str(), 0, &error);
  if (!root) return 0;
  if (json_integer_value_validated(root, "total_results") == 0) return 0;
  int num_pages = json_integer_value_validated(root, "total_pages");
  AddMovieResults(root, resultSet, normedStrings, description, setMinTextMatch);
  json_decref(root);
  return num_pages;
}
bool cMovieDbMovie::AddMovieResults(json_t *root, vector<searchResultTvMovie> &resultSet, const std::vector<cNormedString> &normedStrings, const char *description, bool setMinTextMatch) {
  if(!json_is_object(root)) return false;
  json_t *results = json_object_get(root, "results");
  if(!json_is_array(results)) return false;
  size_t numResults = json_array_size(results);
  for (size_t res = 0; res < numResults; res++) {
    json_t *result = json_array_get(results, res);
    if (!json_is_object(result)) continue;
// id of result
    int id = json_integer_value_validated(result, "id");
    if (!id) continue;
    bool alreadyInList = false;
    for (const searchResultTvMovie &sRes: resultSet ) if (sRes.id() == id) { alreadyInList = true; break; }
    if (alreadyInList) continue;
// Title & OriginalTitle
    std::string resultTitle = normString(json_string_value_validated_c(result, "title") );
    if (resultTitle.empty() ) continue;
    int distOrigTitle = minDistanceNormedStrings(1000, normedStrings, normString(json_string_value_validated_c(result, "original_title") ));
    distOrigTitle = std::min(1000, distOrigTitle + 50);  // increase distance for original title, because it's a less likely match

    searchResultTvMovie sRes(id, true, json_string_value_validated(result, "release_date") );
    sRes.setPositionInExternalResult(resultSet.size() );
    int dist = minDistanceNormedStrings(distOrigTitle, normedStrings, resultTitle);
    if (dist > 300 && description && strlen(description) > 25) {
      const char *overview = json_string_value_validated_c(result, "overview");
      if (overview && *overview) dist = std::min(sentence_distance(description, overview), dist);
    }
    if (setMinTextMatch && res == 0) dist = std::min(dist, 500); // api.themoviedb.org has some alias names, which are used in the search but not displayed. Set the text match to a "minimum" of 500 -> 0.5 , because it was found by api.themoviedb.org
    sRes.setMatchText(dist);
    sRes.setPopularity(json_number_value_validated(result, "popularity"), json_number_value_validated(result, "vote_average"), json_integer_value_validated(result, "vote_count") );
    resultSet.push_back(sRes);
  }
  return true;
}

void cMovieDbMovie::StoreMedia() {
  if (!posterPath.empty() )             m_db->insertTvMediaSeasonPoster (id, posterPath,   mediaPoster, -100);
  if (!backdropPath.empty() )           m_db->insertTvMediaSeasonPoster (id, backdropPath, mediaFanart, -100);
  if (!collectionPosterPath.empty() )   m_db->insertTvMediaSeasonPoster (id, collectionPosterPath,   mediaPosterCollection, collectionId * -1);
  if (!collectionBackdropPath.empty() ) m_db->insertTvMediaSeasonPoster (id, collectionBackdropPath, mediaFanartCollection, collectionId * -1);
}

void cMovieDbMovie::StoreDB(void) {
    string poster, fanart;
    if (!posterPath.empty()   ) poster = posterPath;   else poster = collectionPosterPath;
    if (!backdropPath.empty() ) fanart = backdropPath; else fanart = collectionBackdropPath;
    m_db->InsertMovie(id, title, originalTitle, tagline, overview, adult, collectionId, collectionName, budget, revenue, genres, homepage, releaseDate, runtime, popularity, voteAverage, voteCount, productionCountries, poster, fanart, imdb_id);
}

void cMovieDbMovie::Dump(void) {
    esyslog("tvscraper: -------------- MOVIE DUMP ---------------");
    esyslog("tvscraper: title %s", title.c_str());
    esyslog("tvscraper: originalTitle %s", originalTitle.c_str());
    esyslog("tvscraper: overview %s", overview.c_str());
    esyslog("tvscraper: backdropPath %s", backdropPath.c_str());
    esyslog("tvscraper: posterPath %s", posterPath.c_str());
}

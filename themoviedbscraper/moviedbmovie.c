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
    string json;
    stringstream url;
    url << m_baseURL << "/movie/" << id << "?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << m_movieDBScraper->GetLanguage().c_str();
    if (!CurlGetUrl(url.str().c_str(), &json)) return false;

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
// genres
    json_t *jGenres = json_object_get(movie, "genres");
    if(json_is_array(jGenres)) {
      size_t numGenres = json_array_size(jGenres);
      for (size_t iGenre = 0; iGenre < numGenres; iGenre++) {
        json_t *jGenre = json_array_get(jGenres, iGenre);
        json_t *jGenreName = json_object_get(jGenre, "name");
        if(json_is_string(jGenreName)) {
          if(genres.empty() ) genres = json_string_value(jGenreName);
            else { genres.append("; "); genres.append(json_string_value(jGenreName)); }
          }
      }
    }
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

bool cMovieDbMovie::AddMovieResults(vector<searchResultTvMovie> &resultSet, const string &SearchString, const string &SearchString_ext, csEventOrRecording *sEventOrRecording){
    string json;
    stringstream url;
    bool ret;
    vector<int> years;
    AddYears(years, SearchString.c_str() );
    sEventOrRecording->AddYears(years);
    string t = config.GetThemoviedbSearchOption();

    url << m_baseURL << "/search/movie?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << m_movieDBScraper->GetLanguage().c_str() << t << "&query=" << CurlEscape(SearchString_ext.c_str());
    if (config.enableDebug) esyslog("tvscraper: calling %s", url.str().c_str());
    if (!CurlGetUrl(url.str().c_str(), &json)) return false;

    json_error_t error;
    json_t *root = json_loads(json.c_str(), 0, &error);
    if (!root) return false;
    int num_pages = json_integer_value_validated(root, "total_pages");
    if (num_pages > 1 && years.size() > 0) {
// several pages, restrict with years
      bool found = false;
      for (int &year: years) if (year > 0) {
        stringstream url;
        url << m_baseURL << "/search/movie?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << m_movieDBScraper->GetLanguage().c_str() << t << "&year=" << abs(year) << "&query=" << CurlEscape(SearchString_ext.c_str());
        if (config.enableDebug) esyslog("tvscraper: calling %s", url.str().c_str());
        if (!CurlGetUrl(url.str().c_str(), &json)) continue;
        json_t *root = json_loads(json.c_str(), 0, &error);
        if (!root) continue;
        if (json_integer_value_validated(root, "total_results") > 0) found = true;
        ret = AddMovieResults(root, resultSet, SearchString);
        json_decref(root);
      }
      if (!found) ret = AddMovieResults(root, resultSet, SearchString);
    } else {
// only one page (or no years avilable), check all results in this page
      ret = AddMovieResults(root, resultSet, SearchString);
    }
    json_decref(root);
    return ret;
}
bool cMovieDbMovie::AddMovieResults(json_t *root, vector<searchResultTvMovie> &resultSet, const string &SearchString){
    if(!json_is_object(root)) return false;
    json_t *results = json_object_get(root, "results");
    if(!json_is_array(results)) return false;
    size_t numResults = json_array_size(results);
    for (size_t res = 0; res < numResults; res++) {
        json_t *result = json_array_get(results, res);
        if (!json_is_object(result)) continue;
        string resultTitle = json_string_value_validated(result, "title");
        string resultOriginalTitle = json_string_value_validated(result, "original_title");
        if (resultTitle.empty() ) continue;
//convert result title to lower case
        transform(resultTitle.begin(), resultTitle.end(), resultTitle.begin(), ::tolower);
        transform(resultOriginalTitle.begin(), resultOriginalTitle.end(), resultOriginalTitle.begin(), ::tolower);
        int id = json_integer_value_validated(result, "id");
        if (!id) continue;
        for (const searchResultTvMovie &sRes: resultSet ) if (sRes.id() == id) continue;

        searchResultTvMovie sRes(id, true, json_string_value_validated(result, "release_date") );
        sRes.setPositionInExternalResult(resultSet.size() );
        sRes.setMatchText(min(sentence_distance(resultTitle, SearchString), sentence_distance(resultOriginalTitle, SearchString) ) );
        sRes.setPopularity(json_number_value_validated(result, "popularity"), json_number_value_validated(result, "vote_average"), json_integer_value_validated(result, "vote_count") );
        resultSet.push_back(sRes);
    }
    return true;
}

void cMovieDbMovie::StoreMedia() {
  if (!posterPath.empty() )             m_db->insertTvMediaSeasonPoster (id, posterPath,   1, -100);
  if (!backdropPath.empty() )           m_db->insertTvMediaSeasonPoster (id, backdropPath, 2, -100);
  if (!collectionPosterPath.empty() )   m_db->insertTvMediaSeasonPoster (id, collectionPosterPath, -1, collectionId * -1);
  if (!collectionBackdropPath.empty() ) m_db->insertTvMediaSeasonPoster (id, collectionBackdropPath, -2, collectionId * -1);
}

void cMovieDbMovie::StoreDB(void) {
    m_db->InsertMovie(id, title, originalTitle, tagline, overview, adult, collectionId, collectionName, budget, revenue, genres, homepage, releaseDate, runtime, popularity, voteAverage, voteCount);
}

void cMovieDbMovie::Dump(void) {
    esyslog("tvscraper: -------------- MOVIE DUMP ---------------");
    esyslog("tvscraper: title %s", title.c_str());
    esyslog("tvscraper: originalTitle %s", originalTitle.c_str());
    esyslog("tvscraper: overview %s", overview.c_str());
    esyslog("tvscraper: backdropPath %s", backdropPath.c_str());
    esyslog("tvscraper: posterPath %s", posterPath.c_str());
}

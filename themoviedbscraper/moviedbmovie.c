#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <jansson.h>
#include "moviedbmovie.h"

using namespace std;

cMovieDbMovie::cMovieDbMovie(cTVScraperDB *db, cMovieDBScraper *movieDBScraper, const cEvent *event, const cRecording *recording):
  m_db(db),
  m_movieDBScraper(movieDBScraper),
  m_event(event),
  m_recording(recording)
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

bool cMovieDbMovie::AddMovieResults(vector<searchResultTvMovie> &resultSet, const string &SearchString){
    string json;
    stringstream url;
    bool ret;
    vector<int> years;
    AddYears(years, SearchString.c_str() );
    AddYears(years, m_event->Title() );
    AddYears(years, m_event->ShortText() );
    AddYears(years, m_event->Description() );

    url << m_baseURL << "/search/movie?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << m_movieDBScraper->GetLanguage().c_str() << "&query=" << CurlEscape(SearchString.c_str());
    if (config.enableDebug) esyslog("tvscraper: calling %s", url.str().c_str());
    if (!CurlGetUrl(url.str().c_str(), &json)) return false;

    json_error_t error;
    json_t *root = json_loads(json.c_str(), 0, &error);
    if (!root) return false;
    int num_pages = json_integer_value_validated(root, "total_pages");
    if (num_pages > 1 && years.size() > 0) {
// several pages, restrict with years
      bool found = false;
      for (int &year: years) {
        stringstream url;
        url << m_baseURL << "/search/movie?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << m_movieDBScraper->GetLanguage().c_str() << "&year=" << year << "&query=" << CurlEscape(SearchString.c_str());
        if (config.enableDebug) esyslog("tvscraper: calling %s", url.str().c_str());
        if (!CurlGetUrl(url.str().c_str(), &json)) continue;
        json_t *root = json_loads(json.c_str(), 0, &error);
        if (!root) continue;
        if (json_integer_value_validated(root, "total_results") > 0) found = true;
        ret = AddMovieResults(root, resultSet, SearchString, years);
        json_decref(root);
      }
      if (!found) ret = AddMovieResults(root, resultSet, SearchString, years);
    } else {
// only one page (or no years avilable), check all results in this page
      ret = AddMovieResults(root, resultSet, SearchString, years);
    }
    json_decref(root);
    return ret;
}
bool cMovieDbMovie::AddMovieResults(json_t *root, vector<searchResultTvMovie> &resultSet, const string &SearchString, const vector<int> &years){
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
// can we match the year?
        bool matchYear = false;
        int resultReleaseYear = 0;
        string resultReleaseDate = json_string_value_validated(result, "release_date");
        if(resultReleaseDate.length() >= 4) {
          resultReleaseYear = atoi(resultReleaseDate.substr(0, 4).c_str() );
          matchYear = find(years.begin(), years.end(), resultReleaseYear) != years.end();
        }
        searchResultTvMovie sRes;
        sRes.id = id;
        sRes.movie = true;
        sRes.distance = min(sentence_distance(resultTitle, SearchString), sentence_distance(resultOriginalTitle, SearchString) );
        sRes.popularity = json_number_value_validated(result, "popularity");

        if (matchYear) sRes.year = resultReleaseYear;
                  else sRes.year = 0;

        if(matchYear) sRes.distance -= 150;
        int durationInSec;
        if (m_recording) durationInSec = m_recording->FileName() ? m_recording->LengthInSeconds() : 0;
                    else durationInSec = m_event->Duration();
        if (durationInSec) {
          if ( durationInSec > 80*60 ) {
// event longer than 80 mins, add some points that this is a movie
            sRes.distance -= 100;
          } else sRes.distance += 152; // shorter than 80 mins, most likely not a movie
        }
        resultSet.push_back(sRes);
    }
    return true;
}

void cMovieDbMovie::StoreMedia(string posterBaseUrl, string backdropBaseUrl, string destDir) {
  DownloadFile(posterBaseUrl,   posterPath, destDir, id, "_poster.jpg");
  DownloadFile(backdropBaseUrl, backdropPath, destDir, id, "_backdrop.jpg");
  string destDirCollections = destDir +  "collections/";
  CreateDirectory(destDirCollections);
  DownloadFile(posterBaseUrl, collectionPosterPath, destDirCollections, collectionId, "_poster.jpg");
  DownloadFile(backdropBaseUrl, collectionBackdropPath, destDirCollections, collectionId, "_backdrop.jpg");
}

bool cMovieDbMovie::DownloadFile(const string &urlBase, const string &urlFileName, const string &destDir, int destID, const char * destFileName) {
// download urlBase urlFileName to destDir destID destFileName
    if(urlFileName.empty() ) return false;
    stringstream destFullPath;
    destFullPath << destDir << destID << destFileName;
    stringstream urlFull;
    urlFull << urlBase << urlFileName;
    return Download(urlFull.str(), destFullPath.str());
}
void cMovieDbMovie::StoreDB(void) {
    m_db->InsertMovie(id, title, originalTitle, tagline, overview, adult, collectionId, collectionName, budget, revenue, genres, homepage, releaseDate, runtime, popularity, voteAverage);
}

void cMovieDbMovie::Dump(void) {
    esyslog("tvscraper: -------------- MOVIE DUMP ---------------");
    esyslog("tvscraper: title %s", title.c_str());
    esyslog("tvscraper: originalTitle %s", originalTitle.c_str());
    esyslog("tvscraper: overview %s", overview.c_str());
    esyslog("tvscraper: backdropPath %s", backdropPath.c_str());
    esyslog("tvscraper: posterPath %s", posterPath.c_str());
}

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <jansson.h>
#include "moviedbtv.h"
#include "themoviedbscraper.h"
#include "../tools/searchResultTvMovie.h"


using namespace std;

cMovieDbTv::cMovieDbTv(cTVScraperDB *db, cMovieDBScraper *movieDBScraper):
  m_db(db),
  m_movieDBScraper(movieDBScraper)
{
}

cMovieDbTv::~cMovieDbTv() {
}
bool cMovieDbTv::UpdateDb(){
// read tv data from themoviedb, update this object
// update db, including list of episodes
   int numberOfEpisodes = 0;
   int lastSeason = 0;
   bool exits_in_db = m_db->TvGetNumberOfEpisodes(m_tvID, lastSeason, numberOfEpisodes);
   if(!ReadTv(exits_in_db)) return false;
   if(!exits_in_db || m_tvNumberOfEpisodes > numberOfEpisodes) {
//   if this is new (not yet in db), always search seasons. "number_of_episodes" is sometimes wrong :(
//   there exist new episodes, update db with new episodes
     for(m_seasonNumber = lastSeason; m_seasonNumber <= m_tvNumberOfSeasons; m_seasonNumber++) AddOneSeason();
   }
   m_db->TvSetNumberOfEpisodes(m_tvID, m_tvNumberOfSeasons, m_tvNumberOfEpisodes);
   m_db->TvSetEpisodesUpdated(m_tvID);
   return true;
}

bool cMovieDbTv::ReadTv(bool exits_in_db) {
// call themoviedb api, get data
    string json;
    stringstream url;
    url << m_baseURL << "/tv/" << m_tvID << "?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << m_movieDBScraper->GetLanguage().c_str() << "&include_image_language=en,null";
    if(!exits_in_db) url << "&append_to_response=credits";
    if (!CurlGetUrl(url.str().c_str(), &json)) return false;
// parse json response
    json_t *tv;
    tv = json_loads(json.c_str(), 0, NULL);
    if (!tv) return false;
    bool ret = ReadTv(tv);
    if(ret) {
      if(!exits_in_db) {
// no database entry for tvID, create the db entry
        m_db->InsertTv(m_tvID, m_tvName, m_tvOriginalName, m_tvOverview, m_first_air_date, m_networks, m_genres, m_popularity, m_vote_average, m_status, m_episodeRunTimes);
        json_t *jCredits = json_object_get(tv, "credits");
        AddActorsTv(jCredits);
      }
    }
    json_decref(tv);
    return ret;
}
bool cMovieDbTv::ReadTv(json_t *tv) {
    if(!json_is_object(tv)) return false;
    m_tvName = json_string_value_validated(tv, "name");
    m_tvOriginalName = json_string_value_validated(tv, "original_name");
    m_tvOverview = json_string_value_validated(tv, "overview");
    m_first_air_date = json_string_value_validated(tv, "first_air_date");
    m_networks = json_concatenate_array(tv, "networks", "name");
    m_genres = json_concatenate_array(tv, "genres", "name");
    m_popularity = json_number_value_validated(tv, "popularity");
    m_vote_average = json_number_value_validated(tv, "vote_average");
    m_status = json_string_value_validated(tv, "status");
    m_tvBackdropPath = json_string_value_validated(tv, "backdrop_path");
    m_tvPosterPath = json_string_value_validated(tv, "poster_path");
    m_tvNumberOfSeasons = json_integer_value_validated(tv, "number_of_seasons");
    m_tvNumberOfEpisodes = json_integer_value_validated(tv, "number_of_episodes");
// episode run time
    json_t *jArray = json_object_get(tv, "episode_run_time");
    if(json_is_array(jArray)) {
      size_t numElements = json_array_size(jArray);
      for (size_t i = 0; i < numElements; i++) {
        json_t *jElement = json_array_get(jArray, i);
        if(json_is_integer(jElement)) m_episodeRunTimes.push_back(json_integer_value(jElement) );
      }
    }

// download poster & backdrop
    StoreMedia();
    return true;
}

bool cMovieDbTv::AddTvResults(vector<searchResultTvMovie> &resultSet, const string &tvSearchString, const string &tvSearchString_ext) {
// search for tv series, add search results to resultSet

// call api, get json
    string json;
    stringstream url;
    url << m_baseURL << "/search/tv?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << m_movieDBScraper->GetLanguage().c_str() << "&query=" << CurlEscape(tvSearchString_ext.c_str());
    if (config.enableDebug) esyslog("tvscraper: calling %s", url.str().c_str());
    if (!CurlGetUrl(url.str().c_str(), &json)) return false;
// parse json
    json_t *root;
    root = json_loads(json.c_str(), 0, NULL);
    if (!root) return false;
    bool ret = AddTvResults(root, resultSet, tvSearchString);
    json_decref(root);
    return ret;
}
bool cMovieDbTv::AddTvResults(json_t *root, vector<searchResultTvMovie> &resultSet, const string &tvSearchString) {
    if(!json_is_object(root)) return false;
// get results
    json_t *results = json_object_get(root, "results");
    if(!json_is_array(results)) return false;
    size_t numResults = json_array_size(results);
    for (size_t res = 0; res < numResults; res++) {
        json_t *result = json_array_get(results, res);
        if (!json_is_object(result)) continue;
        string resultName = json_string_value_validated(result, "name");
        string resultOriginalName = json_string_value_validated(result, "original_name");
        if (resultName.empty() ) continue;
//convert result to lower case
        transform(resultName.begin(), resultName.end(), resultName.begin(), ::tolower);
        transform(resultOriginalName.begin(), resultOriginalName.end(), resultOriginalName.begin(), ::tolower);
        json_t *jId = json_object_get(result, "id");
        if (json_is_integer(jId)) {
            int id = (int)json_integer_value(jId);
            for (const searchResultTvMovie &sRes: resultSet ) if (sRes.id == id) continue;
            searchResultTvMovie sRes;
            sRes.id = id;
            sRes.movie = false;
            sRes.positionInExternalResult = resultSet.size();
            sRes.year = 0;
            sRes.distance = min(sentence_distance(resultName, tvSearchString), sentence_distance(resultOriginalName, tvSearchString) );
            sRes.distance += 1;  // avaid this, prefer TVDB
            resultSet.push_back(sRes);
        }
    }
  return true;
}

bool cMovieDbTv::StoreSeasonPoster(const string &SeasonPosterPath) {
  if (SeasonPosterPath.empty() ) return false;
  string seasonPosterUrl = m_movieDBScraper->GetPosterBaseUrl() + SeasonPosterPath;
  stringstream pathPoster;
  pathPoster << m_movieDBScraper->GetTvBaseDir() << m_tvID;
  CreateDirectory(pathPoster.str() );
  pathPoster << "/" << m_seasonNumber;
  CreateDirectory(pathPoster.str() );
  pathPoster << "/poster.jpg";
  return Download(seasonPosterUrl, pathPoster.str());
}

void cMovieDbTv::StoreMedia(void) {
    if(!m_tvPosterPath.empty()){
      stringstream pathPoster;
      pathPoster << m_movieDBScraper->GetTvBaseDir() << m_tvID;
      CreateDirectory(pathPoster.str() );
      pathPoster << "/poster.jpg";
      stringstream posterUrl;
      posterUrl << m_movieDBScraper->GetPosterBaseUrl() << m_tvPosterPath;
      Download(posterUrl.str(), pathPoster.str());
    }
    if(!m_tvBackdropPath.empty()){
      stringstream pathBackdrop;
      pathBackdrop << m_movieDBScraper->GetTvBaseDir() << m_tvID;
      CreateDirectory(pathBackdrop.str() );
      pathBackdrop << "/backdrop.jpg";
      stringstream backdropUrl;
      backdropUrl << m_movieDBScraper->GetBackdropBaseUrl() << m_tvBackdropPath;
      Download(backdropUrl.str(), pathBackdrop.str());
    }
}

void cMovieDbTv::Dump(void) {
    esyslog("tvscraper: -------------- TV DUMP ---------------");
    esyslog("tvscraper: name %s", m_tvName.c_str());
    esyslog("tvscraper: originalName %s", m_tvOriginalName.c_str());
    esyslog("tvscraper: overview %s", m_tvOverview.c_str());
    esyslog("tvscraper: backdropPath %s", m_tvBackdropPath.c_str());
    esyslog("tvscraper: posterPath %s", m_tvPosterPath.c_str());
}
bool cMovieDbTv::AddOneSeason() {
// call api, get json
    string json;
    stringstream url;
    url << m_baseURL << "/tv/" << m_tvID << "/season/" << m_seasonNumber << "?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << m_movieDBScraper->GetLanguage().c_str();
    if (!CurlGetUrl(url.str().c_str(), &json)) return false;
// parse json
    json_t *root;
    root = json_loads(json.c_str(), 0, NULL);
    if (!root) return false;
    bool ret = AddOneSeason(root);
    json_decref(root);
    return ret;
}
bool cMovieDbTv::AddOneSeason(json_t *root) {
    if(!json_is_object(root)) return false;
// posterPath
    json_t *jPosterPath = json_object_get(root, "poster_path");
    if (json_is_string(jPosterPath)) {
      StoreSeasonPoster(json_string_value(jPosterPath));
    }
// episodes
    json_t *episodes = json_object_get(root, "episodes");
    if(!json_is_array(episodes)) return false;
    size_t numEpisodes = json_array_size(episodes);
    for (size_t iEpisode = 0; iEpisode < numEpisodes; iEpisode++) {
        json_t *episode = json_array_get(episodes, iEpisode);
        if (!json_is_object(episode)) return false;
// episode number
        json_t *jEpisodeNumber = json_object_get(episode, "episode_number");
        if (!json_is_integer(jEpisodeNumber)) return false;
        m_episodeNumber = (int)json_integer_value(jEpisodeNumber);
// episode id
        int id = -1;
        json_t *jId = json_object_get(episode, "id");
        if (json_is_integer(jId)) id = (int)json_integer_value(jId);
// episode name
        json_t *jEpisodeName = json_object_get(episode, "name");
        if (!json_is_string(jEpisodeName)) return false;
        string episodeName = json_string_value(jEpisodeName);

        string airDate = json_string_value_validated(episode, "air_date");
        float vote_average = json_number_value_validated(episode, "vote_average");
// episode overview
        string overview = json_string_value_validated(episode, "overview");
// stillPath
        string episodeStillPath = json_string_value_validated(episode, "still_path");
// save in db
        m_db->InsertTv_s_e(m_tvID, m_seasonNumber, m_episodeNumber, id, episodeName, airDate, vote_average, overview, "", episodeStillPath);
//  add actors
        AddActors(episode, id);
    }
    return true;
}
bool cMovieDbTv::AddActorsTv(json_t *jCredits) {
  if(!json_is_object(jCredits)) return false;
// cast
    json_t *jCast = json_object_get(jCredits, "cast");
    if(!json_is_array(jCast)) return false;
    size_t numCast = json_array_size(jCast);
    for (size_t iCast = 0; iCast < numCast; iCast++) {
        json_t *jStar = json_array_get(jCast, iCast);
        if (!json_is_object(jStar)) return false;

        json_t *jId = json_object_get(jStar, "id");
        json_t *jName = json_object_get(jStar, "name");
        json_t *jRole = json_object_get(jStar, "character");
        if (!json_is_integer(jId) || !json_is_string(jName) || !json_is_string(jRole) ) continue;
        int actor_id = json_integer_value(jId);
        string actor_name = json_string_value(jName);
        string actor_role = json_string_value(jRole);
// save in db
        m_db->InsertTvActor(m_tvID, actor_id, actor_name, actor_role);
// download actor
        json_t *jPath = json_object_get(jStar, "profile_path");
        if(json_is_string(jPath)) {
          string actor_path = json_string_value(jPath);
          if(!actor_path.empty()){
            stringstream fullPath;
            fullPath << m_movieDBScraper->GetActorsBaseDir();
            CreateDirectory(fullPath.str());
            fullPath << "/actor_" << actor_id << ".jpg";
            string path = fullPath.str();
            stringstream strUrl;
            strUrl << m_movieDBScraper->GetActorsBaseUrl() << actor_path;
            Download(strUrl.str(), path);
          }
        }
    }
    return true;
}
bool cMovieDbTv::AddActors() {
// add actors of current episode
// call api, get json
    string json;
    stringstream url;
    url << m_baseURL << "/tv/" << m_tvID << "/season/" << m_seasonNumber << "/episode/" << m_episodeNumber << "?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << m_movieDBScraper->GetLanguage().c_str();
    if (!CurlGetUrl(url.str().c_str(), &json)) return false;
// parse json
    json_t *root;
    root = json_loads(json.c_str(), 0, NULL);
    if (!root) return false;
// episode id
    int id = -1;
    json_t *jId = json_object_get(root, "id");
    if (json_is_integer(jId)) id = (int)json_integer_value(jId);
    bool ret = AddActors(root, id);
    json_decref(root);
    return ret;
}

bool cMovieDbTv::AddActors(json_t *root, int episode_id) {
    if(!json_is_object(root)) return false;
// guest_stars
    json_t *jGuestStars = json_object_get(root, "guest_stars");
    if(!json_is_array(jGuestStars)) return false;
    size_t numGuestStars = json_array_size(jGuestStars);
    for (size_t iGuestStars = 0; iGuestStars < numGuestStars; iGuestStars++) {
        json_t *jGuestStar = json_array_get(jGuestStars, iGuestStars);
        if (!json_is_object(jGuestStar)) return false;

        json_t *jId = json_object_get(jGuestStar, "id");
        json_t *jName = json_object_get(jGuestStar, "name");
        json_t *jRole = json_object_get(jGuestStar, "character");
        if (!json_is_integer(jId) || !json_is_string(jName) || !json_is_string(jRole) ) continue;
        int actor_id = json_integer_value(jId);
        string actor_name = json_string_value(jName);
        string actor_role = json_string_value(jRole);
// save in db
        m_db->InsertTvEpisodeActor(episode_id, actor_id, actor_name, actor_role);
// download actor
        json_t *jPath = json_object_get(jGuestStar, "profile_path");
        if(json_is_string(jPath)) {
          string actor_path = json_string_value(jPath);
          if(!actor_path.empty()){
            stringstream fullPath;
            fullPath << m_movieDBScraper->GetActorsBaseDir();
            CreateDirectory(fullPath.str());
            fullPath << "/actor_" << actor_id << ".jpg";
            string path = fullPath.str();
            stringstream strUrl;
            strUrl << m_movieDBScraper->GetActorsBaseUrl() << actor_path;
            Download(strUrl.str(), path);
          }
        }
    }
    return 0;
}

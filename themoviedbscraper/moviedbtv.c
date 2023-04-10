#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include "moviedbtv.h"
#include "themoviedbscraper.h"


using namespace std;

cMovieDbTv::cMovieDbTv(cTVScraperDB *db, cMovieDBScraper *movieDBScraper):
  m_db(db),
  m_movieDBScraper(movieDBScraper)
{
}

cMovieDbTv::~cMovieDbTv() {
}
bool cMovieDbTv::UpdateDb(bool updateEpisodes){
// read tv data from themoviedb, update this object
// only update if not yet in db. In this case, also episodes will be updated
// if updateEpisodes == true: force always update of episodes
  int numberOfEpisodes = 0;
  int lastSeason = 0;
  bool exits_in_db = m_db->TvGetNumberOfEpisodes(m_tvID, lastSeason, numberOfEpisodes);
  if (exits_in_db && !updateEpisodes) return true;
  cLargeString buffer("cMovieDbTv::UpdateDb", 5000);
  if(!ReadTv(exits_in_db, buffer)) return false;
  if(!exits_in_db || m_tvNumberOfEpisodes > numberOfEpisodes) {
//   if this is new (not yet in db), always search seasons. "number_of_episodes" is sometimes wrong :(
//   there exist new episodes, update db with new episodes
     for(m_seasonNumber = lastSeason; m_seasonNumber <= m_tvNumberOfSeasons; m_seasonNumber++) AddOneSeason(buffer);
  }
  m_db->TvSetNumberOfEpisodes(m_tvID, m_tvNumberOfSeasons, m_tvNumberOfEpisodes);
  m_db->TvSetEpisodesUpdated(m_tvID);
  return true;
}

bool cMovieDbTv::ReadTv(bool exits_in_db, cLargeString &buffer) {
// call themoviedb api, get data
  const char *lang = config.GetDefaultLanguage()->m_themoviedb;
  std::string url = concatenate(m_baseURL, "/tv/", m_tvID, "?api_key=", m_movieDBScraper->GetApiKey(), "&language=", lang, "&include_image_language=en,null");
  if(!exits_in_db) url += "&append_to_response=credits";
  rapidjson::Document tv;
  if (!jsonCallRest(tv, buffer, url.c_str(), config.enableDebug) ) return false;
  bool ret = ReadTv(tv);
  if(ret) {
    if (m_episodeRunTimes.empty() ) m_episodeRunTimes.insert(-1); // empty episodeRunTimes results in re-reading it from external db. And there is no data on external db ...
    if(!exits_in_db) {
// no database entry for tvID, create the db entry
      m_db->InsertTv(m_tvID, m_tvName, m_tvOriginalName, m_tvOverview, m_first_air_date, m_networks.c_str(), m_genres, m_popularity, m_vote_average, m_vote_count, m_tvPosterPath, m_tvBackdropPath, nullptr, m_status, m_episodeRunTimes, m_createdBy.c_str() );
// credits
      rapidjson::Value::ConstMemberIterator jCredits_it = tv.FindMember("credits");
      if (jCredits_it != tv.MemberEnd() && jCredits_it->value.IsObject() ) AddActorsTv(jCredits_it->value);
    } else {
      m_db->InsertTvEpisodeRunTimes(m_tvID, m_episodeRunTimes);
    }
  }
  return ret;
}
bool cMovieDbTv::ReadTv(const rapidjson::Value &tv) {
  m_tvName = getValueCharS(tv, "name");
  m_tvOriginalName = getValueCharS(tv, "original_name");
  m_tvOverview = getValueCharS(tv, "overview");
  m_first_air_date = getValueCharS(tv, "first_air_date");
  m_networks = getValueArrayConcatenated(tv, "networks", "name");
  m_genres = getValueArrayConcatenated(tv, "genres", "name");
  m_popularity = getValueDouble(tv, "popularity");
  m_vote_average = getValueDouble(tv, "vote_average");
  m_vote_count = getValueInt(tv, "vote_count");
  m_status = getValueCharS(tv, "status");
  m_tvBackdropPath = getValueCharS(tv, "backdrop_path");
  m_tvPosterPath = getValueCharS(tv, "poster_path");
  m_tvNumberOfSeasons = getValueInt(tv, "number_of_seasons");
  m_tvNumberOfEpisodes = getValueInt(tv, "number_of_episodes");

  rapidjson::Value::ConstMemberIterator createdBy_it = tv.FindMember("created_by");
  if (createdBy_it != tv.MemberEnd() ) m_createdBy = GetCrewMember(createdBy_it->value, NULL, NULL);
// episode run time
  for (const rapidjson::Value &jElement: cJsonArrayIterator(tv, "episode_run_time") ) {
    if (!jElement.IsInt() ) continue;
    int rt = jElement.GetInt();
    if (rt != 0) m_episodeRunTimes.insert(rt);
  }
// download poster & backdrop
  m_db->insertTvMedia(m_tvID, m_tvPosterPath,   mediaPoster);
  m_db->insertTvMedia(m_tvID, m_tvBackdropPath, mediaFanart);
  return true;
}

bool cMovieDbTv::AddOneSeason(cLargeString &buffer) {
// call api, get json
  cConcatenate url;
  const char *lang = config.GetDefaultLanguage()->m_themoviedb;
  url << m_baseURL << "/tv/" << m_tvID << "/season/" << m_seasonNumber << "?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << lang;
  rapidjson::Document root;
  if (!jsonCallRest(root, buffer, url.str().c_str(), config.enableDebug)) return false;
// posterPath
  m_db->insertTvMediaSeasonPoster(m_tvID, getValueCharS(root, "poster_path"), mediaSeason, m_seasonNumber);
// episodes
  for (const rapidjson::Value &episode: cJsonArrayIterator(root, "episodes")) {
// episode number
    m_episodeNumber = getValueInt(episode, "episode_number");
    if (m_episodeNumber == 0) continue;
// episode id
    int id = getValueInt(episode, "id", -1);
// episode name
    const char *episodeName = getValueCharS(episode, "name");
    if (!episodeName) continue;
    const char *airDate = getValueCharS(episode, "air_date");
    float vote_average = getValueDouble(episode, "vote_average");
    int vote_count = getValueInt(episode, "vote_count");
// episode overview
    const char *overview = getValueCharS(episode, "overview");
// stillPath
    const char *episodeStillPath = getValueCharS(episode, "still_path");
    rapidjson::Value::ConstMemberIterator jCrew_it = episode.FindMember("crew");
    if (jCrew_it != episode.MemberEnd() ) 
      m_db->InsertTv_s_e(m_tvID, m_seasonNumber, m_episodeNumber, 0, id, episodeName, airDate, vote_average, vote_count, overview, "", GetCrewMember(jCrew_it->value, "job", "Director"), GetCrewMember(jCrew_it->value, "department", "Writing"), "", episodeStillPath, 0);
    else
      m_db->InsertTv_s_e(m_tvID, m_seasonNumber, m_episodeNumber, 0, id, episodeName, airDate, vote_average, vote_count, overview, "", "", "", "", episodeStillPath, 0);
//  add actors
    AddActors(episode, id);
  }
  return true;
}
std::string cMovieDbTv::GetCrewMember(const rapidjson::Value &jCrew, const char *field, const char *value) {
  if (!jCrew.IsArray() ) return "";
  cContainer members;
  if (field && value) {
    for (const rapidjson::Value &jCrewMember: jCrew.GetArray()) {
      const char *fieldValue = getValueCharS(jCrewMember, field);
      if (fieldValue && strcmp(value, fieldValue) == 0) members.insert(getValueCharS(jCrewMember, "name"));
    }
  } else {
    for (const rapidjson::Value &jCrewMember: jCrew.GetArray())
      members.insert(getValueCharS(jCrewMember, "name"));
  }
  return members.getBuffer();
}

bool cMovieDbTv::AddActorsTv(const rapidjson::Value &jCredits) {
// cast
  for (const rapidjson::Value &jStar: cJsonArrayIterator(jCredits, "cast")) {
// download actor, and save in db
    int actor_id = getValueInt(jStar, "id");
    const char *actor_name = getValueCharS(jStar, "name");
    if (!actor_id || !actor_name) continue;
    const char *actor_role = getValueCharS(jStar, "character");
    const char *actor_path = getValueCharS(jStar, "profile_path");
    if (!actor_path || !*actor_path) {
      m_db->InsertTvActor(m_tvID, actor_id, actor_name, actor_role, false);
    } else {
      m_db->InsertTvActor(m_tvID, actor_id, actor_name, actor_role, true);
      m_db->AddActorDownload (m_tvID, false, actor_id, actor_path);
    }
  }
  return true;
}

bool cMovieDbTv::AddActors(const rapidjson::Value &root, int episode_id) {
// guest_stars
  for (const rapidjson::Value &jGuestStar: cJsonArrayIterator(root, "guest_stars")) {
    int actor_id = getValueInt(jGuestStar, "id");
    const char *actor_name = getValueCharS(jGuestStar, "name");
    if (!actor_id || !actor_name) continue;
    const char *actor_role = getValueCharS(jGuestStar, "character");
    const char *actor_path = getValueCharS(jGuestStar, "profile_path");
    if (!actor_path || !*actor_path) {
      m_db->InsertTvEpisodeActor(episode_id, actor_id, actor_name, actor_role, false);
    } else {
      m_db->InsertTvEpisodeActor(episode_id, actor_id, actor_name, actor_role, true);
      m_db->AddActorDownload (m_tvID, false, actor_id, actor_path);
    }
  }
  return true;
}

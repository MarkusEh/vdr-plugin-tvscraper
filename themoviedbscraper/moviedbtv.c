#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include "moviedbtv.h"
#include "themoviedbscraper.h"
#include "moviedbactors.h"


using namespace std;

cMovieDbTv::cMovieDbTv(cTVScraperDB *db, cMovieDBScraper *movieDBScraper):
  m_db(db),
  m_movieDBScraper(movieDBScraper)
{
}

cMovieDbTv::~cMovieDbTv() {
}
bool cMovieDbTv::UpdateDb(bool forceUpdate) {
// read tv data from themoviedb, update this object
// only update if not yet in db or forceUpdate == true. In this case, also episodes will be updated
  if (m_tvID == 0) return false;
  time_t tv_last_updated = 0;
  cSql stmtLastUpdate(m_db, "SELECT tv_last_updated FROM tv2 WHERE tv_id = ?", m_tvID);
  bool exists = stmtLastUpdate.readRow(tv_last_updated);
  if (exists && !forceUpdate) return true; // already in db
  int numberOfEpisodes = 0;
  int lastSeason = 0;
  if (exists) m_db->TvGetNumberOfEpisodes(m_tvID, lastSeason, numberOfEpisodes);

  if(!ReadTv(exists)) return false;
  if(!exists || m_tvNumberOfEpisodes > numberOfEpisodes) {
//  if this is new (not yet in db), always search seasons. "number_of_episodes" is sometimes wrong :(
//  there exist new episodes, update db with new episodes
    for(m_seasonNumber = lastSeason; m_seasonNumber <= m_tvNumberOfSeasons; m_seasonNumber++) AddOneSeason();
    m_db->exec("UPDATE tv2 SET tv_last_changed = ? WHERE tv_id= ?", time(0), m_tvID);
  }
  m_db->TvSetNumberOfEpisodes(m_tvID, m_tvNumberOfSeasons, m_tvNumberOfEpisodes);
  m_db->exec("UPDATE tv2 SET tv_last_updated = ? WHERE tv_id= ?", time(0), m_tvID);
  return true;
}

bool cMovieDbTv::ReadTv(bool exits_in_db) {
// call themoviedb api, get data
  const char *lang = config.GetDefaultLanguage()->m_themoviedb;
  std::string url = concatenate(m_baseURL, "/tv/", m_tvID, "?api_key=", m_movieDBScraper->GetApiKey(), "&language=", lang, "&include_image_language=en,null");
  if(!exits_in_db) url += "&append_to_response=credits";
  cJsonDocumentFromUrl tv;
  tv.set_enableDebug(config.enableDebug);
  if (!tv.download_and_parse(url.c_str())) return false;
//  if (!jsonCallRest(tv, url.c_str(), config.enableDebug) ) return false;
  bool ret = ReadTv(tv);
  if(ret) {
    if (m_episodeRunTimes.empty() ) m_episodeRunTimes.insert(-1); // empty episodeRunTimes results in re-reading it from external db. And there is no data on external db ...
    if(!exits_in_db) {
// no database entry for tvID, create the db entry
      m_db->InsertTv(m_tvID, m_tvName, m_tvOriginalName, m_tvOverview, m_first_air_date, m_networks.c_str(), m_genres, m_popularity, m_vote_average, m_vote_count, m_tvPosterPath, m_tvBackdropPath, nullptr, m_status, m_episodeRunTimes, m_createdBy.c_str(), nullptr);
// credits
      rapidjson::Value::ConstMemberIterator jCredits_it = tv.FindMember("credits");
      if (jCredits_it != tv.MemberEnd() && jCredits_it->value.IsObject() )
        readAndStoreMovieDbActors(m_db, jCredits_it->value, m_tvID, false);
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
  m_createdBy = getValueArrayConcatenated(tv, "created_by", "name");
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

bool cMovieDbTv::AddOneSeason() {
// call api, get json
  cToSvConcat url;
  const char *lang = config.GetDefaultLanguage()->m_themoviedb;
  url << m_baseURL << "/tv/" << m_tvID << "/season/" << m_seasonNumber << "?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << lang;
  cJsonDocumentFromUrl root;
  root.set_enableDebug(config.enableDebug);
  if (!root.download_and_parse(url.c_str())) return false;
//  if (!jsonCallRest(root, url.getCharS(), config.enableDebug)) return false;
// posterPath
  m_db->insertTvMediaSeasonPoster(m_tvID, getValueCharS(root, "poster_path"), mediaSeason, m_seasonNumber);
// episodes
  cSql stmtInsertTv_s_e(m_db, "INSERT OR REPLACE INTO tv_s_e (tv_id, season_number, episode_number, episode_id, episode_name, episode_air_date, episode_vote_average, episode_vote_count, episode_overview, episode_director, episode_writer, episode_still_path, episode_run_time) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0);");
  for (const rapidjson::Value &episode: cJsonArrayIterator(root, "episodes")) {
// episode number
    m_episodeNumber = getValueInt(episode, "episode_number");
    if (m_episodeNumber == 0) continue;
// episode id / name
    int id = getValueInt(episode, "id", -1);
    const char *episodeName = getValueCharS(episode, "name");
    if (id == -1 || !episodeName) continue;
    const char *airDate = getValueCharS(episode, "air_date");
    float vote_average = getValueDouble(episode, "vote_average");
    int vote_count = getValueInt(episode, "vote_count");
// episode overview
    const char *overview = getValueCharS(episode, "overview");
// stillPath
    const char *episodeStillPath = getValueCharS(episode, "still_path");
    std::string director;
    std::string writer;
    getDirectorWriter(director, writer, episode);
    stmtInsertTv_s_e.resetBindStep(m_tvID, m_seasonNumber, m_episodeNumber, id, episodeName, airDate, vote_average, vote_count, overview, director, writer, episodeStillPath);
//  add actors
    readAndStoreMovieDbActors(m_db, episode, id, false, true);
  }
  return true;
}

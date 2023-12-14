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
    m_db->exec("INSERT OR REPLACE INTO tv_name (tv_id, language_id, tv_last_updated) VALUES (?, ?, ?)", m_tvID, config.GetDefaultLanguage()->m_id, time(0));
  }
  m_db->TvSetNumberOfEpisodes(m_tvID, m_tvNumberOfSeasons, m_tvNumberOfEpisodes);
  m_db->exec("UPDATE tv2 SET tv_last_updated = ? WHERE tv_id= ?", time(0), m_tvID);
  return true;
}

int cMovieDbTv::downloadEpisodes(bool forceUpdate, const cLanguage *lang) {
// read tv data from themoviedb, update episodes
// return codes:
//   -1 object does not exist (we already called db->DeleteSeriesCache(-seriesID))
//    0 success
//    1 no episode names in this language
//    2 invalid input data: tvID not set
//    3 invalid input data: lang
//    5 or > 5: Other error
// only update if required (new data expected)
  if (m_tvID == 0) return 2;
  if (!lang) return 3;
  if (!forceUpdate && !m_db->episodeNameUpdateRequired(m_tvID, lang->m_id)) return 0;
  if (config.GetDefaultLanguage()->m_id == lang->m_id) {
    if (UpdateDb(forceUpdate)) return 0;
    return 5;
  }
/*
 * TODO: Find available language for themoviedb, where langualge list is missing
 *  https://api.themoviedb.org/3/tv/{series_id}/alternative_titles
 *  -> these titles have also languages assigned ...
 * for now: leave this out, and download whatever is available in the requested language
*/
  if (!UpdateDb(true)) return 5;  // full update, incl. episodes, to get all data
  if (config.enableDebug) esyslog("tvscraper: cMovieDbTv::downloadEpisodes lang %s, langMovieDbId %i, tvID %i", lang->getNames().c_str(), lang->m_id, m_tvID);
  for(m_seasonNumber = 0; m_seasonNumber <= m_tvNumberOfSeasons; m_seasonNumber++) AddOneSeason(lang);
  m_db->exec("INSERT OR REPLACE INTO tv_name (tv_id, language_id, tv_last_updated) VALUES (?, ?, ?)", m_tvID, lang->m_id, time(0));
  return 0;
}

bool cMovieDbTv::ReadTv(bool exits_in_db) {
// call themoviedb api, get data
  const char *lang = config.GetDefaultLanguage()->m_themoviedb;
  cToSvConcat url(m_baseURL, "/tv/", m_tvID, "?api_key=", m_movieDBScraper->GetApiKey(), "&language=", lang, "&include_image_language=en,null&append_to_response=translations,alternative_titles");
  if(!exits_in_db) url.append(",credits");
  cJsonDocumentFromUrl tv;
  tv.set_enableDebug(config.enableDebug);
  if (!tv.download_and_parse(url.c_str() )) return false;
  bool ret = ReadTv(tv);
  if (ret) {
    if (m_episodeRunTimes.empty() ) m_episodeRunTimes.insert(-1); // empty episodeRunTimes results in re-reading it from external db. And there is no data on external db ...
    if (!exits_in_db) {
// no database entry for tvID, create the db entry
      m_db->InsertTv(m_tvID, m_tvName, m_tvOriginalName, m_tvOverview, m_first_air_date, m_networks.c_str(), m_genres, m_popularity, m_vote_average, m_vote_count, m_tvPosterPath, m_tvBackdropPath, nullptr, m_status, m_episodeRunTimes, m_createdBy.c_str(), m_languages.c_str() );
// credits
      rapidjson::Value::ConstMemberIterator jCredits_it = tv.FindMember("credits");
      if (jCredits_it != tv.MemberEnd() && jCredits_it->value.IsObject() )
        readAndStoreMovieDbActors(m_db, jCredits_it->value, m_tvID, false);
    } else {
      m_db->InsertTvEpisodeRunTimes(m_tvID, m_episodeRunTimes);
      m_db->exec("INSERT OR REPLACE INTO tv_score (tv_id, tv_score, tv_languages, tv_languages_last_update, tv_actors_last_update, tv_data_available) VALUES (?, ?, ?, ?, ?, ?)", m_tvID, m_popularity, m_languages, time(0), time(0), 4);
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
// alternative titles
  cSql sql_titles(m_db, "INSERT OR REPLACE INTO alternative_titles (external_database, id, alternative_title, iso_3166_1) VALUES (?, ?, ?, ?);");
  rapidjson::Value::ConstMemberIterator alternative_titles_it = tv.FindMember("alternative_titles");
  if (alternative_titles_it != tv.MemberEnd() && alternative_titles_it->value.IsObject() ) {
    for (const rapidjson::Value &title: cJsonArrayIterator(alternative_titles_it->value, "results")) {
      sql_titles.resetBindStep(2, m_tvID, getValueCharS(title, "title"), getValueCharS(title, "iso_3166_1"));
    }
  }
// languages
  cToSvConcat l_languages;
  bool first = true;
  rapidjson::Value::ConstMemberIterator translations_it = tv.FindMember("translations");
  if (translations_it != tv.MemberEnd() && translations_it->value.IsObject() ) {
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
  m_languages = std::string(l_languages);
// download poster & backdrop
  m_db->insertTvMedia(m_tvID, m_tvPosterPath,   mediaPoster);
  m_db->insertTvMedia(m_tvID, m_tvBackdropPath, mediaFanart);
  return true;
}

bool cMovieDbTv::AddOneSeason() {
// call api, get json
  cToSvConcat url;
  url << m_baseURL << "/tv/" << m_tvID << "/season/" << m_seasonNumber << "?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << config.GetDefaultLanguage()->m_themoviedb;
  cJsonDocumentFromUrl root;
  root.set_enableDebug(config.enableDebug);
  if (!root.download_and_parse(url.c_str())) return false;
//  if (!jsonCallRest(root, url.getCharS(), config.enableDebug)) return false;
// posterPath
  m_db->insertTvMediaSeasonPoster(m_tvID, getValueCharS(root, "poster_path"), mediaSeason, m_seasonNumber);
// episodes
  cSql stmtInsertTv_s_e(m_db, "INSERT OR REPLACE INTO tv_s_e (tv_id, season_number, episode_number, episode_id, episode_name, episode_air_date, episode_vote_average, episode_vote_count, episode_overview, episode_director, episode_writer, episode_still_path, episode_run_time) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0);");
  cSql stmtInsert_tv_s_e_name(m_db, "INSERT OR REPLACE INTO tv_s_e_name_moviedb (episode_id, language_id, episode_name) VALUES (?, ?, ?);");
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
    stmtInsert_tv_s_e_name.resetBindStep(id, config.GetDefaultLanguage()->m_id, episodeName);
//  add actors
    readAndStoreMovieDbActors(m_db, episode, id, false, true);
  }
  return true;
}
bool cMovieDbTv::AddOneSeason(const cLanguage *lang) {
// add episode names in language lang to tv_s_e_name_moviedb
// call api, get json
  cToSvConcat url;
  url << m_baseURL << "/tv/" << m_tvID << "/season/" << m_seasonNumber << "?api_key=" << m_movieDBScraper->GetApiKey() << "&language=" << lang->m_themoviedb;
  cJsonDocumentFromUrl root;
  root.set_enableDebug(config.enableDebug);
  if (!root.download_and_parse(url.c_str())) return false;
// episodes
  cSql stmtInsert_tv_s_e_name(m_db, "INSERT OR REPLACE INTO tv_s_e_name_moviedb (episode_id, language_id, episode_name) VALUES (?, ?, ?);");
  for (const rapidjson::Value &episode: cJsonArrayIterator(root, "episodes")) {
// episode id / name
    int id = getValueInt(episode, "id", -1);
    const char *episodeName = getValueCharS(episode, "name");
    if (id == -1 || !episodeName) continue;
    stmtInsert_tv_s_e_name.resetBindStep(id, lang->m_id, episodeName);
  }
  return true;
}

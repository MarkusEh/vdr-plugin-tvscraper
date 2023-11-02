#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "thetvdbscraper.h"

using namespace std;

cTVDBScraper::cTVDBScraper(cTVScraperDB *db) {
  this->db = db;
}

cTVDBScraper::~cTVDBScraper() {
}

bool cTVDBScraper::Connect(void) {
  return true;
}

bool cTVDBScraper::GetToken(void) {
// return true on success
  if (time(0) - tokenHeaderCreated < 24*60*60) return true; // call only once a day
  const char *url = "https://api4.thetvdb.com/v4/login";
  std::string buffer;

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "accept: application/json");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "charset: utf-8");
  bool result =  CurlPostUrl(url, "{\"apikey\": \"5476e702-85aa-45fd-a8da-e74df3840baf\"}", buffer, headers);
  curl_slist_free_all(headers);
  if (!result) {
    esyslog("tvscraper: ERROR cTVDBScraper::GetToken, calling %s", url);
    return false;
  }
// now read the tocken
  if (!GetToken(buffer)) {
    esyslog("tvscraper: ERROR cTVDBScraper::GetToken, url %s, parsing json result %s", url, buffer.substr(0, 50).c_str() );
    return false;
  }
  return true;
}

bool cTVDBScraper::GetToken(std::string &jsonResponse) {
  rapidjson::Document document;
  document.ParseInsitu(jsonResponse.data() );
  if (document.HasParseError() ) {
    esyslog("tvscraper: ERROR cTVDBScraper::GetToken, parse failed, error code %d", (int)document.GetParseError () );
    return false; // no data
  }
  if (!document.IsObject() ) return false;

  const char *status;
  if (!getValue(document, "status", status) || !status) {
    esyslog("tvscraper: cTVDBScraper::GetToken, tag status missing");
    return false;
  }
  if (strcmp(status, "success") != 0) {
    esyslog("tvscraper: ERROR getting thetvdb token, status = %s", status);
    return false;
  }
  rapidjson::Value::ConstMemberIterator data_it = getTag(document, "data", "cTVDBScraper::GetToken", nullptr);
  if (data_it == document.MemberEnd() || !data_it->value.IsObject() ) return false;
  const char *tocken = getValueCharS(data_it->value, "token", "cTVDBScraper::GetToken");
  if (!tocken) return false;
  tokenHeader = "Authorization: Bearer ";
  tokenHeader.append(tocken);
  tokenHeaderCreated = time(0);
  return true;
}

const cLanguage *displayLanguageTvdb(cSv translations) {
// input: translations: List of translations
// output: displayLanguage: Language used to display the data.
//                          or nullptr if not found
  cSplit transSplit(translations, '|');
// check: translation in configured default language available?
  if (transSplit.find(config.GetDefaultLanguage()->m_thetvdb) != transSplit.end()) return config.GetDefaultLanguage();
// translation in configured default language is not available
// check: translation in an additional language available?
  for (int l: config.GetAdditionalLanguages() ) {
    auto f = config.m_languages.find(l);
    if (f == config.m_languages.end() ) continue;   // ? should not happen
    if (!f->m_thetvdb || !*f->m_thetvdb) continue;  // ? should not happen
    if (config.isDefaultLanguage(&(*f)) ) continue;   // ignore default language, as this was already checked
    if (transSplit.find(f->m_thetvdb) != transSplit.end()) return &(*f);
  }
  return nullptr;
}
const cLanguage *languageTvdb(cSv tvdbLang) {
  auto lang = find_if(config.m_languages.begin(), config.m_languages.end(), [tvdbLang](const cLanguage& x) { return tvdbLang == x.m_thetvdb;});
  if (lang != config.m_languages.end() ) return &(*lang);
  return nullptr;
}

int languageTvdbInt(cSv tvdbLang) {
  const cLanguage *lang = languageTvdb(tvdbLang);
  if (lang) return lang->m_id;
  return -2; // tvdbLang not available as language in tvscraper available
}

int displayLanguageTvdbInt(cSv translations) {
// input: translations: List of translations
// output: displayLanguage: Language used to display the data.
  const cLanguage *lang = displayLanguageTvdb(translations);
  if (lang) return lang->m_id;
  return -2; // no translation is available
}

std::string displayLanguageTvdb(cSv translations, const char *originalLanguage) {
// input: translations: List of translations
// input: originalLanguage: originalLanguage of the series / tv show
// output: displayLanguage: Language used to display the data.
  const cLanguage *lang = displayLanguageTvdb(translations);
  if (lang) return lang->m_thetvdb;
// if no translation is available, use originalLanguage as last resort
  return charPointerToString(originalLanguage);
}

void getCharacters(const rapidjson::Value &characters, cTVDBSeries &series) {
  for (const rapidjson::Value &character: cJsonArrayIterator(characters, "characters") ) {
    series.ParseJson_Character(character);
  }
}

int cTVDBScraper::StoreSeriesJson(int seriesID, bool forceUpdate) {
// return 0 in case of error
// return -1 if the object does not exist in external db
// otherwise, return seriesID > 0
// only update if not yet in db
  if (seriesID == 0) return 0;
// figure out: display language
  const cLanguage *displayLanguage = config.GetDefaultLanguage();
  if (!displayLanguage || !displayLanguage->m_thetvdb) {
    esyslog("tvscraper: ERROR cTVDBScraper::StoreSeriesJson invalid displayLanguages 1, seriesID %i", seriesID);
    if (displayLanguage) displayLanguage->log();
    return 0;
  }
  cSql stmtNameLang(db, "SELECT tv_name, tv_display_language FROM tv2 WHERE tv_id = ?", -seriesID);
  if (stmtNameLang.readRow() && stmtNameLang.getInt(1) > 0) displayLanguage = config.GetLanguage(stmtNameLang.getInt(1) );
  else {
    cSql stmtLanguages(db, "SELECT tv_languages, tv_languages_last_update FROM tv_score WHERE tv_id = ?", -seriesID);
    if (stmtLanguages.readRow() && !config.isUpdateFromExternalDbRequired(stmtLanguages.getInt64(1) )) {
      const cLanguage *lang = displayLanguageTvdb(stmtLanguages.getStringView(0));
      if (lang) {
        displayLanguage = lang;
/*
        int displayLangTvdbId = config.GetLanguageThetvdb(displayLanguage)->m_id;
        db->exec("INSERT INTO tv2 (tv_id, tv_display_language) VALUES (?, ?) ON CONFLICT(tv_id) DO UPDATE SET tv_display_language=excluded.tv_display_language",
                 -seriesID, displayLangTvdbId)
*/
      }
    }
  }
  if (!displayLanguage || !displayLanguage->m_thetvdb) {
    esyslog("tvscraper: ERROR cTVDBScraper::StoreSeriesJson invalid displayLanguages 2, seriesID %i", seriesID);
    if (displayLanguage) displayLanguage->log();
    return 0;
  }
  cLargeString buffer("cTVDBScraper::StoreSeriesJson", 15000);
  const cLanguage *newDisplayLanguage = 0;
  int rde = downloadEpisodes(buffer, seriesID, forceUpdate, displayLanguage, true, &newDisplayLanguage);
  if (rde == 1) {
    if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::StoreSeriesJson displayLanguage %s not available, seriesID %i",  displayLanguage->getNames().c_str(), seriesID);
    if (!newDisplayLanguage) return 0;
    displayLanguage = newDisplayLanguage;
    rde = downloadEpisodes(buffer, seriesID, forceUpdate, displayLanguage, true);
    if (rde == 1) {
      esyslog("tvscraper: ERROR cTVDBScraper::StoreSeriesJson newDisplayLanguage %s not available, seriesID %i", displayLanguage->getNames().c_str(), seriesID);
      return 0;
    }
  }
  if (!forceUpdate && stmtNameLang.readRow() && !stmtNameLang.getStringView(0).empty() ) return seriesID; // already in db

  cTVDBSeries series(db, this, seriesID);
// this call gives also episode related information like actors, images, ...
// Change: season images type 6 ("Banner") & 7 ("Poster") still included
// Episode Guest stars, writer, ... NOT included any more
// Episode Guest stars, writer, ... NOT available in https://api4.thetvdb.com/v4/seasons/1978231/extended
//
  CONCATENATE(url, baseURL4, "series/", seriesID, "/extended?meta=translations&short=false");
  rapidjson::Document document;
  const rapidjson::Value *data;
  int error = CallRestJson(document, data, buffer, url);
  if (error != 0) {
    if (error == -1) { db->DeleteSeriesCache(-seriesID); return -1; } // object does not exist
    return 0;
  }
  series.ParseJson_Series(*data, displayLanguage);
// characters / actors
// we also add characters to episodes. Therefore, we do this after parsing the episodes
  for (const rapidjson::Value &character: cJsonArrayIterator(*data, "characters") ) {
    series.ParseJson_Character(character);
  }

// we also add season images. Therefore, we do this after parsing the episodes
  series.ParseJson_Artwork(*data);
// store series here, as here information (incl. episode runtimes, poster URL, ...) is complete
  series.StoreDB();
  return seriesID;
}

int cTVDBScraper::downloadEpisodes(cLargeString &buffer, int seriesID, bool forceUpdate, const cLanguage *lang, bool langIsIntendedDisplayLanguage, const cLanguage **displayLanguage) {
// return codes:
//   -1 object does not exist (we already called db->DeleteSeriesCache(-seriesID))
//    0 success
//    1 no episode names in this language
//    2 invalid input data: seriesID
//    3 invalid input data: lang
//    5 or > 5: Other error
// only update if required (new data expected)
// ignore tv_number_of_episodes in tv2. Just count tv_s_e
  if (seriesID <= 0) return 2;
  if (!lang) return 3;
  int langTvdbId = config.GetLanguageThetvdb(lang)->m_id;
  if (!forceUpdate && !db->episodeNameUpdateRequired(-seriesID, langTvdbId)) return 0;
// check cached information: episodes available in lang?
  cSql stmtScore(db, "SELECT tv_languages, tv_languages_last_update FROM tv_score WHERE tv_id = ?", -seriesID);
  if (stmtScore.readRow() && !config.isUpdateFromExternalDbRequired(stmtScore.getInt64(1) )) {
    cSplit transSplit(stmtScore.getCharS(0), '|');
    if (transSplit.find(lang->m_thetvdb) == transSplit.end() ) {
// this language is not available
      if (!displayLanguage)  return 1; // translation in needed language not available, available language not requested
// figure out displayLanguage
      *displayLanguage = displayLanguageTvdb(stmtScore.getCharS(0));
      if (*displayLanguage) return 1; // an additional language is available
// we have to continue to get original language ...
    }
  }
  if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::downloadEpisodes lang %s, langTvdbId %i, seriesID %i, displayLanguage %s", lang->getNames().c_str(), langTvdbId, seriesID, displayLanguage?"requested":"not requested");

  int numEpisodes = cSql(db, "SELECT COUNT(episode_id) FROM tv_s_e WHERE tv_id = ?;", -seriesID).getInt(0);
  cSql insertEpisodeLang(db, "INSERT OR REPLACE INTO tv_s_e_name2 (episode_id, language_id, episode_name) VALUES (?, ?, ?);");
  cSql insertEpisode2(db, "INSERT INTO tv_s_e (tv_id, season_number, episode_number, episode_id, episode_air_date, episode_still_path, episode_run_time) VALUES (?, ?, ?, ?, ?, ?, ?) ON CONFLICT(tv_id,season_number,episode_number) DO UPDATE SET episode_air_date=excluded.episode_air_date,episode_run_time=excluded.episode_run_time;");
  cSql insertEpisode3(db, "INSERT INTO tv_s_e (tv_id, season_number, episode_number, episode_id, episode_air_date, episode_overview, episode_still_path, episode_run_time) VALUES (?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(tv_id,season_number,episode_number) DO UPDATE SET episode_air_date=excluded.episode_air_date, episode_overview=excluded.episode_overview, episode_run_time=excluded.episode_run_time;");
  cSql insertRuntime(db, "INSERT OR REPLACE INTO tv_episode_run_time (tv_id,episode_run_time) VALUES (?, ?)");

  string urlE = concatenate(baseURL4, "series/", seriesID, "/episodes/default/", lang->m_thetvdb, "?page=0");
  bool first = true;
  bool langIsDisplayLanguage = langIsIntendedDisplayLanguage;
  do {
    rapidjson::Document episodes;
    const rapidjson::Value *data_episodes;
    int error = CallRestJson(episodes, data_episodes, buffer, urlE.c_str() );
    if (error != 0) {
      if (error == -1) { db->DeleteSeriesCache(-seriesID); return -1; } // object does not exist
      return 5;
    }
    if (first) {
// parse not episode related data
      std::string translations = getArrayConcatenated(*data_episodes, "nameTranslations");
      float popularity = getValueInt(*data_episodes, "score");
      if (popularity == 0) popularity = 1;  // 0 indicates that no score is available in internal db, and will result in access to external db.
      db->exec("INSERT INTO tv_score (tv_id, tv_score, tv_languages, tv_languages_last_update) VALUES(?, ?, ?, ?) ON CONFLICT(tv_id) DO UPDATE SET tv_score = excluded.tv_score, tv_languages = excluded.tv_languages, tv_languages_last_update = excluded.tv_languages_last_update", -seriesID, popularity, translations, time(0) );

      db->exec("INSERT INTO tv2 (tv_id, tv_original_name, tv_first_air_date, tv_popularity, tv_status) VALUES (?, ?, ?, ?, ?) ON CONFLICT(tv_id) DO UPDATE SET tv_original_name=excluded.tv_original_name, tv_first_air_date=excluded.tv_first_air_date, tv_popularity=excluded.tv_popularity, tv_status=excluded.tv_status",
        -seriesID, getValueCharS(*data_episodes, "name"), getValueCharS(*data_episodes, "firstAired"), popularity, getValueCharS2(*data_episodes, "status", "name") );

// check: translations in lang available?
      cSplit transSplit(translations, '|');
      bool translationAvailable = transSplit.find(lang->m_thetvdb) != transSplit.end();
      if (translations.empty() ) {
        const char *ol = getValueCharS(*data_episodes, "originalLanguage");
        if (ol && strcmp(ol, lang->m_thetvdb) == 0) translationAvailable = true;
        esyslog("tvscraper: ERROR cTVDBScraper::downloadEpisodes nameTranslations attribute missing, seriesID %i", seriesID);
      }
      if (!translationAvailable) {
        if (!displayLanguage) return 1;
// figure out displayLanguage
        *displayLanguage = displayLanguageTvdb(translations);
        if (!*displayLanguage) {
          const char *ol = getValueCharS(*data_episodes, "originalLanguage", "cTVDBScraper::downloadEpisodes");
          if (!ol) {
            esyslog("tvscraper: ERROR cTVDBScraper::downloadEpisodes originalLanguage attribute missing, seriesID %i", seriesID);
            *displayLanguage = nullptr;
            return 1;
          }
          auto liter = find_if(config.m_languages.begin(), config.m_languages.end(), [ol](const cLanguage& x) { return strcmp(ol, x.m_thetvdb) == 0;});
          if (liter != config.m_languages.end() ) {
            *displayLanguage = &(*liter);
          } else {
            esyslog("tvscraper: ERROR cTVDBScraper::downloadEpisodes original language %s in m_languages missing", ol);
            *displayLanguage = nullptr;
          }
        }
        return 1;
      }
// check: is lang the display language, do we update description texts?
      if (!langIsDisplayLanguage && config.isDefaultLanguageThetvdb(lang)) langIsDisplayLanguage = true;
      if (!langIsDisplayLanguage) {
        int tv2lang = db->queryInt("SELECT tv_display_language FROM tv2 WHERE tv_id = ?", -seriesID);
        if (tv2lang > 0 &&
            strcmp(config.GetLanguage(tv2lang)->m_thetvdb, lang->m_thetvdb) == 0) langIsDisplayLanguage = true;
      }
      if (langIsDisplayLanguage) {
// lang is available, and the display language. Set this in tv2
        db->exec("UPDATE tv2 SET tv_display_language = ? WHERE tv_id = ?", langTvdbId, -seriesID);
      }
    }  // end if (first)
// parse episodes
    for (const rapidjson::Value &episode: cJsonArrayIterator(*data_episodes, "episodes") ) {
      int episodeID = getValueInt(episode, "id");
      if (episodeID == 0) continue;
// episodeRunTime
      int episodeRunTime = getValueInt(episode, "runtime");
      if (episodeRunTime != 0) insertRuntime.resetBindStep(-seriesID, episodeRunTime);
      else episodeRunTime = -1; // -1: no data available in external db; 0: data in external db not requested

// tv_s_e
      if (langIsDisplayLanguage) {
// 3: remove name           : here in language lang, which is the display language
        insertEpisode3.resetBindStep(
           -seriesID, getValueInt(episode, "seasonNumber"), getValueInt(episode, "number"), episodeID,
           getValueCharS(episode, "aired"), getValueCharS(episode, "overview"), getValueCharS(episode, "image"), episodeRunTime);
      } else {
// 2: remove name & overview: here in language lang, which is not the display language
        insertEpisode2.resetBindStep(
           -seriesID, getValueInt(episode, "seasonNumber"), getValueInt(episode, "number"), episodeID,
           getValueCharS(episode, "aired"), getValueCharS(episode, "image"), episodeRunTime);
      }

// tv_name
      const char *episodeName = getValueCharS(episode, "name");
      if (episodeName && *episodeName) insertEpisodeLang.resetBindStep(episodeID, langTvdbId, episodeName);
    } // end loop over each episode

    rapidjson::Value::ConstMemberIterator links_it = getTag(episodes, "links", "cTVDBScraper::StoreSeriesJson_lang", &buffer);
    if (links_it == episodes.MemberEnd() || !links_it->value.IsObject() ) break;
    urlE = charPointerToString(getValueCharS(links_it->value, "next"));
    first = false;
  } while (!urlE.empty() );
  time_t now = time(0);
  db->exec("UPDATE tv2 SET tv_last_updated = ? WHERE tv_id = ?", now, -seriesID);
  if (config.enableDebug) {
    long long te = db->queryInt64("SELECT tv_last_updated FROM tv2 WHERE tv_id = ?", -seriesID);
    if (te != (long long)now || te == 0) esyslog("tvscraper: ERROR cTVDBScraper::downloadEpisodes te %lli != now %lli", te, (long long)now);
  }
  if (cSql(db, "SELECT COUNT(episode_id) FROM tv_s_e WHERE tv_id = ?;", -seriesID).getInt(0) > numEpisodes)
    db->exec("UPDATE tv2 SET tv_last_changed = ? WHERE tv_id = ?", now, -seriesID);
  db->exec("INSERT OR REPLACE INTO tv_name (tv_id, language_id, tv_last_updated) VALUES (?, ?, ?)", -seriesID, langTvdbId, now);
// tv_episode_run_time: -1: no data available in external db; no entry: data in external db not requested
  if (!cSql(db, "SELECT 1 from tv_episode_run_time WHERE tv_id = ?", -seriesID).readRow()) insertRuntime.resetBindStep(-seriesID, -1);
  return 0;
}

int cTVDBScraper::CallRestJson(rapidjson::Document &document, const rapidjson::Value *&data, cLargeString &buffer, const char *url, bool disableLog) {
// return 0 on success. In this case, "data" exists, and can be accessed with document["data"].
//  1: error: In this case, an ERROR message is written in syslog
// -1: "Not Found" (no message in syslog, just the reqested object does not exist)
  if (!GetToken() ) return 1; // GetToken will write error in syslog
  buffer.clear();
  apiCalls.start();
  bool success = jsonCallRest(document, buffer, url, config.enableDebug && !disableLog, tokenHeader.c_str(), "charset: utf-8");
  apiCalls.stop();
  if (!success) return 1; // jsonCallRest wrote error in syslog
  const char *status;
  if (!getValue(document, "status", status) || !status) {
    esyslog("tvscraper: cTVDBScraper::CallRestJson, url %s, buffer %s, no status", url, buffer.substr(0, 100).c_str() );
    return 1;
  }
  if (strcmp(status, "success") != 0) {
    const char *message = getValueCharS(document, "message");
    if (strcmp(status, "failure") == 0) {
      if (message && strcmp(message, "Not Found") == 0) return -1;
      if (message && strncmp(message, "NotFoundException", 17) == 0) return -1;
    }
    esyslog("tvscraper: ERROR cTVDBScraper::CallRestJson, url %s, status = %s, message = %s, buffer %s", url, status, message?message:"no message", buffer.substr(0, 100).c_str() );
    return 1;
  }
  rapidjson::Value::ConstMemberIterator data_it = getTag(document, "data", "cTVDBScraper::CallRestJson", &buffer);
  if (data_it == document.MemberEnd() ) return 1;  // getTag wrote "cTVDBScraper::CallRestJson" in syslog
  data = &data_it->value;
  return 0;
}

// methods to download / store media

const char *cTVDBScraper::prefixImageURL1 = "https://artworks.thetvdb.com/banners/";
const char *cTVDBScraper::prefixImageURL2 = "https://artworks.thetvdb.com";

const char *cTVDBScraper::getDbUrl(const char *url) {
  if (!url || !*url) return url;
  const char *s = removePrefix(url, cTVDBScraper::prefixImageURL1);
  if (s) return s;
  s = removePrefix(url, "/banners/");
  if (s) return s;
  return url;
}

void cTVDBScraper::download(const char *url, const char *localPath) {
// create full download url fron given url, and download to local path
  if (!url || !*url || !localPath || !*localPath) return;
  if (strncmp(url, cTVDBScraper::prefixImageURL2, strlen(cTVDBScraper::prefixImageURL2) ) == 0) {
    Download(url, localPath);
    return;
  }
  std::string fullUrl;
  if (url[0] == '/') fullUrl = concatenate(cTVDBScraper::prefixImageURL2, url);
  fullUrl = concatenate(cTVDBScraper::prefixImageURL1, url);  // for URLs returned by APIv3
  Download(fullUrl, localPath);
}

void cTVDBScraper::StoreActors(int seriesID) {
  std::string destDir = concatenate(config.GetBaseDirSeries(), seriesID);
//  if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::StoreActors, seriesID %i destDir %s", seriesID, destDir.c_str());
  if (!CreateDirectory(destDir)) return;
  destDir += "/actor_";
  int destDirLen = destDir.length();
  cSql stmt0(db, "SELECT actor_id, actor_path FROM actor_download WHERE movie_id = ? AND is_movie = ?");
  for (cSql &stmt: stmt0.resetBindStep(seriesID * -1, false)) {
    const char *actor_path = stmt.getCharS(1);
    if (!actor_path || !*actor_path) continue;
    destDir.erase(destDirLen);
    stringAppend(destDir, stmt.getInt(0), ".jpg");
    download(actor_path, destDir.c_str() );
  }
  db->DeleteActorDownload (seriesID * -1, false);
}
void cTVDBScraper::StoreStill(int seriesID, int seasonNumber, int episodeNumber, const char *episodeFilename) {
    if (!episodeFilename || !*episodeFilename) return;
    std::string destDir; destDir.reserve(200);
    stringAppend(destDir, config.GetBaseDirSeries(), seriesID, "/");
    if (!CreateDirectory(destDir) ) return;
    stringAppend(destDir, seasonNumber, "/");
    if (!CreateDirectory(destDir) ) return;
    stringAppend(destDir, "still_", episodeNumber, ".jpg");
    download(episodeFilename, destDir.c_str() );
}
void cTVDBScraper::DownloadMedia (int tvID) {
  std::string destDir = concatenate(config.GetBaseDirSeries(), tvID, "/");
  if (!CreateDirectory(destDir)) return;

  DownloadMedia (tvID, mediaPoster, destDir + "poster_");
  DownloadMedia (tvID, mediaFanart, destDir + "fanart_");
  DownloadMedia (tvID, mediaSeason, destDir + "season_poster_");
  DownloadMediaBanner (tvID, destDir + "banner.jpg");
  db->deleteTvMedia (tvID * -1, false, true);
}

void cTVDBScraper::DownloadMedia (int tvID, eMediaType mediaType, const string &destDir) {
//  if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::DownloadMedia, tvID %i mediaType %i destDir %s", tvID, mediaType, destDir.c_str());
  cSql sql(db, "select media_path, media_number from tv_media where tv_id = ? and media_type = ? and media_number >= 0");
  for (cSql &stmt: sql.resetBindStep(tvID * -1, (int)mediaType)) {
//    if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::DownloadMedia, media[0] %s media[1] %s", media[0].c_str(), media[1].c_str() );
    const char *media_path = stmt.getCharS(0);
    if (!media_path || !*media_path ) continue;
    download(media_path, concatenate(destDir, stmt.getInt(1), ".jpg").c_str() );
  }
}

void cTVDBScraper::DownloadMediaBanner (int tvID, const string &destPath) {
  cSql sql(db,  "select media_path from tv_media where tv_id = ? and media_type = ? and media_number >= 0");
  for (cSql &stmt: sql.resetBindStep(tvID * -1, (int)mediaBanner)) {
    const char *media_path = stmt.getCharS(0);
    if (!media_path || !*media_path ) continue;
    download(media_path, destPath.c_str() );
    return;
  }
}

// Search series
bool cTVDBScraper::AddResults4(cLargeString &buffer, vector<searchResultTvMovie> &resultSet, cSv SearchString, const cCompareStrings &compareStrings, const cLanguage *lang) {
  std::string url; url.reserve(300);
  url.append(baseURL4Search);
  stringAppendCurlEscape(url, SearchString);
 
  rapidjson::Document root;
  const rapidjson::Value *data;
  if (CallRestJson(root, data, buffer, url.c_str() ) != 0) return false;
  if (!data || !data->IsArray() ) {
    esyslog("tvscraper: ERROR cTVDBScraper::AddResults4, data is %s", data?"not an array":"NULL");
    return false;
  }
  for (const rapidjson::Value &result: data->GetArray() ) {
    ParseJson_searchSeries(result, resultSet, compareStrings, lang);
  }
  return true;
}

void cTVDBScraper::ParseJson_searchSeries(const rapidjson::Value &data, vector<searchResultTvMovie> &resultSet, const cCompareStrings &compareStrings, const cLanguage *lang) {// add search results to resultSet
  if (!data.IsObject() ) {
    esyslog("tvscraper: ERROR cTVDBScraper::ParseJson_searchSeries, data is not an object");
    return;
  }
  const char *objectID = getValueCharS(data, "objectID", "cTVDBScraper::ParseJson_searchSeries");
  if (!objectID) return; // syslog entry already created by getValueCharS
  if (strlen(objectID) < 8) {
    esyslog("tvscraper: ERROR cTVDBScraper::ParseJson_searchSeries, objectID.length() < 8, %s", objectID);
    return;
  }
  if (strncmp(objectID, "series-", 7) != 0) {
    esyslog("tvscraper: ERROR cTVDBScraper::ParseJson_searchSeries, objectID does not start with series-, %s", objectID);
    return;
  }
  int seriesID = atoi(objectID + 7);
  if (seriesID == 0) {
    esyslog("tvscraper: ERROR cTVDBScraper::ParseJson_searchSeries, seriesID = 0, %s", objectID);
    return;
  }
  cNormedString normedOriginalName(removeLastPartWithP(getValueCharS(data, "name")));  // name is the original name
  cNormedString normedName;
  if (lang) {
    rapidjson::Value::ConstMemberIterator translationsIt = data.FindMember("translations");
    if (translationsIt != data.MemberEnd() && translationsIt->value.IsObject() ) {
      const char *langVal = getValueCharS(translationsIt->value, lang->m_thetvdb);
      if (langVal) normedName.reset(removeLastPartWithP(langVal));
    }
  }
  for (char delim: compareStrings) {
// distance == deviation from search text
    int dist_a = 1000;
// if search string is not in original language, consider name (== original name) same as alias
    if (lang) dist_a = compareStrings.minDistance(delim, normedOriginalName, dist_a);
    for (const rapidjson::Value &alias: cJsonArrayIterator(data, "aliases")) {
      if (alias.IsString() )
      dist_a = compareStrings.minDistance(delim, removeLastPartWithP(alias.GetString()), dist_a);
    }
// in search results, aliases don't have language information
// in series/<id>/extended, language information for aliases IS available
// still, effort using that seems to be too high
// we give a malus for the missing language information, similar to movies
    dist_a = std::min(dist_a + 50, 1000);
    int requiredDistance = 600; // "standard" require same text similarity as we required for episode matching
    if (!normedName.empty() ) {
      dist_a = compareStrings.minDistance(delim, normedName, dist_a);
      requiredDistance = 700;  // translation in EPG language is available. Reduce requirement somewhat
    }
    if (!lang) dist_a = compareStrings.minDistance(delim, normedOriginalName, dist_a);
    if (dist_a < requiredDistance) {
      auto sResIt = find_if(resultSet.begin(), resultSet.end(),
          [seriesID,delim](const searchResultTvMovie& x) { return x.id() == -seriesID && x.delim() == delim;});
      if (sResIt != resultSet.end() ) {
        if (!normedName.empty() ) {
          sResIt->setMatchTextMin(dist_a, normedName);
        } else {
          sResIt->setMatchTextMin(dist_a, normedOriginalName);
        }
      } else {
// create new result object sRes
        searchResultTvMovie sRes(-seriesID, false, getValueCharS(data, "year"));
        sRes.setPositionInExternalResult(resultSet.size() );
        sRes.setDelim(delim);
        sRes.setMatchText(dist_a);
// main purpose of sRes.m_normedName: figure out if two TV shows are similar
// TODO: We could improve this, may be 2 normedName, or explicitly mark normedOriginalName
// on the other hand side: the current implementation might be good enough, so wait for
// some real live test issues before we change here anything
        if (!normedName.empty() ) sRes.m_normedName = normedName;
        else sRes.m_normedName = normedOriginalName;
        resultSet.push_back(std::move(sRes));
      }
    }
  }
}

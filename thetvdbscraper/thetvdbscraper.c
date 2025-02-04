#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "thetvdbscraper.h"

using namespace std;

cTVDBScraper::cTVDBScraper(cCurl *curl, cTVScraperDB *db): m_curl(curl) {
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
  int result =  m_curl->PostUrl(url, "{\"apikey\": \"5476e702-85aa-45fd-a8da-e74df3840baf\"}", buffer, headers);
  curl_slist_free_all(headers);
  if (result != 0) {
    esyslog("tvscraper: ERROR code %i in cTVDBScraper::GetToken, calling %s", result, url);
    return false;
  }
// now read the tocken
  if (!GetToken(buffer)) {
    esyslog("tvscraper: ERROR cTVDBScraper::GetToken, url %s, parsing json result %s", url, buffer.substr(0, 50).c_str() );
    return false;
  }
  return true;
}

bool cTVDBScraper::GetToken(cStr jsonResponse) {
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

const cLanguage *languageMatchesTvdb(int l, cSplit<cSv> &transSplit) {
// check if language number (ID) l matches any language in transSplit
//   if yes, return this language
//   if no , return nullptr
  auto f = config.m_languages.find(l);
  if (f == config.m_languages.end() ) return nullptr;   // ? should not happen
  if (!f->m_thetvdb || !*f->m_thetvdb) return nullptr;  // ? should not happen
  if (transSplit.find(f->m_thetvdb) != transSplit.end()) return &(*f);
  return nullptr;
}

const cLanguage *displayLanguageTvdb(cSv translations) {
// input: translations: List of translations
// output: displayLanguage: Language used to display the data.
//                          or nullptr if not found
  cSplit transSplit(translations, '|', eSplitDelimBeginEnd::required);
// check: translation in configured default language available?
  if (transSplit.find(config.GetDefaultLanguage()->m_thetvdb) != transSplit.end()) return config.GetDefaultLanguage();
// translation in configured default language is not available
// check: translation in an additional language available?
  std::vector<int> al = config.GetAdditionalLanguages();
  std::array lang_eng_arr{6};
  for (int l: cUnion(al, lang_eng_arr) ) { // add. languages, English
    const cLanguage *res = languageMatchesTvdb(l, transSplit);
    if (res) return res;
  }
  return nullptr;
}

const cLanguage *languageTvdb(cSv tvdbLang, const char *context = nullptr) {
// input: tvdbLang: string with language in TheTVDB format, like "eng"
// output: normed (with config.GetLanguageThetvdb) language
  if (tvdbLang == std::string_view() ) {
    if (context)
      esyslog("tvscraper: ERROR languageTvdb no language provided, context %s", context);
    return nullptr;
  }
  auto lang = find_if(config.m_languages.begin(), config.m_languages.end(), [tvdbLang](const cLanguage& x) { return tvdbLang == x.m_thetvdb;});
  if (lang == config.m_languages.end() ) {
    if (context)
      esyslog("tvscraper: ERROR languageTvdb language %.*s in m_languages missing, context %s", (int)tvdbLang.length(), tvdbLang.data(), context);
    return nullptr;
  }
  const cLanguage *lang_tvdb = config.GetLanguageThetvdb(&(*lang));
  if (!lang_tvdb) {
    esyslog("tvscraper: ERROR languageTvdb GetLanguageThetvdb for %.*s not found, context %s", (int)tvdbLang.length(), tvdbLang.data(), context?context:"no context");
    return &(*lang);
  }
  return lang_tvdb;
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
    if (stmtLanguages.readRow() && !config.isUpdateFromExternalDbRequired(stmtLanguages.get<time_t>(1) )) {
      const cLanguage *lang = displayLanguageTvdb(stmtLanguages.getStringView(0));
      if (lang) displayLanguage = lang;
    }
  }
  if (!displayLanguage || !displayLanguage->m_thetvdb) {
    esyslog("tvscraper: ERROR cTVDBScraper::StoreSeriesJson invalid displayLanguages 2, seriesID %i", seriesID);
    if (displayLanguage) displayLanguage->log();
    return 0;
  }
  const cLanguage *newDisplayLanguage = 0;
  int rde = downloadEpisodes(seriesID, forceUpdate, displayLanguage, true, &newDisplayLanguage);
  if (rde == -1) return -1; // object does not exist
  if (rde != 0 && rde != 1) return 0; // other error
  if (rde == 1) {
// episodes not available in displayLanguage
    if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::StoreSeriesJson displayLanguage %s not available, seriesID %i",  displayLanguage->getNames().c_str(), seriesID);
    if (newDisplayLanguage) {
      displayLanguage = newDisplayLanguage;
      rde = downloadEpisodes(seriesID, forceUpdate, displayLanguage, true);
      if (rde == 1) {
        esyslog("tvscraper: ERROR cTVDBScraper::StoreSeriesJson newDisplayLanguage %s not available, seriesID %i", displayLanguage?displayLanguage->getNames().c_str():"no displayLanguage", seriesID);
      }
    }
  }
  if (!forceUpdate && stmtNameLang.readRow() && !stmtNameLang.getStringView(0).empty() ) return seriesID; // already in db

  cTVDBSeries series(db, this, seriesID);
// this call gives also episode related information like actors, images, ...
// Change: season images type 6 ("Banner") & 7 ("Poster") still included
// Episode Guest stars, writer, ... NOT included any more
// Episode Guest stars, writer, ... NOT available in https://api4.thetvdb.com/v4/seasons/1978231/extended
//
  cJsonDocumentFromUrl document(m_curl);
  const rapidjson::Value *data;
  int error = CallRestJson(document, data, cToSvConcat(baseURL4, "series/", seriesID, "/extended?meta=translations&short=false") );
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
  series.ParseJson_Artwork(*data, displayLanguage);
// store series here, as here information (incl. episode runtimes, poster URL, ...) is complete
  series.StoreDB();
  return seriesID;
}

int cTVDBScraper::downloadEpisodes(int seriesID, bool forceUpdate, const cLanguage *lang, bool langIsIntendedDisplayLanguage, const cLanguage **displayLanguage) {
// if lang == nullptr: use original language
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
//  if (!lang) return 3;
  int langTvdbId = 0;
  if (lang) {
    langTvdbId = config.GetLanguageThetvdb(lang)->m_id;
    if (!forceUpdate && !db->episodeNameUpdateRequired(-seriesID, langTvdbId)) return 0;
// check cached information: episodes available in lang?
    cSql stmtScore(db, "SELECT tv_languages, tv_languages_last_update FROM tv_score WHERE tv_id = ?", -seriesID);
    if (stmtScore.readRow() && !config.isUpdateFromExternalDbRequired(stmtScore.get<time_t>(1) )) {
      cSplit transSplit(stmtScore.getCharS(0), '|', eSplitDelimBeginEnd::required);
      if (transSplit.find(lang->m_thetvdb) == transSplit.end() ) {
// this language is not available
        if (!displayLanguage) return 1; // translation in needed language not available, available language not requested
// figure out displayLanguage
        *displayLanguage = displayLanguageTvdb(stmtScore.getCharS(0));
        if (*displayLanguage) return 1; // an additional language is available
// we have to continue to get original language ...
      }
    }
  }
  if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::downloadEpisodes lang %s, langTvdbId %i, seriesID %i, displayLanguage %s", lang?lang->getNames().c_str():"original language", langTvdbId, seriesID, displayLanguage?"requested":"not requested");

  int numEpisodes = cSql(db, "SELECT COUNT(episode_id) FROM tv_s_e WHERE tv_id = ?;", -seriesID).getInt(0);
  cSql insertEpisodeLang(db, "INSERT OR REPLACE INTO tv_s_e_name2 (episode_id, language_id, episode_name) VALUES (?, ?, ?);");
  cSql insertEpisode2(db, "INSERT INTO tv_s_e (tv_id, season_number, episode_number, episode_id, episode_air_date, episode_still_path, episode_run_time) VALUES (?, ?, ?, ?, ?, ?, ?) ON CONFLICT(tv_id,season_number,episode_number) DO UPDATE SET episode_air_date=excluded.episode_air_date,episode_still_path=excluded.episode_still_path,episode_run_time=excluded.episode_run_time;");
  cSql insertEpisode3(db, "INSERT INTO tv_s_e (tv_id, season_number, episode_number, episode_id, episode_air_date, episode_overview, episode_still_path, episode_run_time) VALUES (?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(tv_id,season_number,episode_number) DO UPDATE SET episode_air_date=excluded.episode_air_date, episode_overview=excluded.episode_overview, episode_still_path=excluded.episode_still_path,episode_run_time=excluded.episode_run_time;");
  cSql insertRuntime(db, "INSERT OR REPLACE INTO tv_episode_run_time (tv_id,episode_run_time) VALUES (?, ?)");

  cToSvConcat urlE(baseURL4, "series/", seriesID);
  if (lang) urlE.concat("/episodes/default/", lang->m_thetvdb, "?page=0");
  else urlE.concat("/episodes/default?page=0");
  bool first = true;
  bool langIsDisplayLanguage = langIsIntendedDisplayLanguage;
  do {
    cJsonDocumentFromUrl episodes(m_curl);
    const rapidjson::Value *data_episodes;
    int error = CallRestJson(episodes, data_episodes, urlE);
    if (error != 0) {
      if (error == -1) {
        if (displayLanguage) *displayLanguage = nullptr;
// might also be returned if episode names not available in this language, using the API (bug in API)
// so, before deleting the series, check esxistance with series api
        cJsonDocumentFromUrl document_series(m_curl);
        const rapidjson::Value *data_series;
        error = CallRestJson(document_series, data_series, cToSvConcat(baseURL4, "series/", seriesID) );
        if (error == -1) db->DeleteSeriesCache(-seriesID); // object does not exist (now verified with other API call)
        return -1;
      }
      return 5;
    }
    if (first) {
// parse not episode related data
      first = false;
      const rapidjson::Value *data_series;
      if (lang) data_series = data_episodes;
      else {
// no language provided. Called API without lang
// this reasults in a different output format, so data_episodes is within tag "series"
// also, we set lang to original language assuming that the API without language returns data in original language
        langIsDisplayLanguage = false;
        rapidjson::Value::ConstMemberIterator s_i = getTag(*data_episodes, "series", "cTVDBScraper::downloadEpisodes no lang", &episodes);
        if (s_i == data_episodes->MemberEnd() ) return 5;
        data_series = &s_i->value;
        const char *orig_lang = getValueCharS(*data_series, "originalLanguage");
        lang = languageTvdb(orig_lang, cToSvConcat("cTVDBScraper::downloadEpisodes series ", seriesID).c_str() );
        if (!lang) {
          if (displayLanguage) *displayLanguage = nullptr;
          return 1;
        }
        langTvdbId = lang->m_id;
      }
      std::string translations = getArrayConcatenated(*data_series, "nameTranslations");
      float popularity = getValueInt(*data_series, "score");
      if (popularity == 0) popularity = 1;  // 0 indicates that no score is available in internal db, and will result in access to external db.
      db->exec("INSERT INTO tv_score (tv_id, tv_score, tv_languages, tv_languages_last_update) VALUES(?, ?, ?, ?) ON CONFLICT(tv_id) DO UPDATE SET tv_score = excluded.tv_score, tv_languages = excluded.tv_languages, tv_languages_last_update = excluded.tv_languages_last_update", -seriesID, popularity, translations, time(0) );

      db->exec("INSERT INTO tv2 (tv_id, tv_original_name, tv_first_air_date, tv_popularity, tv_status) VALUES (?, ?, ?, ?, ?) ON CONFLICT(tv_id) DO UPDATE SET tv_original_name=excluded.tv_original_name, tv_first_air_date=excluded.tv_first_air_date, tv_popularity=excluded.tv_popularity, tv_status=excluded.tv_status",
        -seriesID, getValueCharS(*data_series, "name"), getValueCharS(*data_series, "firstAired"), popularity, getValueCharS2(*data_series, "status", "name") );

// check: translations in lang available?
      cSplit transSplit(translations, '|', eSplitDelimBeginEnd::required);
      bool translationAvailable = transSplit.find(lang->m_thetvdb) != transSplit.end();
      if (translations.empty() ) {
        const char *ol = getValueCharS(*data_series, "originalLanguage");
        if (ol && strcmp(ol, lang->m_thetvdb) == 0) translationAvailable = true;
        esyslog("tvscraper: ERROR cTVDBScraper::downloadEpisodes nameTranslations attribute missing, seriesID %i", seriesID);
      }
      if (displayLanguage) {
// figure out displayLanguage
        *displayLanguage = displayLanguageTvdb(translations);
        if (!*displayLanguage) {
          const char *ol = getValueCharS(*data_series, "originalLanguage", "cTVDBScraper::downloadEpisodes");
          *displayLanguage = languageTvdb(ol, "cTVDBScraper::downloadEpisodes original language");
        }
      }
      if (!translationAvailable) return 1;
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

    rapidjson::Value::ConstMemberIterator links_it = getTag(episodes, "links", "cTVDBScraper::StoreSeriesJson_lang", &episodes);
    if (links_it == episodes.MemberEnd() || !links_it->value.IsObject() ) break;
    urlE.clear();
    urlE.concat(getValueCharS(links_it->value, "next"));
  } while (!urlE.empty() );
  time_t now = time(0);
  db->exec("UPDATE tv2 SET tv_last_updated = ? WHERE tv_id = ?", now, -seriesID);
  if (config.enableDebug) {
    long long te = cSql(db, "SELECT tv_last_updated FROM tv2 WHERE tv_id = ?", -seriesID).get<long long>();
    if (te != (long long)now || te == 0) esyslog("tvscraper: ERROR cTVDBScraper::downloadEpisodes te %lli != now %lli", te, (long long)now);
  }
  if (cSql(db, "SELECT COUNT(episode_id) FROM tv_s_e WHERE tv_id = ?;", -seriesID).getInt(0) > numEpisodes)
    db->exec("UPDATE tv2 SET tv_last_changed = ? WHERE tv_id = ?", now, -seriesID);
  db->exec("INSERT OR REPLACE INTO tv_name (tv_id, language_id, tv_last_updated) VALUES (?, ?, ?)", -seriesID, langTvdbId, now);
// tv_episode_run_time: -1: no data available in external db; no entry: data in external db not requested
  if (!cSql(db, "SELECT 1 from tv_episode_run_time WHERE tv_id = ?", -seriesID).readRow()) insertRuntime.resetBindStep(-seriesID, -1);
  return 0;
}

int cTVDBScraper::CallRestJson(cJsonDocumentFromUrl &document, const rapidjson::Value *&data, cStr url, bool disableLog) {
// return 0 on success. In this case, "data" exists, and can be accessed with document["data"].
//  1: error: In this case, an ERROR message is written in syslog
// -1: "Not Found" (no message in syslog, just the reqested object does not exist)
  int i = 0;
  int rc;
  do {
    rc = CallRestJson_int(document, data, url, disableLog);
    if (rc != 100) return rc;
// rc == 100 -> Internal Server Error
    if (++i > 5) break;
    if (config.enableDebug) esyslog("tvscraper: INFO: Internal Server Error calling \"%s\", i = %i, http response code %ld, trying again ...", url.c_str(), i, document.getHttpResponseCode() );
    sleep(2 + 3*i);
  } while (true);

  esyslog("tvscraper: ERROR Internal Server Error calling \"%s\", i = %i, http response code %ld, giving up", url.c_str(), i, document.getHttpResponseCode() );
  return 1;
}
int cTVDBScraper::CallRestJson_int(cJsonDocumentFromUrl &document, const rapidjson::Value *&data, cStr url, bool disableLog) {
// return 0 on success. In this case, "data" exists, and can be accessed with document["data"].
//  1: error: In this case, an ERROR message is written in syslog
// -1: "Not Found" (no message in syslog, just the reqested object does not exist)
  if (!GetToken() ) return 1; // GetToken will write error in syslog
  document.set_enableDebug(config.enableDebug && !disableLog);
  apiCalls.start();
  bool success = document.download_and_parse(url, tokenHeader.c_str(), "charset: utf-8");
  apiCalls.stop();
  if (!success) return 1; // jsonCallRest wrote error in syslog
  const char *status;
  if (!getValue(document, "status", status) || !status) {
    esyslog("tvscraper: cTVDBScraper::CallRestJson, url %s, doc %s, no status", url.c_str(), document.context() );
    return 1;
  }
  if (strcmp(status, "success") != 0) {
    long httpResponseCode = document.getHttpResponseCode();
    if (httpResponseCode == 404) return -1;  // series not found. This is documented in https://thetvdb.github.io/v4-ap
    const char *message = getValueCharS(document, "message");
    if (strcmp(status, "failure") == 0) {
      if (message && strcmp(message, "Not Found") == 0) return -1;
      if (message && strncmp(message, "NotFoundException", 17) == 0) return -1;
      if (message && strncmp(message, "Internal Server Error", 21) == 0) return 100;
    }
    esyslog("tvscraper: ERROR cTVDBScraper::CallRestJson, url %s, http response code %ld, status = %s, message = %s, doc %s", url.c_str(), httpResponseCode, status, message?message:"no message", document.context() );
    return 1;
  }
  rapidjson::Value::ConstMemberIterator data_it = getTag(document, "data", "cTVDBScraper::CallRestJson", &document);
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

void cTVDBScraper::download(cStr url, cStr localPath) {
// create full download url fron given url, and download to local path
  if (strncmp(url.c_str(), cTVDBScraper::prefixImageURL2, strlen(cTVDBScraper::prefixImageURL2) ) == 0) {
    DownloadImg(m_curl, url, localPath);
    return;
  }
  cToSvConcat fullUrl;
  if (url[0] == '/') fullUrl.concat(cTVDBScraper::prefixImageURL2, url);
  else fullUrl.concat(cTVDBScraper::prefixImageURL1, url);  // for URLs returned by APIv3
  DownloadImg(m_curl, fullUrl, localPath);
}

void cTVDBScraper::StoreActors(int seriesID) {
  cToSvConcat destDir(config.GetBaseDirSeries(), seriesID);
//  if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::StoreActors, seriesID %i destDir %s", seriesID, destDir.c_str());
  if (!CreateDirectory(destDir)) return;
  destDir << "/actor_";
  int destDirLen = destDir.length();
  cSql stmt0(db, "SELECT actor_id, actor_path FROM actor_download WHERE movie_id = ? AND is_movie = ?");
  for (cSql &stmt: stmt0.resetBindStep(-seriesID, false)) {
    const char *actor_path = stmt.getCharS(1);
    if (!actor_path || !*actor_path) continue;
    destDir.erase(destDirLen);
    destDir << stmt.getInt(0) << ".jpg";
    download(actor_path, destDir);
  }
  db->DeleteActorDownload (-seriesID, false);
}
void cTVDBScraper::StoreStill(int seriesID, int seasonNumber, int episodeNumber, const char *episodeFilename) {
    if (!episodeFilename || !*episodeFilename) return;
    cToSvConcat destDir(config.GetBaseDirSeries(), seriesID, "/");
    if (!CreateDirectory(destDir) ) return;
    destDir << seasonNumber << "/";
    if (!CreateDirectory(destDir) ) return;
    destDir << "still_" << episodeNumber << ".jpg";
    download(episodeFilename, destDir);
}
void cTVDBScraper::DownloadMedia (int tvID) {
  cToSvConcat destDir(config.GetBaseDirSeries(), tvID, "/");
  if (!CreateDirectory(destDir)) return;

  DownloadMedia (tvID, mediaPoster, cToSvConcat(destDir, "poster_"));
  DownloadMedia (tvID, mediaFanart, cToSvConcat(destDir, "fanart_"));
  DownloadMedia (tvID, mediaSeason, cToSvConcat(destDir, "season_poster_"));
  DownloadMediaBanner (tvID, cToSvConcat(destDir, "banner.jpg"));
  db->deleteTvMedia (tvID * -1, false, true);
}

void cTVDBScraper::DownloadMedia (int tvID, eMediaType mediaType, cStr destDir) {
//  if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::DownloadMedia, tvID %i mediaType %i destDir %s", tvID, mediaType, destDir.c_str());
  cSql sql(db, "select media_path, media_number from tv_media where tv_id = ? and media_type = ? and media_number >= 0");
  for (cSql &stmt: sql.resetBindStep(tvID * -1, (int)mediaType)) {
//    if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::DownloadMedia, media[0] %s media[1] %s", media[0].c_str(), media[1].c_str() );
    const char *media_path = stmt.getCharS(0);
    if (!media_path || !*media_path ) continue;
    download(media_path, cToSvConcat(destDir, stmt.getInt(1), ".jpg"));
  }
}

void cTVDBScraper::DownloadMediaBanner (int tvID, cStr destPath) {
  cSqlValue<const char*> sql(db,  "select media_path from tv_media where tv_id = ? and media_type = ? and media_number >= 0");
  for (const char *media_path: sql.resetBindStep(tvID * -1, (int)mediaBanner)) {
    if (!media_path || !*media_path ) continue;
    download(media_path, destPath);
    return;
  }
}

// Search series
bool cTVDBScraper::AddResults4(vector<searchResultTvMovie> &resultSet, cSv SearchString, const cCompareStrings &compareStrings, const cLanguage *lang, int network_id) {
  cToSvConcat url(baseURL4Search);
  m_curl->appendCurlEscape(url, SearchString);
 
  cJsonDocumentFromUrl root(m_curl);
  const rapidjson::Value *data;
  if (CallRestJson(root, data, url) != 0) return false;
  if (!data || !data->IsArray() ) {
    esyslog("tvscraper: ERROR cTVDBScraper::AddResults4, data is %s", data?"not an array":"NULL");
    return false;
  }
  for (const rapidjson::Value &result: data->GetArray() ) {
    ParseJson_searchSeries(result, resultSet, compareStrings, lang, network_id);
  }
  return true;
}

void cTVDBScraper::ParseJson_searchSeries(const rapidjson::Value &data, vector<searchResultTvMovie> &resultSet, const cCompareStrings &compareStrings, const cLanguage *lang, int network_id) {// add search results to resultSet
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
  for (char delim: compareStrings) if (delim != 's') {
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
        if (network_id != 0) sRes.setNetworkMatch(config.Get_TheTVDB_company_ID_from_TheTVDB_company_name(getValueCharS(data, "network")) == network_id);
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

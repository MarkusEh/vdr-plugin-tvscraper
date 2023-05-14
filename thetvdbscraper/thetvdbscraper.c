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
    esyslog("tvscraper: ERROR cTVDBScraper::GetToken, url %s, parsing json result %s", url, buffer.erase(40).c_str() );
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
  rapidjson::Value::ConstMemberIterator data_it = getTag(document, "data", "cTVDBScraper::GetToken");
  if (data_it == document.MemberEnd() || !data_it->value.IsObject() ) return false;
  const char *tocken = getValueCharS(data_it->value, "token", "cTVDBScraper::GetToken");
  if (!tocken) return false;
  tokenHeader = "Authorization: Bearer ";
  tokenHeader.append(tocken);
  tokenHeaderCreated = time(0);
  return true;
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
  time_t tv_last_updated = 0;
  int numEpisodes = 0;
  cSql stmtLastUpdate(db, "SELECT tv_last_updated FROM tv2 WHERE tv_id = ?", -1*seriesID);
  bool exists = stmtLastUpdate.readRow(tv_last_updated);
  if (exists && !forceUpdate) return seriesID; // already in db
  cSql stmtNumEpisodes(db, "SELECT COUNT(episode_id) FROM tv_s_e WHERE tv_id = ?;");
  if (exists) numEpisodes = stmtNumEpisodes.resetBindStep(-1*seriesID).getInt(0);

  cTVDBSeries series(db, this, seriesID);
// this call gives also episode related information like actors, images, ...
// Change: season images type 6 ("Banner") & 7 ("Poster") still included
// Episode Guest stars, writer, ... NOT included any more
// Episode Guest stars, writer, ... NOT available in https://api4.thetvdb.com/v4/seasons/1978231/extended
//
  CONCATENATE(url, baseURL4, "series/", seriesID, "/extended?meta=translations&short=false");
  cLargeString buffer("cTVDBScraper::StoreSeriesJson", 15000);
  rapidjson::Document document;
  const rapidjson::Value *data;
  int error = CallRestJson(document, data, buffer, url);
  if (error != 0) {
    if (error == -1) { db->DeleteSeriesCache(-seriesID); return -1; } // object does not exist
    return 0;
  }
  series.ParseJson_Series(*data);
// episodes
  cSql insertEpisode(db, "INSERT OR REPLACE INTO tv_s_e (tv_id, season_number, episode_number, episode_id, episode_name, episode_air_date, episode_overview, episode_still_path, episode_run_time) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);");
  cLargeString bufferE("cTVDBScraper::StoreSeriesJson Episodes", 15000);
  string urlE = concatenate(baseURL4, "series/", seriesID, "/episodes/default/", series.m_language, "?page=0");
  while (!urlE.empty() ) {
    rapidjson::Document episodes;
    const rapidjson::Value *data_episodes;
    if (CallRestJson(episodes, data_episodes, bufferE, urlE.c_str() ) != 0) break;
// parse episodes
    for (const rapidjson::Value &episode: cJsonArrayIterator(*data_episodes, "episodes") ) {
      series.ParseJson_Episode(episode, insertEpisode);
// dont't read episode character data here, this is too often called just to figure out if we have the right series
    }
    rapidjson::Value::ConstMemberIterator links_it = getTag(episodes, "links", "cTVDBScraper::StoreSeriesJson");
    if (links_it == episodes.MemberEnd() || !links_it->value.IsObject() ) break;
    urlE = charPointerToString(getValueCharS(links_it->value, "next"));
  }
// characters / actors
// we also add characters to episodes. Therefore, we do this after parsing the episodes
  for (const rapidjson::Value &character: cJsonArrayIterator(*data, "characters") ) {
    series.ParseJson_Character(character);
  }

// we also add season images. Therefore, we do this after parsing the episodes
  series.ParseJson_Artwork(*data);
// store series here, as here information (incl. episode runtimes, poster URL, ...) is complete
  series.StoreDB();
  db->exec("UPDATE tv2 SET tv_last_updated = ? WHERE tv_id= ?", time(0), -1*seriesID);
  if (!exists || numEpisodes < stmtNumEpisodes.resetBindStep(-1*seriesID).getInt(0))
    db->exec("UPDATE tv2 SET tv_last_changed = ? WHERE tv_id= ?", time(0), -1*seriesID);
  return seriesID;
}

int cTVDBScraper::StoreSeriesJson(int seriesID, const cLanguage *lang) {
// return 0 in case of error
// only update if required (there are less episodes in this language than episodes in default language
  if (seriesID == 0) return 0;
  if (!db->episodeNameUpdateRequired(seriesID * (-1), lang->m_id)) return seriesID;
  if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::StoreSeriesJson lang %s, seriesID %i", lang->getNames().c_str(), seriesID);

  cSql insertEpisodeLang(db, "INSERT OR REPLACE INTO tv_s_e_name2 (episode_id, language_id, episode_name) VALUES (?, ?, ?);");

  cTVDBSeries series(db, this, seriesID);
// episodes
  cLargeString bufferE("cTVDBScraper::StoreSeriesJson lang", 10000);
  string urlE = concatenate(baseURL4, "series/", seriesID, "/episodes/default/", lang->m_thetvdb, "?page=0");
  while (!urlE.empty() ) {
    rapidjson::Document episodes;
    const rapidjson::Value *data_episodes;
    if (CallRestJson(episodes, data_episodes, bufferE, urlE.c_str() ) != 0) break;
// parse episodes
    for (const rapidjson::Value &episode: cJsonArrayIterator(*data_episodes, "episodes") ) {
      series.ParseJson_Episode(episode, lang, insertEpisodeLang);
    }
    rapidjson::Value::ConstMemberIterator links_it = getTag(episodes, "links", "cTVDBScraper::StoreSeriesJson_lang");
    if (links_it == episodes.MemberEnd() || !links_it->value.IsObject() ) break;
    urlE = charPointerToString(getValueCharS(links_it->value, "next"));
  }
  db->exec("INSERT OR REPLACE INTO tv_name (tv_id, language_id, tv_last_updated) VALUES (?, ?, ?)", -1*seriesID, lang->m_id, time(0));
  return seriesID;
}

int cTVDBScraper::CallRestJson(rapidjson::Document &document, const rapidjson::Value *&data, cLargeString &buffer, const char *url, bool disableLog) {
// return 0 on success. In this case, "data" exists, and can be accessed with document["data"].
//  1: error
// -1: "Not Found" (no message in syslog, just the reqested object does not exist)
  if (!GetToken() ) return 1;
  buffer.clear();
  apiCalls.start();
  bool success = jsonCallRest(document, buffer, url, config.enableDebug && !disableLog, tokenHeader.c_str(), "charset: utf-8");
  apiCalls.stop();
  if (!success) return 1;
  const char *status;
  if (!getValue(document, "status", status) || !status) {
    esyslog("tvscraper: cTVDBScraper::CallRestJson, url %s, buffer %s, no status", url, buffer.erase(50).c_str() );
    return 1;
  }
  if (strcmp(status, "success") != 0) {
    const char *message = getValueCharS(document, "message");
    if (strcmp(status, "failure") == 0) {
      if (message && strcmp(message, "Not Found") == 0) return -1;
      if (message && strncmp(message, "NotFoundException", 17) == 0) return -1;
    }
    esyslog("tvscraper: ERROR cTVDBScraper::CallRestJson, url %s, status = %s, message = %s, buffer %s", url, status, message?message:"no message", buffer.erase(50).c_str());
    return 1;
  }
  rapidjson::Value::ConstMemberIterator data_it = getTag(document, "data", "cTVDBScraper::CallRestJson");
  if (data_it == document.MemberEnd() ) return 1; 
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
bool cTVDBScraper::AddResults4(cLargeString &buffer, vector<searchResultTvMovie> &resultSet, std::string_view SearchString, const std::vector<std::optional<cNormedString>> &normedStrings, const cLanguage *lang) {
  CONVERT(SearchString_rom, SearchString, removeRomanNumC);
  if (*SearchString_rom == 0) {
    esyslog("tvscraper: ERROR cTVDBScraper::AddResults4, SearchString_rom == empty");
    return false;
  }
  CURLESCAPE(url_e, SearchString_rom);
  CONCATENATE(url, baseURL4Search, url_e);
  rapidjson::Document root;
  const rapidjson::Value *data;
  if (CallRestJson(root, data, buffer, url) != 0) return false;
  if (!data || !data->IsArray() ) {
    esyslog("tvscraper: ERROR cTVDBScraper::AddResults4, data is %s", data?"not an array":"NULL");
    return false;
  }
  for (const rapidjson::Value &result: data->GetArray() ){
    ParseJson_searchSeries(result, resultSet, normedStrings, lang);
  }
  return true;
}

void cTVDBScraper::ParseJson_searchSeries(const rapidjson::Value &data, vector<searchResultTvMovie> &resultSet, const std::vector<std::optional<cNormedString>> &normedStrings, const cLanguage *lang) {// add search results to resultSet
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
// is this series already in the list?
  for (const searchResultTvMovie &sRes: resultSet) if (sRes.id() == seriesID * (-1) ) return;

// create new result object sRes
  searchResultTvMovie sRes(seriesID * (-1), false, getValueCharS(data, "year"));
  sRes.setPositionInExternalResult(resultSet.size() );

// distance == deviation from search text
  int dist_a = 1000;
// if search string is not in original language, consider name (== original name) same as alias
  if (lang) dist_a = cNormedString(removeLastPartWithP(getValueCharS(data, "name"))).minDistanceNormedStrings(normedStrings, dist_a);
  for (const rapidjson::Value &alias: cJsonArrayIterator(data, "aliases")) {
    if (alias.IsString() )
    dist_a = cNormedString(removeLastPartWithP(alias.GetString() )).minDistanceNormedStrings(normedStrings, dist_a);
  }
// in search results, aliases don't have language information
// in series/<id>/extended, language information for aliases IS available
// still, effort using that seems to be too high
// we give a malus for the missing language information, similar to movies
  dist_a = std::min(dist_a + 50, 1000);
  int requiredDistance = 600; // "standard" require same text similarity as we required for episode matching
  if (lang) {
    rapidjson::Value::ConstMemberIterator translationsIt = data.FindMember("translations");
    if (translationsIt != data.MemberEnd() && translationsIt->value.IsObject() ) {
      const char *langVal = getValueCharS(translationsIt->value, lang->m_thetvdb);
      if (langVal) {
        sRes.normedName.reset(removeLastPartWithP(langVal));
        dist_a = sRes.normedName.minDistanceNormedStrings(normedStrings, dist_a);
        requiredDistance = 700;  // translation in EPG language is available. Reduce requirement somewhat
      }
    }
  } else {
// name is the name in original / primary language
    sRes.normedName.reset(removeLastPartWithP(getValueCharS(data, "name")));
    dist_a = sRes.normedName.minDistanceNormedStrings(normedStrings, dist_a);
  }
  if (dist_a < requiredDistance) {
    sRes.setMatchText(dist_a);
    resultSet.push_back(std::move(sRes));
  }
}

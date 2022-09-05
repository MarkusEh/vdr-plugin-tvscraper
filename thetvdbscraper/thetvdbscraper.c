#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "thetvdbscraper.h"

using namespace std;

cTVDBScraper::cTVDBScraper(string baseDir, cTVScraperDB *db, string language) {
    apiKey = "E9DBB94CA50832ED";
    apiKey4 = "5476e702-85aa-45fd-a8da-e74df3840baf";
    baseURL = "thetvdb.com";
    baseURL4 = "https://api4.thetvdb.com/v4/";
    baseURL4Search = "https://api4.thetvdb.com/v4/search?type=series&query=";
    this->baseDir = baseDir;
    this->language = language;
    this->db = db;
    mirrors = NULL;
}

cTVDBScraper::~cTVDBScraper() {
    if (mirrors) delete mirrors;
}

bool cTVDBScraper::Connect(void) {
    stringstream url;
    url << baseURL << "/api/" << apiKey << "/mirrors.xml";
    string mirrorsXML;
    if (CurlGetUrl(url.str().c_str(), mirrorsXML)) {
        mirrors = new cTVDBMirrors(mirrorsXML);
        return mirrors->ParseXML();
    }
    return false;
}

bool cTVDBScraper::GetToken(void) {
// return true on success
  if (time(0) - tokenHeaderCreated < 24*60*60) return true; // call only once a day
  const char *url = "https://api4.thetvdb.com/v4/login";
  std::string out;

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "accept: application/json");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "charset: utf-8");
  bool result =  CurlPostUrl(url, "{\"apikey\": \"5476e702-85aa-45fd-a8da-e74df3840baf\"}", out, headers);
  curl_slist_free_all(headers);
  if (!result) {
    esyslog("tvscraper: error calling %s", url);
    return false;
  }
// now read the tocken
  if (!GetToken(out)) {
    esyslog("tvscraper: error parsing json result %s", out.substr(0, 40).c_str() );
    return false;
  }
  return true;
}

bool cTVDBScraper::GetToken(const std::string &jsonResponse) {
  json_t *root = json_loads(jsonResponse.c_str(), 0, NULL);
  if (!root) return false;
  if (!json_is_object(root)) { json_decref(root); return false; }
  if (json_string_value_validated(root, "status").compare("success") != 0) {
    esyslog("tvscraper: error getting thetvdb token, status = %s", json_string_value_validated(root, "status").c_str() );
    json_decref(root);
    return false;
  }
  json_t *jData = json_object_get(root, "data");
  if(!json_is_object(jData)) { json_decref(root); return false; }
  tokenHeader = "Authorization: Bearer ";
  tokenHeader.append(json_string_value_validated(jData, "token") );
  tokenHeaderCreated = time(0);
  json_decref(root);
  return true;
}

int cTVDBScraper::StoreSeries(int seriesID, bool onlyEpisodes) {
    int ns, ne;
    if (config.enableDebug && seriesID == 232731) esyslog("tvscraper: cTVDBScraper::StoreSeries, 1, seriesID %i, onlyEpisodes %i", seriesID, onlyEpisodes);
    if (!onlyEpisodes && db->TvGetNumberOfEpisodes(seriesID * (-1), ns, ne)) return seriesID; // already in db
    cTVDBSeries *series = NULL;
    cTVDBSeriesMedia *media = NULL;
    cTVDBActors *actors = NULL;
    if (!ReadAll(seriesID, series, actors, media, onlyEpisodes)) return 0; // this also stores the series + episodes
    if (config.enableDebug && seriesID == 232731) esyslog("tvscraper: cTVDBScraper::StoreSeries, 3, seriesID %i, onlyEpisodes %i", seriesID, onlyEpisodes);
    if (!series) return 0;
    if (!onlyEpisodes) {
      if (actors) actors->StoreDB(db, series->ID());
      StoreMedia(series, media, actors);
    }
    if (series) {
      seriesID = series->ID();
      delete series;
    }
    if (media) delete media;
    if (actors) delete actors;
    return seriesID;
}

bool cTVDBScraper::ReadAll(int seriesID, cTVDBSeries *&series, cTVDBActors *&actors, cTVDBSeriesMedia *&media, bool onlyEpisodes) {
    stringstream url;
    url << mirrors->GetMirrorXML() << "/api/" << apiKey << "/series/" << seriesID << "/all/" << language << ".xml";
// https://thetvdb.com/api/E9DBB94CA50832ED/series/413627/de.xml
// https://thetvdb.com/api/E9DBB94CA50832ED/series/413627/all/de.xml
// for all information, including episodes:  https://thetvdb.com/api/E9DBB94CA50832ED/series/413627/all/de.zip
    string xmlAll;
    if (config.enableDebug) esyslog("tvscraper: calling %s", url.str().c_str());
    if (!CurlGetUrl(url.str().c_str(), xmlAll)) return false;
// xmlAll available
    xmlInitParser();
    xmlDoc *doc = xmlReadMemory(xmlAll.c_str(), strlen(xmlAll.c_str()), "noname.xml", NULL, 0);
    if(!doc) {
      esyslog("tvscraper: ERROR xml parsing document returned from %s", url.str().c_str());
      return false;
    }
    series = new cTVDBSeries(db, this);
    series->ParseXML_all(doc);
    if (!onlyEpisodes) {
      actors = NULL;
      media  = NULL;
      xmlNode *node = NULL;
      node = xmlDocGetRootElement(doc);
      if (node && !xmlStrcmp(node->name, (const xmlChar *)"Data")) {
        node = node->children;
        for (xmlNode *cur_node = node; cur_node; cur_node = cur_node->next) {
          if ((cur_node->type == XML_ELEMENT_NODE) && !xmlStrcmp(cur_node->name, (const xmlChar *)"Actors")) {
            if(!actors) {
//            if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::ReadAll, before actors");
              actors = new cTVDBActors(language);
              actors->ReadActors(doc, cur_node);
//            if (seriesID == 353808 && config.enableDebug) esyslog("tvscraper: cTVDBScraper::ReadAll, after actors");
            }
          } else if ((cur_node->type == XML_ELEMENT_NODE) && !xmlStrcmp(cur_node->name, (const xmlChar *)"Banners")) {
            if(!media) {
//              if (seriesID == 353808 && config.enableDebug) esyslog("tvscraper: cTVDBScraper::ReadAll, before media");
              media = new cTVDBSeriesMedia(language);
              media->ReadMedia(doc, cur_node);
//      if (seriesID == 353808 && config.enableDebug && !series) esyslog("tvscraper: cTVDBScraper::ReadAll, after media, series exists NOT!");
//      if (seriesID == 353808 && config.enableDebug &&  series) esyslog("tvscraper: cTVDBScraper::ReadAll, after media, series exists");
            }
          }
        }
      } else esyslog("tvscraper: ERROR xml root node != \"Data\", document returned from %s", url.str().c_str());
    }
    xmlFreeDoc(doc);
//  if (seriesID == 353808 && config.enableDebug && !series) esyslog("tvscraper: cTVDBScraper::ReadAll, (End), series exists NOT!");
//  if (seriesID == 353808 && config.enableDebug &&  series) esyslog("tvscraper: cTVDBScraper::ReadAll, (End), series exists");
    return true;
}

void cTVDBScraper::StoreMedia(cTVDBSeries *series, cTVDBSeriesMedia *media, cTVDBActors *actors) {
    stringstream baseUrl;
    baseUrl << mirrors->GetMirrorBanner() << "/banners/";
    stringstream destDir;
//  if (config.enableDebug &&  series) esyslog("tvscraper: cTVDBScraper::StoreMedia, before series->ID(), series %i exists", series->ID());
//    if (config.enableDebug && !series) esyslog("tvscraper: cTVDBScraper::StoreMedia, before series->ID(), series exists NOT");
    destDir << baseDir << "/" << series->ID() << "/";
    bool ok = CreateDirectory(destDir.str().c_str());
    if (!ok)
        return;
    if (series) {
        series->StoreMedia(series->ID() );
//        if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::StoreMedia, after series->StoreBanner");
    }
    if (media) {
        media->Store(series->ID(), db);
//        if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::StoreMedia, after media->Store");
    }
}
void cTVDBScraper::StoreActors(int seriesID) {
  stringstream baseUrl;
  baseUrl << mirrors->GetMirrorBanner() << "/banners/";
  stringstream destDir;
  destDir << baseDir << "/" << seriesID;
//  if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::StoreActors, seriesID %i destDir %s baseUrl %s", seriesID, destDir.str().c_str(), baseUrl.str().c_str() );
  if (!CreateDirectory(destDir.str().c_str()) ) return;
  destDir << "/actor_";
  for (const vector<string> &actor: db->GetActorDownload(seriesID * -1, false) ) {
    if (config.enableDebug && seriesID == 232731) esyslog("tvscraper: cTVDBScraper::StoreActors, seriesID %i destDir %s baseUrl %s, actor[0] %s actor[1] %s", seriesID, destDir.str().c_str(), baseUrl.str().c_str(), actor[0].c_str(), actor[1].c_str() );
    if (actor.size() < 2 || actor[1].empty() ) continue;
    Download(baseUrl.str() + actor[1], destDir.str() + actor[0] + ".jpg");
  }
  db->DeleteActorDownload (seriesID * -1, false);
}
void cTVDBScraper::StoreStill(int seriesID, int seasonNumber, int episodeNumber, const string &episodeFilename) {
    if (episodeFilename.empty() ) return;
    stringstream url;
    url << mirrors->GetMirrorBanner() << "/banners/" << episodeFilename;
    stringstream destDir;
    destDir << baseDir << "/" << seriesID << "/";
    bool ok = CreateDirectory(destDir.str().c_str());
    if (!ok) return;
    destDir << seasonNumber << "/";
    ok = CreateDirectory(destDir.str().c_str());
    if (!ok) return;
    destDir << "still_" << episodeNumber << ".jpg";
    string pathStill = destDir.str();
    Download(url.str(), pathStill);
}
vector<vector<string>> cTVDBScraper::GetTvRuntimes(int seriesID) {
  vector<vector<string>> runtimes = db->GetTvRuntimes(seriesID * -1);
  if (runtimes.size() > 0) return runtimes;
  StoreSeries(seriesID, false);
  return db->GetTvRuntimes(seriesID * -1);
}
void cTVDBScraper::GetTvVote(int seriesID, float &vote_average, int &vote_count) {
  if (db->GetTvVote(seriesID * -1, vote_average, vote_count) ) return;
  StoreSeries(seriesID, false);
  db->GetTvVote(seriesID * -1, vote_average, vote_count);
}

void cTVDBScraper::DownloadMedia (int tvID) {
  stringstream baseUrl, destDir;
  baseUrl << mirrors->GetMirrorBanner() << "/banners/";
  destDir << baseDir << "/" << tvID << "/";
  if (!CreateDirectory(destDir.str().c_str()) ) return;

  DownloadMedia (tvID, mediaPoster, destDir.str() + "poster_", baseUrl.str() );
  DownloadMedia (tvID, mediaFanart, destDir.str() + "fanart_", baseUrl.str() );
  DownloadMedia (tvID, mediaSeason, destDir.str() + "season_poster_", baseUrl.str() );
  DownloadMediaBanner (tvID, destDir.str() + "banner.jpg", baseUrl.str() );
  db->deleteTvMedia (tvID * -1, false, true);
}

void cTVDBScraper::DownloadMedia (int tvID, eMediaType mediaType, const string &destDir, const string &baseUrl) {
//  if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::DownloadMedia, tvID %i mediaType %i destDir %s baseUrl %s", tvID, mediaType, destDir.c_str(), baseUrl.c_str() );
  for (const vector<string> &media: db->GetTvMedia(tvID * -1, mediaType) ) if (media.size() == 2) {
//    if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::DownloadMedia, media[0] %s media[1] %s", media[0].c_str(), media[1].c_str() );
    if (media[0].empty() ) continue;
    Download(baseUrl + media[0], destDir + media[1] + ".jpg");
  }
}

void cTVDBScraper::DownloadMediaBanner (int tvID, const string &destPath, const string &baseUrl) {
  for (const vector<string> &media: db->GetTvMedia(tvID * -1, mediaBanner) ) if (media.size() == 2) {
    if (media[0].empty() ) continue;
    Download(baseUrl + media[0], destPath);
    return;
  }
}

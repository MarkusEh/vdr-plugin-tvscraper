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
    baseURL = "thetvdb.com";
    this->baseDir = baseDir;
    this->language = language;
    this->db = db;
    mirrors = NULL;
}

cTVDBScraper::~cTVDBScraper() {
    if (mirrors)
        delete mirrors;
}

int cTVDBScraper::StoreSeries(int seriesID, bool onlyEpisodes) {
    int ns, ne;
    if (!onlyEpisodes && db->TvGetNumberOfEpisodes(seriesID * (-1), ns, ne)) return seriesID; // already in db
    cTVDBSeries *series = NULL;
    cTVDBSeriesMedia *media = NULL;
    cTVDBActors *actors = NULL;
    if (!ReadAll(seriesID, series, actors, media, onlyEpisodes)) return 0; // this also stores the series + episodes
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


bool cTVDBScraper::Connect(void) {
    stringstream url;
    url << baseURL << "/api/" << apiKey << "/mirrors.xml";
    string mirrorsXML;
    if (CurlGetUrl(url.str().c_str(), &mirrorsXML)) {
        mirrors = new cTVDBMirrors(mirrorsXML);
        return mirrors->ParseXML();
    }
    return false;
}

bool cTVDBScraper::ReadAll(int seriesID, cTVDBSeries *&series, cTVDBActors *&actors, cTVDBSeriesMedia *&media, bool onlyEpisodes) {
    stringstream url;
    url << mirrors->GetMirrorXML() << "/api/" << apiKey << "/series/" << seriesID << "/all/" << language << ".xml";
// https://thetvdb.com/api/E9DBB94CA50832ED/series/413627/de.xml
// https://thetvdb.com/api/E9DBB94CA50832ED/series/413627/all/de.xml
// for all information, including episodes:  https://thetvdb.com/api/E9DBB94CA50832ED/series/413627/all/de.zip
    string xmlAll;
    if (config.enableDebug) esyslog("tvscraper: calling %s", url.str().c_str());
    if (!CurlGetUrl(url.str().c_str(), &xmlAll)) return false;
// xmlAll available
    xmlInitParser();
    xmlDoc *doc = xmlReadMemory(xmlAll.c_str(), strlen(xmlAll.c_str()), "noname.xml", NULL, 0);
    if(!doc) return false;
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
//              if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::ReadAll, before actors");
              actors = new cTVDBActors(language);
              actors->ReadActors(doc, cur_node);
    // if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::ReadAll, after actors");
            }
          } else if ((cur_node->type == XML_ELEMENT_NODE) && !xmlStrcmp(cur_node->name, (const xmlChar *)"Banners")) {
            if(!media) {
//              if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::ReadAll, before media");
              media = new cTVDBSeriesMedia(language);
              media->ReadMedia(doc, cur_node);
//    if (config.enableDebug && !series) esyslog("tvscraper: cTVDBScraper::ReadAll, after media, series exists NOT!");
//    if (config.enableDebug &&  series) esyslog("tvscraper: cTVDBScraper::ReadAll, after media, series exists");
            }
          }
        }
      }
    }
    xmlFreeDoc(doc);
//    if (config.enableDebug && !series) esyslog("tvscraper: cTVDBScraper::ReadAll, (End), series exists NOT!");
//    if (config.enableDebug &&  series) esyslog("tvscraper: cTVDBScraper::ReadAll, (End), series exists");
    return true;
}

void cTVDBScraper::StoreMedia(cTVDBSeries *series, cTVDBSeriesMedia *media, cTVDBActors *actors) {
    stringstream baseUrl;
    baseUrl << mirrors->GetMirrorBanner() << "/banners/";
    stringstream destDir;
//    if (config.enableDebug &&  series) esyslog("tvscraper: cTVDBScraper::StoreMedia, before series->ID(), series exists");
//    if (config.enableDebug && !series) esyslog("tvscraper: cTVDBScraper::StoreMedia, before series->ID(), series exists NOT");
    destDir << baseDir << "/" << series->ID() << "/";
    bool ok = CreateDirectory(destDir.str().c_str());
    if (!ok)
        return;
    if (series) {
        series->StoreBanner(baseUrl.str(), destDir.str());
//        if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::StoreMedia, after series->StoreBanner");
    }
    if (media) {
        media->Store(baseUrl.str(), destDir.str());
//        if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::StoreMedia, after media->Store");
    }
    if (actors) {
        actors->Store(baseUrl.str(), destDir.str());
//        if (config.enableDebug) esyslog("tvscraper: cTVDBScraper::StoreMedia, after actors->Store");
    }
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

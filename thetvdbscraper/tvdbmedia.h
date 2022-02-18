#ifndef __TVSCRAPER_TVDBMEDIA_H
#define __TVSCRAPER_TVDBMEDIA_H

using namespace std; 

enum mediaType {
    mediaUnknown,
    mediaPoster,
    mediaFanart,
    mediaSeason,
    mediaBanner,
};

// --- cTVDBMedia -------------------------------------------------------------
class cTVDBMedia {
public:
    cTVDBMedia(void) {
        path = "";
        language = "";
        season = 0;
    };
    mediaType type;
    string path;
    string language;
    int season;
};

// --- cTVDBSeriesMedia --------------------------------------------------------

class cTVDBSeriesMedia {
private:
    string language;
    vector<cTVDBMedia*> medias;
    void ReadEntry(xmlDoc *doc, xmlNode *node);
public:
    cTVDBSeriesMedia(string language);
    virtual ~cTVDBSeriesMedia(void);
    void ReadMedia(xmlDoc *doc, xmlNode *nodeBanners);
    void Store(int tvID, cTVScraperDB *db);
    void Dump(bool verbose);
};

#endif //__TVSCRAPER_TVDBMEDIA_H

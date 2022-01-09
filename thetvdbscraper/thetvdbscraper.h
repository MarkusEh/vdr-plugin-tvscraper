#ifndef __TVSCRAPER_TVDBSCRAPER_H
#define __TVSCRAPER_TVDBSCRAPER_H

using namespace std;

// --- cTVDBScraper -------------------------------------------------------------

class cTVDBScraper {
private:
    string apiKey;
    string baseURL;
    string baseDir;
    string language;
    cTVScraperDB *db;
    cTVDBMirrors *mirrors;
    bool ReadAll(int seriesID, cTVDBSeries *&series, cTVDBActors *&actors, cTVDBSeriesMedia *&media, bool onlyEpisodes);
    void StoreMedia(cTVDBSeries *series, cTVDBSeriesMedia *media, cTVDBActors *actors);
public:
    cTVDBScraper(string baseDir, cTVScraperDB *db, string language);
    virtual ~cTVDBScraper(void);
    bool Connect(void);
    cTVDBMirrors *GetMirrors(void) { return mirrors; }
    const string GetLanguage(void) { return language; }
    int StoreSeries(int seriesID, bool onlyEpisodes);
    void StoreStill(int seriesID, int seasonNumber, int episodeNumber, const string &episodeFilename);
};


#endif //__TVSCRAPER_TVDBSCRAPER_H

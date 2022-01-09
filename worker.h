#ifndef __TVSCRAPER_WORKER_H
#define __TVSCRAPER_WORKER_H

#include "tools/searchResultTvMovie.h"

// --- cTVScraperWorker -------------------------------------------------------------

class cTVScraperWorker : public cThread {
private:
    bool startLoop;
    bool scanVideoDir;
    bool manualScan;
    string language;
    string plgBaseDir;
    string plgConfDir;
    string seriesDir;
    string movieDir;
    cOverRides *overrides;
    int initSleep;
    int loopSleep;
    cCondVar waitCondition;
    cMutex mutex;
    cTVScraperDB *db;
    cMovieDBScraper *moviedbScraper;
    cTVDBScraper *tvdbScraper;
    map<string, sMovieOrTv> cache;
    map<string, searchResultTvMovie> cacheTv;
    scrapType GetScrapType(const cEvent *event);
    bool ConnectScrapers(void);
    void DisconnectScrapers(void);
    void CheckRunningTimers(void);
    void ScrapEPG(void);
    void ScrapRecordings(void);
    bool StartScrapping(void);
    bool Scrap(const cEvent *event, const cRecording *recording);
    scrapType Search(cTVDBSeries *TVtv, cMovieDbTv *tv, cMovieDbMovie *movie, const string &name, scrapType type_override, searchResultTvMovie &searchResult);
    void FindBestResult(const vector<searchResultTvMovie> &resultSet, searchResultTvMovie &searchResult);
    bool FindBestResult2(vector<searchResultTvMovie> &resultSet, searchResultTvMovie &searchResult);
    bool ScrapFindAndStore(sMovieOrTv &movieOrTv, const cEvent *event, const cRecording *recording);
    void Scrap_assign(const sMovieOrTv &movieOrTv, const cEvent *event, const cRecording *recording);
public:
    void UpdateEpisodeListIfRequired(int tvID);
    cTVScraperWorker(cTVScraperDB *db, cOverRides *overrides);
    virtual ~cTVScraperWorker(void);
    void SetLanguage(void);
    void SetDirectories(void);
    void InitVideoDirScan(void);
    void InitManualScan(void);
    virtual void Action(void);
    void Stop(void);
};


#endif //__TVSCRAPER_WORKER_H

#ifndef __TVSCRAPER_WORKER_H
#define __TVSCRAPER_WORKER_H
#include <set>

// --- cTVScraperWorker -------------------------------------------------------------

class cTVScraperWorker : public cThread {
private:
    bool startLoop;
    bool scanVideoDir;
    bool manualScan;
    cOverRides *overrides;
    int initSleep;
    int loopSleep;
    time_t lastTimerRecordingCheck;
    cCondVar waitCondition;
    cMutex mutex;
    cTVScraperDB *db;
    cMovieDBScraper *moviedbScraper;
    cTVDBScraper *tvdbScraper;
#if APIVERSNUM >= 20301
    cStateKey schedulesStateKey;
#endif
    map<tChannelID, set<tEventID>*> lastEvents;

    bool ConnectScrapers(void);
    void DisconnectScrapers(void);
    bool CheckRunningTimers(void);
    bool ScrapEPG(void);
    void ScrapRecordings(void);
    bool StartScrapping(bool &fullScan);
    bool TimersRunningPlanned(double nextMinutes);
public:
    cTVScraperWorker(cTVScraperDB *db, cOverRides *overrides);
    virtual ~cTVScraperWorker(void);
    void SetDirectories(void);
    void InitVideoDirScan(void);
    void InitManualScan(void);
    virtual void Action(void);
    void Stop(void);
};


#endif //__TVSCRAPER_WORKER_H

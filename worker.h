#ifndef __TVSCRAPER_WORKER_H
#define __TVSCRAPER_WORKER_H
#include <set>

// --- cTVScraperWorker -------------------------------------------------------------

class cTVScraperWorker : public cThread {
  private:
    bool startLoop;
    bool scanVideoDir;
    std::string m_recording;
    bool manualScan;
    bool backup_requested = true;
    cOverRides *overrides;
    int initSleep;
    int loopSleep;
    time_t lastTimerRecordingCheck;
    time_t lastScrapeRecordings = 0;
    cCondVar waitCondition;
    cMutex mutex;
    cTVScraperDB *db;
    cMovieDBScraper *moviedbScraper;
    cTVDBScraper *tvdbScraper;
    cMovieDbMovieScraper *m_movieDbMovieScraper;
    cMovieDbTvScraper *m_movieDbTvScraper;
    cTvDbTvScraper *m_tvDbTvScraper;
#if APIVERSNUM >= 20301
    cStateKey schedulesStateKey;
#endif
    map<tChannelID, set<tEventID>*> lastEvents;
    cCurl m_curl;

    bool ConnectScrapers(void);
    void DisconnectScrapers(void);
//    bool CheckRunningTimers(void);
    bool ScrapEPG(void);
    cMovieOrTv *ScrapRecording(const cRecording *recording);
//     void ScrapRecordings(void);
    void ScrapChangedRecordings();
    void ScrapRecordings(const std::vector<std::string> &recordingFileNames);
    bool StartScrapping(bool &fullScan);
    bool TimersRunningPlanned(double nextMinutes);
public:
    cTVScraperWorker(cTVScraperDB *db, cOverRides *overrides);
    virtual ~cTVScraperWorker(void);
    void SetDirectories(void);
    void InitVideoDirScan(const char *recording = nullptr);
    void InitManualScan(void);
    virtual void Action(void);
    void Stop(void);
    bool Running_() { return Running(); }
};


#endif //__TVSCRAPER_WORKER_H

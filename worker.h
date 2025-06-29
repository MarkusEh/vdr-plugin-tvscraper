#ifndef __TVSCRAPER_WORKER_H
#define __TVSCRAPER_WORKER_H
#include <set>

// --- cTVScraperWorker -------------------------------------------------------------

class cEpgImage {
  friend class cEpgImages;
  public:
    cEpgImage(time_t event_start, cSv url, cSv local_path):
      m_event_start(event_start), m_url(url), m_local_path(local_path) {}
    bool isOutdated() { return m_event_start <= time(NULL); }
  private:
    time_t m_event_start;
    std::string m_url;
    std::string m_local_path;
    int m_failed_downloads = 0;
    int download(cCurl *curl); // return 0 in case of success. Otherwise, number of failed downloads
};
class cEpgImages {
  public:
    cEpgImages(cCurl *curl, int epgImageDownloadSleep): m_curl(curl), m_epgImageDownloadSleep(epgImageDownloadSleep) {}
    void add(time_t event_start, cSv url, cStr local_path) {
      if (!FileExistsImg(local_path)) m_epg_images.emplace_back(event_start, url, local_path); }
    void downloadOne();

  private:
    cCurl *m_curl;
    const int m_epgImageDownloadSleep;
    time_t m_last_download = 0;
    time_t m_last_report = 0;
    std::vector<cEpgImage> m_epg_images;
};
class cTVScraperWorker : public cThread {
  private:
    cCurl m_curl;
    const int m_epgImageDownloadSleep;
    const int m_epgImageDownloadIterations;
    cEpgImages m_epg_images;
    bool startLoop;
    bool scanVideoDir;
    bool manualScan;
    bool backup_requested = true;
    cOverRides *overrides;
    int initSleep;
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

    bool ConnectScrapers(void);
    void DisconnectScrapers(void);
    void CheckRunningTimers();
    bool ScrapEPG(void);
    cMovieOrTv *ScrapRecording(const cRecording *recording);
    void ScrapChangedRecordings();
    void ScrapRecordings(const std::vector<std::string> &recordingFileNames);
    bool StartScrapping(bool &fullScan);
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

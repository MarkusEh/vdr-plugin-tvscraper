#ifndef __TVSCRAPER_TVDBSCRAPER_H
#define __TVSCRAPER_TVDBSCRAPER_H

using namespace std;

// --- cTVDBScraper -------------------------------------------------------------

class cTVDBScraper {
    friend class cTVDBSeries;
private:
    string apiKey;
    string apiKey4;
    string baseURL;
    string baseURL4;
    string baseURL4Search;
    string tokenHeader;
    time_t tokenHeaderCreated = 0;
    string baseDir;
    string language;
    cTVScraperDB *db;
    cTVDBMirrors *mirrors;
    bool ReadAll(int seriesID, cTVDBSeries *&series, cTVDBActors *&actors, cTVDBSeriesMedia *&media, bool onlyEpisodes);
    void StoreMedia(cTVDBSeries *series, cTVDBSeriesMedia *media, cTVDBActors *actors);
    bool GetToken(const std::string &jsonResponse);
public:
    cTVDBScraper(string baseDir, cTVScraperDB *db, string language);
    virtual ~cTVDBScraper(void);
    bool Connect(void);
    bool GetToken();
    cTVDBMirrors *GetMirrors(void) { return mirrors; }
    const string GetLanguage(void) { return language; }
    int StoreSeries(int seriesID, bool onlyEpisodes);
    void StoreStill(int seriesID, int seasonNumber, int episodeNumber, const string &episodeFilename);
    void StoreActors(int seriesID);
    vector<vector<string>> GetTvRuntimes(int seriesID);
    void GetTvVote(int seriesID, float &vote_average, int &vote_count);
    void DownloadMedia (int tvID);
    void DownloadMedia (int tvID, eMediaType mediaType, const string &destDir, const string &baseUrl);
    void DownloadMediaBanner (int tvID, const string &destPath, const string &baseUrl);
};


#endif //__TVSCRAPER_TVDBSCRAPER_H

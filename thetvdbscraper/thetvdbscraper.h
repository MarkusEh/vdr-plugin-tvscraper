#ifndef __TVSCRAPER_TVDBSCRAPER_H
#define __TVSCRAPER_TVDBSCRAPER_H

using namespace std;

// --- cTVDBScraper -------------------------------------------------------------

class cTVDBScraper {
    friend class cTVDBSeries;
private:
    string baseURL4;
    string baseURL4Search;
    string tokenHeader;
    time_t tokenHeaderCreated = 0;
    string baseDir;
    cTVScraperDB *db;
    json_t *CallRestJson(const std::string &url);
    bool GetToken(const std::string &jsonResponse);
    void ParseJson_searchSeries(json_t *data, vector<searchResultTvMovie> &resultSet, const string &SearchStringStripExtraUTF8, const cLanguage *lang);
    bool ParseJson_search(json_t *root, vector<searchResultTvMovie> &resultSet, const string &SearchString, const cLanguage *lang);
    static const char *prefixImageURL1;
    static const char *prefixImageURL2;
public:
    cTVDBScraper(string baseDir, cTVScraperDB *db);
    virtual ~cTVDBScraper(void);
    bool Connect(void);
    bool GetToken();
    int StoreSeriesJson(int seriesID, bool onlyEpisodes);
    int StoreSeriesJson(int seriesID, const cLanguage *lang);
    void StoreStill(int seriesID, int seasonNumber, int episodeNumber, const string &episodeFilename);
    void StoreActors(int seriesID);
    vector<vector<string>> GetTvRuntimes(int seriesID);
//    void GetTvVote(int seriesID, float &vote_average, int &vote_count);
    int GetTvScore(int seriesID);
    void DownloadMedia (int tvID);
    void DownloadMedia (int tvID, eMediaType mediaType, const string &destDir);
    void DownloadMediaBanner (int tvID, const string &destPath);
    bool AddResults4(vector<searchResultTvMovie> &resultSet, const string &SearchString, const string &SearchString_ext, const cLanguage *lang);
    static const char *getDbUrl(const char *url);
    static std::string getFullDownloadUrl(const char *url);
    void Download4(const char *url, const std::string &localPath);
};
#endif //__TVSCRAPER_TVDBSCRAPER_H

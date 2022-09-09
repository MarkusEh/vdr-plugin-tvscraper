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
    string language;
    cTVScraperDB *db;
    json_t *CallRestJson(const std::string &url);
    bool GetToken(const std::string &jsonResponse);
    void ParseJson_searchSeries(json_t *data, vector<searchResultTvMovie> &resultSet, const string &SearchStringStripExtraUTF8);
    bool ParseJson_search(json_t *root, vector<searchResultTvMovie> &resultSet, const string &SearchString);
public:
    cTVDBScraper(string baseDir, cTVScraperDB *db, string language);
    virtual ~cTVDBScraper(void);
    bool Connect(void);
    bool GetToken();
    const string GetLanguage(void) { return language; }
    int StoreSeriesJson(int seriesID, bool onlyEpisodes);
    void StoreStill(int seriesID, int seasonNumber, int episodeNumber, const string &episodeFilename);
    void StoreActors(int seriesID);
    vector<vector<string>> GetTvRuntimes(int seriesID);
//    void GetTvVote(int seriesID, float &vote_average, int &vote_count);
    int GetTvScore(int seriesID);
    void DownloadMedia (int tvID);
    void DownloadMedia (int tvID, eMediaType mediaType, const string &destDir);
    void DownloadMediaBanner (int tvID, const string &destPath);
    bool AddResults4(vector<searchResultTvMovie> &resultSet, const string &SearchString, const string &SearchString_ext);
};
#endif //__TVSCRAPER_TVDBSCRAPER_H

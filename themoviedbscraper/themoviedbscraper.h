#ifndef __TVSCRAPER_MOVIEDBSCRAPER_H
#define __TVSCRAPER_MOVIEDBSCRAPER_H

struct searchResultTvMovie;

// --- cMovieDBScraper -------------------------------------------------------------

class cMovieDBScraper {
  private:
    const char *apiKey = "abb01b5a277b9c2c60ec0302d83c5ee9";
    const char *baseURL = "api.themoviedb.org/3";
    const char *posterSize = "w780";
    const char *stillSize = "w300";
    const char *backdropSize = "w1280";
    const char *actorthumbSize = "h632";
    std::string m_posterBaseUrl;
    std::string m_backdropBaseUrl;
    std::string m_stillBaseUrl;
    std::string m_actorsBaseUrl;
    cTVScraperDB *db;
    cOverRides *overrides;
    bool parseJSON(const rapidjson::Value &root);
    bool AddTvResults(cLargeString &buffer, vector<searchResultTvMovie> &resultSet, string_view tvSearchString, const std::vector<cNormedString> &normedStrings, const cLanguage *lang);
    void AddMovieResults(const rapidjson::Document &root, vector<searchResultTvMovie> &resultSet, const std::vector<cNormedString> &normedStrings, const char *description, bool setMinTextMatch);
    int AddMovieResultsForUrl(cLargeString &buffer, const char *url, vector<searchResultTvMovie> &resultSet, const std::vector<cNormedString> &normedStrings, const char *description, bool setMinTextMatch);
    void AddMovieResults(cLargeString &buffer, vector<searchResultTvMovie> &resultSet, std::string_view SearchString, const std::vector<cNormedString> &normedStrings, const char *description, bool setMinTextMatch, const cYears &years, const cLanguage *lang);

public:
    cMovieDBScraper(cTVScraperDB *db, cOverRides *overrides);
    virtual ~cMovieDBScraper(void);
    bool Connect(void);
    const char* GetApiKey(void) { return apiKey; }
    void StoreMovie(int movieID, bool forceUpdate = false);
    bool DownloadFile(const string &urlBase, const string &urlFileName, const string &destDir, int destID, const char * destFileName, bool movie);
    void DownloadMedia(int movieID);
    void DownloadMediaTv(int tvID);
    void DownloadActors(int tvID, bool movie);
    void StoreStill(int tvID, int seasonNumber, int episodeNumber, const char *stillPathTvEpisode);
    int GetMovieRuntime(int movieID);
    void UpdateTvRuntimes(int tvID);
    bool AddTvResults(vector<searchResultTvMovie> &resultSet, string_view tvSearchString, const cLanguage *lang);
    void AddMovieResults(vector<searchResultTvMovie> &resultSet, std::string_view SearchString, const char *description, const cYears &years, const cLanguage *lang);
};

#endif //__TVSCRAPER_MOVIEDBSCRAPER_H

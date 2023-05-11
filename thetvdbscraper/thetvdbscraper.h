#ifndef __TVSCRAPER_TVDBSCRAPER_H
#define __TVSCRAPER_TVDBSCRAPER_H

#include "../extMovieTvDb.h"
using namespace std;

// --- cTVDBScraper -------------------------------------------------------------

class cTVDBScraper {
    friend class cTVDBSeries;
    friend class cTvDbTvScraper;
  private:
    const char *baseURL4 = "https://api4.thetvdb.com/v4/";
    const char *baseURL4Search = "https://api4.thetvdb.com/v4/search?type=series&query=";
    std::string tokenHeader;
    time_t tokenHeaderCreated = 0;
    cTVScraperDB *db;
    int CallRestJson(rapidjson::Document &document, const rapidjson::Value *&data, cLargeString &buffer, const char *url, bool disableLog = false);
    bool GetToken(std::string &jsonResponse);
    bool AddResults4(cLargeString &buffer, vector<searchResultTvMovie> &resultSet, std::string_view SearchString, const std::vector<std::optional<cNormedString>> &normedStrings, const cLanguage *lang);
    void ParseJson_searchSeries(const rapidjson::Value &data, vector<searchResultTvMovie> &resultSet, const std::vector<std::optional<cNormedString>> &normedStrings, const cLanguage *lang);
    int StoreSeriesJson(int seriesID, bool forceUpdate);
    int StoreSeriesJson(int seriesID, const cLanguage *lang);
    void StoreStill(int seriesID, int seasonNumber, int episodeNumber, const char *episodeFilename);
    void StoreActors(int seriesID);
    void DownloadMedia (int tvID);
    void DownloadMedia (int tvID, eMediaType mediaType, const string &destDir);
    void DownloadMediaBanner (int tvID, const string &destPath);
//    bool AddResults4(vector<searchResultTvMovie> &resultSet, std::string_view SearchString, const cLanguage *lang);
    static const char *getDbUrl(const char *url);
    void download(const char *url, const char *localPath);

    static const char *prefixImageURL1;
    static const char *prefixImageURL2;
  public:
    cTVDBScraper(cTVScraperDB *db);
    virtual ~cTVDBScraper(void);
    bool Connect(void);
    bool GetToken();
    cMeasureTime apiCalls;
};


class cTvDbTvScraper: public iExtMovieTvDb {
  public:
    cTvDbTvScraper(cTVDBScraper *TVDBScraper): m_TVDBScraper(TVDBScraper) {}
    virtual int download(int id, bool forceUpdate = false) {
      return m_TVDBScraper->StoreSeriesJson(id, forceUpdate);
    }
    virtual int download(int id, const cLanguage *lang) {
      if (config.isDefaultLanguage(lang)) return download(id, false);
      return m_TVDBScraper->StoreSeriesJson(id, lang);
    }

    virtual void enhance1(int id) {
// note: we assume, that during the time tv_score was written, also tv_episode_run_time
//       was written. So no extra check for tv_episode_run_time
//       same for series_actors
// done / checked: series_actors: not deleted during delete of series. After that: series is again loaded. What happens?
//   cTVScraperDB::InsertActor(seriesID, name, role, path)
//   checks if seriesID, name, role already exists in db.
//   If yes, it will just download path (if the file is not available)
      cSql stmt(m_TVDBScraper->db, "select 1 from tv_score where tv_id = ?", -1*id);
      if (stmt.readRow() ) return;
      download(id, true);
    }

    virtual int downloadImages(int id, int seasonNumber, int episodeNumber) {
      m_TVDBScraper->DownloadMedia(id);
      m_TVDBScraper->StoreActors(id);
      if (!seasonNumber && !episodeNumber) return 0;
      cSql stmt(m_TVDBScraper->db, "SELECT episode_still_path FROM tv_s_e WHERE tv_id = ? and season_number = ? and episode_number = ?");
      const char *episodeStillPath = stmt.resetBindStep(-1*id, seasonNumber, episodeNumber).getCharS(0);
      if (episodeStillPath && *episodeStillPath)
        m_TVDBScraper->StoreStill(id, seasonNumber, episodeNumber, episodeStillPath);
      return 0;
    }
    virtual void addSearchResults(cLargeString &buffer, vector<searchResultTvMovie> &resultSet, std::string_view searchString, bool isFullSearchString, const std::vector<std::optional<cNormedString>> &normedStrings, const char *description, const cYears &years, const cLanguage *lang) {
      m_TVDBScraper->AddResults4(buffer, resultSet, searchString, normedStrings, lang);
    }
  private:
    cTVDBScraper *m_TVDBScraper;
};
#endif //__TVSCRAPER_TVDBSCRAPER_H

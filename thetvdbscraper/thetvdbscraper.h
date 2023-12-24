#ifndef __TVSCRAPER_TVDBSCRAPER_H
#define __TVSCRAPER_TVDBSCRAPER_H

#include "../extMovieTvDb.h"
using namespace std;

std::string displayLanguageTvdb(cSv translations, const char *originalLanguage);
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
    int CallRestJson(cJsonDocumentFromUrl &document, const rapidjson::Value *&data, const char *url, bool disableLog = false);
    int CallRestJson_int(cJsonDocumentFromUrl &document, const rapidjson::Value *&data, const char *url, bool disableLog);
    bool GetToken(std::string &jsonResponse);
    bool AddResults4(vector<searchResultTvMovie> &resultSet, cSv SearchString, const cCompareStrings &compareStrings, const cLanguage *lang);
    void ParseJson_searchSeries(const rapidjson::Value &data, vector<searchResultTvMovie> &resultSet, const cCompareStrings &compareStrings, const cLanguage *lang);
    int StoreSeriesJson(int seriesID, bool forceUpdate);
    int downloadEpisodes(int seriesID, bool forceUpdate, const cLanguage *lang, bool langIsIntendedDisplayLanguage = false, const cLanguage **displayLanguage = nullptr);
    void StoreStill(int seriesID, int seasonNumber, int episodeNumber, const char *episodeFilename);
    void StoreActors(int seriesID);
    void DownloadMedia (int tvID);
    void DownloadMedia (int tvID, eMediaType mediaType, const string &destDir);
    void DownloadMediaBanner (int tvID, const string &destPath);
//    bool AddResults4(vector<searchResultTvMovie> &resultSet, cSv SearchString, const cLanguage *lang);
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

    virtual int download(int id) {
      return m_TVDBScraper->StoreSeriesJson(id, false);
    }
    virtual int downloadEpisodes(int id, const cLanguage *lang) {
      int res = m_TVDBScraper->downloadEpisodes(id, false, lang);
//      if (res == 1 && config.enableDebug) esyslog("tvscraper: cTvDbTvScraper::downloadEpisodes lang %s not available, id %i",  lang->getNames().c_str(), id);
      return res;
    }

    virtual void enhance1(searchResultTvMovie &searchResultTvMovie, const cLanguage *lang) {
// note: we assume, that during the time tv_score was written, also tv_episode_run_time
//       was written. So no extra check for tv_episode_run_time
// done / checked: series_actors: not deleted during delete of series. After that: series is again loaded. What happens?
//   cTVScraperDB::InsertActor(seriesID, name, role, path)
//   checks if seriesID, name, role already exists in db.
//   If yes, it will just download path (if the file is not available)
      cSql stmt(m_TVDBScraper->db,
        "SELECT tv_languages, tv_languages_last_update, tv_actors_last_update, tv_data_available integer FROM tv_score WHERE tv_id = ?", searchResultTvMovie.id() );
      if (!stmt.readRow() || stmt.valueInitial(1) || stmt.valueInitial(2) || stmt.getInt(3) < 5) {
// no cache at all, or missing actors => complete download
        if (config.enableDebug) isyslog("tvscraper: cTvDbTvScraper::enhance1 force update, reason %s, lang %s, seriesID %i",
          stmt.readRow()?"actors missing":"does not exist", lang->getNames().c_str(), searchResultTvMovie.id() );

        m_TVDBScraper->StoreSeriesJson(-searchResultTvMovie.id(), true);
        return;
      }
      bool update_required = config.isUpdateFromExternalDbRequired(stmt.getInt64(1));
      if (!update_required && config.isUpdateFromExternalDbRequiredMR(stmt.getInt64(1) )){
// check: do we have a runtime?
        bool found = false;
        for (cSql &sql_rt: cSql(m_TVDBScraper->db, "SELECT episode_run_time FROM tv_episode_run_time WHERE tv_id = ?", searchResultTvMovie.id() )) {
          if (sql_rt.getInt(0) > 0) {
            found = true;
            break;
          }
        }
        if (!found) update_required = true; // no runtime found
      }

      if (!update_required) return;
// we also need to update the episode runtimes, so update regularily
      int res = m_TVDBScraper->downloadEpisodes(-searchResultTvMovie.id(), true, lang); // this will not update the actors, which is not required here
      if (res == 1 && config.enableDebug) esyslog("tvscraper: cTvDbTvScraper::enhance1 lang %s not available, id %i",  lang->getNames().c_str(), searchResultTvMovie.id() );
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
    virtual void addSearchResults(vector<searchResultTvMovie> &resultSet, cSv searchString, bool isFullSearchString, const cCompareStrings &compareStrings, const char *shortText, const char *description, const cYears &years, const cLanguage *lang) {
      m_TVDBScraper->AddResults4(resultSet, searchString, compareStrings, lang);
    }
  private:
    cTVDBScraper *m_TVDBScraper;
};
#endif //__TVSCRAPER_TVDBSCRAPER_H

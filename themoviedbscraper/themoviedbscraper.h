#ifndef __TVSCRAPER_MOVIEDBSCRAPER_H
#define __TVSCRAPER_MOVIEDBSCRAPER_H

#include "../extMovieTvDb.h"

struct searchResultTvMovie;

// --- cMovieDBScraper -------------------------------------------------------------

class cMovieDBScraper {
  friend class cMovieDbMovieScraper;
  friend class cMovieDbTvScraper;
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
    bool AddTvResults(vector<searchResultTvMovie> &resultSet, cSv tvSearchString, const cCompareStrings &compareStrings, const cLanguage *lang);
    void AddMovieResults(const rapidjson::Document &root, vector<searchResultTvMovie> &resultSet, const cCompareStrings &compareStrings, const char *description, bool setMinTextMatch, const cLanguage *lang);
    int AddMovieResultsForUrl(const char *url, vector<searchResultTvMovie> &resultSet, const cCompareStrings &compareStrings, const char *description, bool setMinTextMatch, const cLanguage *lang);
    void AddMovieResults(vector<searchResultTvMovie> &resultSet, cSv SearchString, const cCompareStrings &compareStrings, const char *description, bool setMinTextMatch, const cYears &years, const cLanguage *lang);

    void StoreMovie(int movieID, bool forceUpdate = false);
    bool DownloadFile(const string &urlBase, const string &urlFileName, const string &destDir, int destID, const char * destFileName, bool movie);
    void DownloadMedia(int movieID);
    void DownloadMediaTv(int tvID);
    void DownloadActors(int tvID, bool movie);
    void StoreStill(int tvID, int seasonNumber, int episodeNumber, const char *stillPathTvEpisode);
//    bool AddTvResults(vector<searchResultTvMovie> &resultSet, cSv tvSearchString, const cLanguage *lang);
//    void AddMovieResults(vector<searchResultTvMovie> &resultSet, cSv SearchString, const char *description, const cYears &years, const cLanguage *lang);
public:
    cMovieDBScraper(cTVScraperDB *db, cOverRides *overrides);
    virtual ~cMovieDBScraper(void);
    bool Connect(void);
    const char* GetApiKey(void) { return apiKey; }
    cMeasureTime apiCalls;
};

class cMovieDbMovieScraper: public iExtMovieTvDb {
  public:
    cMovieDbMovieScraper(cMovieDBScraper *movieDBScraper): m_movieDBScraper(movieDBScraper) {}

    virtual int download(int id) {
      m_movieDBScraper->StoreMovie(id, false);
      return 0;
    }
    virtual int downloadEpisodes(int id, const cLanguage *lang) { return 0; }

    virtual void enhance1(searchResultTvMovie &searchResultTvMovie, const cLanguage *lang) {
      int runtime = m_movieDBScraper->db->GetMovieRuntime(searchResultTvMovie.id() );
      if (runtime  >  0 || runtime == -1) return;
// themoviedb never checked for runtime, check now
      m_movieDBScraper->StoreMovie(searchResultTvMovie.id(), true);
    }

    virtual int downloadImages(int id, int seasonNumber = 0, int episodeNumber = 0) {
      m_movieDBScraper->DownloadMedia(id);
      m_movieDBScraper->DownloadActors(id, true);
      return 0;
    }
    virtual void addSearchResults(vector<searchResultTvMovie> &resultSet, cSv searchString, bool isFullSearchString, const cCompareStrings &compareStrings, const char *description, const cYears &years, const cLanguage *lang) {
      m_movieDBScraper->AddMovieResults(resultSet, searchString, compareStrings, description, isFullSearchString, years, lang);
    }
  private:
    cMovieDBScraper *m_movieDBScraper;
};
class cMovieDbTvScraper: public iExtMovieTvDb {
  public:
    cMovieDbTvScraper(cMovieDBScraper *movieDBScraper): m_movieDBScraper(movieDBScraper) {}

    virtual int download(int id) {
      cMovieDbTv tv(m_movieDBScraper->db, m_movieDBScraper);
      tv.SetTvID(id);
      tv.UpdateDb(false);
      return 0;
    }

    virtual int downloadEpisodes(int id, const cLanguage *lang) {
      if (!config.isDefaultLanguage(lang)) return 0; // language dependent texts in episodes in themoviedb are not implemented ...
      cMovieDbTv tv(m_movieDBScraper->db, m_movieDBScraper);
      tv.SetTvID(id);
      tv.UpdateDb(false);
      return 0;
    }

    virtual void enhance1(searchResultTvMovie &searchResultTvMovie, const cLanguage *lang) {
      cSql stmt(m_movieDBScraper->db, "select 1 from tv_episode_run_time where tv_id = ?", searchResultTvMovie.id() );
      if (stmt.readRow() ) return;
      cMovieDbTv tv(m_movieDBScraper->db, m_movieDBScraper);
      tv.SetTvID(searchResultTvMovie.id() );
      tv.UpdateDb(true);
    }

    virtual int downloadImages(int id, int seasonNumber, int episodeNumber) {
      m_movieDBScraper->DownloadMediaTv(id);
      m_movieDBScraper->DownloadActors(id, false);
      if (!seasonNumber && !episodeNumber) return 0;
      cSql stmt(m_movieDBScraper->db, "SELECT episode_still_path FROM tv_s_e WHERE tv_id = ? and season_number = ? and episode_number = ?");
      const char *episodeStillPath = stmt.resetBindStep(id, seasonNumber, episodeNumber).getCharS(0);
      if (episodeStillPath && *episodeStillPath)
        m_movieDBScraper->StoreStill(id, seasonNumber, episodeNumber, episodeStillPath);
      return 0;
    }
    virtual void addSearchResults(vector<searchResultTvMovie> &resultSet, cSv searchString, bool isFullSearchString, const cCompareStrings &compareStrings, const char *description, const cYears &years, const cLanguage *lang) {
      m_movieDBScraper->AddTvResults(resultSet, searchString, compareStrings, lang);
    }
  private:
    cMovieDBScraper *m_movieDBScraper;
};

#endif //__TVSCRAPER_MOVIEDBSCRAPER_H

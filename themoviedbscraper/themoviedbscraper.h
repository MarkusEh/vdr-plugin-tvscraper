#ifndef __TVSCRAPER_MOVIEDBSCRAPER_H
#define __TVSCRAPER_MOVIEDBSCRAPER_H

#include "../extMovieTvDb.h"

struct searchResultTvMovie;

// --- cMovieDBScraper -------------------------------------------------------------

class cMovieDBScraper {
  friend class cMovieDbMovieScraper;
  friend class cMovieDbMovie;
  friend class cMovieDbTvScraper;
  friend class cMovieDbTv;
  private:
    const char *apiKey = "abb01b5a277b9c2c60ec0302d83c5ee9";
    const char *baseURL = "api.themoviedb.org/3";
    const char *posterSize = "original";
    const char *stillSize = "original";
    const char *backdropSize = "original";
    const char *actorthumbSize = "original";
    std::string m_posterBaseUrl;
    std::string m_backdropBaseUrl;
    std::string m_stillBaseUrl;
    std::string m_actorsBaseUrl;
    cTVScraperDB *db;
    cCurl *m_curl;
    cOverRides *overrides;
    bool parseJSON(const rapidjson::Value &root);
    bool AddTvResults(vector<searchResultTvMovie> &resultSet, cSv tvSearchString, const cCompareStrings &compareStrings, const cLanguage *lang);
    void AddMovieResults(const rapidjson::Document &root, vector<searchResultTvMovie> &resultSet, const cCompareStrings &compareStrings, const char *shortText, cSv description, bool setMinTextMatch, const cLanguage *lang);
    int AddMovieResultsForUrl(const char *url, vector<searchResultTvMovie> &resultSet, const cCompareStrings &compareStrings, const char *shortText, cSv description, bool setMinTextMatch, const cLanguage *lang);
    void AddMovieResults(vector<searchResultTvMovie> &resultSet, cSv SearchString, const cCompareStrings &compareStrings, const char *shortText, cSv description, bool setMinTextMatch, const cYears &years, const cLanguage *lang);

    void StoreMovie(int movieID, bool forceUpdate = false);
    bool DownloadFile(cSv urlBase, const cSv urlFileName, cSv destDir, int destID, const char * destFileName, bool movie);
    void DownloadMedia(int movieID);
    void DownloadMediaTv(int tvID);
    void DownloadActors(int tvID, bool movie);
    void StoreStill(int tvID, int seasonNumber, int episodeNumber, const char *stillPathTvEpisode);
public:
    cMovieDBScraper(cCurl *curl, cTVScraperDB *db, cOverRides *overrides);
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
      cSql sql_data_available(m_movieDBScraper->db, "SELECT movie_last_update, movie_data_available, movie_runtime FROM movie_runtime2 WHERE movie_id = ?", searchResultTvMovie.id() );
      if (!sql_data_available.readRow() || sql_data_available.valueInitial(0) ||
          sql_data_available.getInt(1) < 4 || config.isUpdateFromExternalDbRequired(sql_data_available.get<time_t>(0) ))
        m_movieDBScraper->StoreMovie(searchResultTvMovie.id(), true);
      else if (config.isUpdateFromExternalDbRequiredMR(sql_data_available.get<time_t>(0) ) &&
               sql_data_available.getInt(2) <= 0) m_movieDBScraper->StoreMovie(searchResultTvMovie.id(), true);
    }

    virtual int downloadImages(int id, int seasonNumber = 0, int episodeNumber = 0) {
      config.timeDownloadMedia.start();
      m_movieDBScraper->DownloadMedia(id);
      config.timeDownloadMedia.stop();
      config.timeDownloadActorsMovie.start();
      m_movieDBScraper->DownloadActors(id, true);
      config.timeDownloadActorsMovie.stop();
      return 0;
    }
    virtual void addSearchResults(vector<searchResultTvMovie> &resultSet, cSv searchString, bool isFullSearchString, const cCompareStrings &compareStrings, const char *shortText, cSv description, const cYears &years, const cLanguage *lang, int network_id) {
      m_movieDBScraper->AddMovieResults(resultSet, searchString, compareStrings, shortText, description, isFullSearchString, years, lang);
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
      cMovieDbTv tv(m_movieDBScraper->db, m_movieDBScraper);
      tv.SetTvID(id);
      return tv.downloadEpisodes(false, lang);
    }

    virtual void enhance1(searchResultTvMovie &searchResultTvMovie, const cLanguage *lang) {
      cMovieDbTv tv(m_movieDBScraper->db, m_movieDBScraper);
      tv.SetTvID(searchResultTvMovie.id() );
      tv.downloadEpisodes(false, lang);
      cSql sql_data_available(m_movieDBScraper->db, "SELECT tv_actors_last_update, tv_data_available FROM tv_score WHERE tv_id = ?", searchResultTvMovie.id() );
      if (!sql_data_available.readRow() || sql_data_available.valueInitial(0) ||
          sql_data_available.getInt(1) < 5 || config.isUpdateFromExternalDbRequired(sql_data_available.get<time_t>(0) ))
        tv.UpdateDb(true);
      else if (config.isUpdateFromExternalDbRequiredMR(sql_data_available.get<time_t>(0) ) &&
              !m_movieDBScraper->db->TvRuntimeAvailable(searchResultTvMovie.id()) ) {
        tv.UpdateDb(true);
      }
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
    virtual void addSearchResults(vector<searchResultTvMovie> &resultSet, cSv searchString, bool isFullSearchString, const cCompareStrings &compareStrings, const char *shortText, cSv description, const cYears &years, const cLanguage *lang, int network_id) {
      m_movieDBScraper->AddTvResults(resultSet, searchString, compareStrings, lang);
    }
  private:
    cMovieDBScraper *m_movieDBScraper;
};

#endif //__TVSCRAPER_MOVIEDBSCRAPER_H

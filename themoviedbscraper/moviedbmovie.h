#ifndef __TVSCRAPER_MOVIEDBMOVIE_H
#define __TVSCRAPER_MOVIEDBMOVIE_H

// --- cMovieDbMovie -------------------------------------------------------------

class cMovieDbMovie {
  private:
    cTVScraperDB *m_db;
    cMovieDBScraper *m_movieDBScraper;
    const char *m_baseURL = "api.themoviedb.org/3";
    bool ReadMovie(const rapidjson::Value &movie, int id);
  public:
    cMovieDbMovie(cTVScraperDB *db, cMovieDBScraper *movieDBScraper);
    ~cMovieDbMovie(void) {}
    bool ReadAndStore(int id);
};

#endif //__TVSCRAPER_TVDBSERIES_H

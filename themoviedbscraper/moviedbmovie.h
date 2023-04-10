#ifndef __TVSCRAPER_MOVIEDBMOVIE_H
#define __TVSCRAPER_MOVIEDBMOVIE_H

using namespace std;

// --- cMovieDbMovie -------------------------------------------------------------

class cMovieDbMovie {
private:
    cTVScraperDB *m_db;
    cMovieDBScraper *m_movieDBScraper;
    const char *m_baseURL = "api.themoviedb.org/3";
    cLargeString m_buffer;
    rapidjson::Document m_document;
    int id;
    const char *title = NULL;
    const char *originalTitle = NULL;
    const char *tagline = NULL;
    const char *overview = NULL;
    bool adult;
    int collectionId;
    const char *collectionName = NULL;
    const char *collectionPosterPath = NULL;
    const char *collectionBackdropPath = NULL;
    int budget;
    int revenue;
    std::string genres;
    std::string productionCountries;
    const char *imdb_id = NULL;
    const char *homepage = NULL;
    const char *releaseDate = NULL;
    int runtime;
    float popularity;
    float voteAverage;
    int voteCount;

    const char *backdropPath = NULL;
    const char *posterPath = NULL;
    bool ReadMovie(const rapidjson::Value &movie);
public:
    cMovieDbMovie(cTVScraperDB *db, cMovieDBScraper *movieDBScraper);
    virtual ~cMovieDbMovie(void);
    bool ReadMovie(void);
    void SetID(int movieID) { id = movieID; };
    int ID(void) { return id; };
    void StoreDB(void);
    void StoreMedia();
};


#endif //__TVSCRAPER_TVDBSERIES_H

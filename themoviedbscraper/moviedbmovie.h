#ifndef __TVSCRAPER_MOVIEDBMOVIE_H
#define __TVSCRAPER_MOVIEDBMOVIE_H

using namespace std;

// --- cMovieDbMovie -------------------------------------------------------------

class cMovieDbMovie {
private:
    cTVScraperDB *m_db;
    cMovieDBScraper *m_movieDBScraper;
    const string m_baseURL = "api.themoviedb.org/3";
    int id;
    std::string title;
    std::string originalTitle;
    std::string tagline;
    std::string overview;
    bool adult;
    int collectionId;
    std::string collectionName;
    string collectionPosterPath;
    string collectionBackdropPath;
    int budget;
    int revenue;
    std::string genres;
    std::string homepage;
    std::string releaseDate;
    int runtime;
    float popularity;
    float voteAverage;
    int voteCount;

    string backdropPath;
    string posterPath;
    bool ReadMovie(json_t *movie);
    bool AddMovieResults(json_t *root, vector<searchResultTvMovie> &resultSet, const string &SearchString);
public:
    cMovieDbMovie(cTVScraperDB *db, cMovieDBScraper *movieDBScraper);
    virtual ~cMovieDbMovie(void);
    bool AddMovieResults(vector<searchResultTvMovie> &resultSet, const string &SearchString, const string &SearchString_ext, csEventOrRecording *sEventOrRecording);
    bool ReadMovie(void);
    void SetID(int movieID) { id = movieID; };
    int ID(void) { return id; };
    void StoreDB(void);
    void StoreMedia();
    void Dump();
};


#endif //__TVSCRAPER_TVDBSERIES_H

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
    std::string productionCountries;
    std::string imdb_id;
    std::string homepage;
    std::string releaseDate;
    int runtime;
    float popularity;
    float voteAverage;
    int voteCount;

    string backdropPath;
    string posterPath;
    bool ReadMovie(json_t *movie);
    bool AddMovieResults(json_t *root, vector<searchResultTvMovie> &resultSet, const std::vector<cNormedString> &normedStrings, const char *description, bool setMinTextMatch);
    int AddMovieResultsForUrl(cLargeString &buffer, const string &url, vector<searchResultTvMovie> &resultSet, const std::vector<cNormedString> &normedStrings, const char *description, bool setMinTextMatch);
    void AddMovieResults(cLargeString &buffer, vector<searchResultTvMovie> &resultSet, std::string_view SearchString, const std::vector<cNormedString> &normedStrings, const char *description, bool setMinTextMatch, const vector<int> &years, const cLanguage *lang);
public:
    cMovieDbMovie(cTVScraperDB *db, cMovieDBScraper *movieDBScraper);
    virtual ~cMovieDbMovie(void);
    void AddMovieResults(vector<searchResultTvMovie> &resultSet, std::string_view SearchString, const char *description, const vector<int> &years, const cLanguage *lang);
    bool ReadMovie(void);
    void SetID(int movieID) { id = movieID; };
    int ID(void) { return id; };
    void StoreDB(void);
    void StoreMedia();
    void Dump();
};


#endif //__TVSCRAPER_TVDBSERIES_H

#ifndef __TVSCRAPER_MOVIEDBMOVIE_H
#define __TVSCRAPER_MOVIEDBMOVIE_H

using namespace std;

// --- cMovieDbMovie -------------------------------------------------------------

class cMovieDbMovie {
private:
    cTVScraperDB *m_db;
    cMovieDBScraper *m_movieDBScraper;
    const cEvent *m_event;
    const cRecording *m_recording;
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

    string backdropPath;
    string posterPath;
    bool ReadMovie(json_t *movie);
    bool AddMovieResults(json_t *root, vector<searchResultTvMovie> &resultSet, const string &SearchString, const vector<int> &years);
    bool DownloadFile(const string &urlBase, const string &urlFileName, const string &destDir, int destID, const char * destFileName);
public:
    cMovieDbMovie(cTVScraperDB *db, cMovieDBScraper *movieDBScraper, const cEvent *event, const cRecording *recording);
    virtual ~cMovieDbMovie(void);
    bool AddMovieResults(vector<searchResultTvMovie> &resultSet, const string &SearchString);
    bool ReadMovie(void);
    void SetID(int movieID) { id = movieID; };
    int ID(void) { return id; };
    void StoreDB(void);
    void StoreMedia(string posterBaseUrl, string backdropBaseUrl, string destDir);
    void Dump();
};


#endif //__TVSCRAPER_TVDBSERIES_H

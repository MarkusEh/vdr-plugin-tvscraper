#ifndef __TVSCRAPER_MOVIEDBSCRAPER_H
#define __TVSCRAPER_MOVIEDBSCRAPER_H

using namespace std;

// --- cMovieDBScraper -------------------------------------------------------------

class cMovieDBScraper {
private:
    string apiKey;
    string language;
    string baseURL;
    string baseDir;
    string imageUrl;
    string posterSize;
    string backdropSize;
    string actorthumbSize;
    cTVScraperDB *db;
    map<string, int> cache;
    bool parseJSON(string jsonString);
    int SearchMovie(string movieName);
    int SearchMovieElaborated(string movieName);
    cMovieDbMovie *ReadMovie(int movieID);
    cMovieDbActors *ReadActors(int movieID);
    void StoreMedia(cMovieDbMovie *movie, cMovieDbActors *actors);
public:
    cMovieDBScraper(string baseDir, cTVScraperDB *db, string language);
    virtual ~cMovieDBScraper(void);
    bool Connect(void);
    void Scrap(const cEvent *event, int recordingID = 0);
};


#endif //__TVSCRAPER_MOVIEDBSCRAPER_H

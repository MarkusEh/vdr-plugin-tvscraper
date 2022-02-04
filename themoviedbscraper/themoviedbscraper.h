#ifndef __TVSCRAPER_MOVIEDBSCRAPER_H
#define __TVSCRAPER_MOVIEDBSCRAPER_H

using namespace std;
class cMovieDbMovie;
class cMovieDbActors;

struct searchResultTvMovie;


// --- cMovieDBScraper -------------------------------------------------------------

class cMovieDBScraper {
private:
    string apiKey;
    string language;
    string baseURL;
    string baseDir;
    string imageUrl;
    string posterSize;
    string stillSize;
    string backdropSize;
    string actorthumbSize;
    cTVScraperDB *db;
    cOverRides *overrides;
    map<string, int> cache;
    map<string, int> cacheTv;
    bool parseJSON(json_t *root);
    cMovieDbActors *ReadActors(int movieID);
    void StoreMedia(cMovieDbMovie *movie, cMovieDbActors *actors);

public:
    cMovieDBScraper(string baseDir, cTVScraperDB *db, string language, cOverRides *overrides);
    virtual ~cMovieDBScraper(void);
    bool Connect(void);
    const string GetLanguage(void) { return language; }
    const string GetApiKey(void) { return apiKey; }
    const string GetPosterBaseUrl(void) { return imageUrl + posterSize; }
    const string GetBackdropBaseUrl(void) { return imageUrl + backdropSize; }
    const string GetStillBaseUrl(void) { return imageUrl + stillSize; }
    const string GetActorsBaseUrl(void) { return imageUrl + actorthumbSize; }
    const string GetTvBaseDir(void) { return baseDir + "/tv/";  }
    const string GetActorsBaseDir(void) { return baseDir + "/actors";  }
    void StoreMovie(cMovieDbMovie &movie);
    void StoreStill(int tvID, int seasonNumber, int episodeNumber, const string &stillPathTvEpisode);
    int GetMovieRuntime(int movieID);
};


#endif //__TVSCRAPER_MOVIEDBSCRAPER_H

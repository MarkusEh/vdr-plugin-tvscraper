#ifndef __TVSCRAPER_IMAGESERVER_h
#define __TVSCRAPER_IMAGESERVER_h


using namespace std;

// --- cImageServer --------------------------------------------------------

class cImageServer {
private:
    cTVScraperDB *db;
    bool GetTvStill (cTvMedia &media, int id, int season_number, int episode_number);
    bool GetTvPoster(cTvMedia &media, int id);
    bool GetTvBackdrop(cTvMedia &media, int id);
    bool GetTvBackdrop(cTvMedia &media, int id, int season_number);
    bool GetTvPosterOrBackdrop(string &filePath, int id, const char *filename); // filename = poster.jpg || backdrop.jpg
    bool GetTvPosterOrBackdrop(string &filePath, int id, int season_number, const char *filename);
public:
    cImageServer(cTVScraperDB *db);
    virtual ~cImageServer(void);
    scrapType GetIDs(const cEvent *event, const cRecording *recording, int &movie_tv_id, int &season_number, int &episode_number);
    cTvMedia GetPosterOrBanner(int id, int season_number, int episode_number, scrapType type);
    cTvMedia GetPoster(int id, int season_number, int episode_number);
    bool GetBanner(cTvMedia &media, int id);
    cTvMedia GetStill(int id, int season_number, int episode_number);
    vector<cTvMedia> GetPosters(int id, scrapType type);
    vector<cTvMedia> GetSeriesFanarts(int id, int season_number, int episode_number);
    cTvMedia GetMovieFanart(int id);
    cTvMedia GetCollectionPoster(int id);
    cTvMedia GetCollectionFanart(int id);
    vector<cActor> GetActors(int id, int episodeID, scrapType type);
    string GetDescription(int id, int season_number, int episode_number, scrapType type);
    bool GetTvPoster(cTvMedia &media, int id, int season_number);
};

#endif // __TVSCRAPER_IMAGESERVER_h

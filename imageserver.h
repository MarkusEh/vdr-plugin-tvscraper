using namespace std;

// --- cImageServer --------------------------------------------------------

class cImageServer {
private:
    cTVScraperDB *db;
    cOverRides *overrides;
public:
    cImageServer(cTVScraperDB *db, cOverRides *overrides);
    virtual ~cImageServer(void);
    scrapType GetScrapType(const cEvent *event);
    int GetID(int eventID, scrapType type, bool isRecording);
    tvMedia GetPosterOrBanner(int id, scrapType type);
    tvMedia GetPoster(int id, scrapType type);
    tvMedia GetBanner(int id);
    vector<tvMedia> GetPosters(int id, scrapType type);
    vector<tvMedia> GetFanart(int id, scrapType type);
    vector<tvActor> GetActors(int id, scrapType type);
    string GetDescription(int id, scrapType type);
};
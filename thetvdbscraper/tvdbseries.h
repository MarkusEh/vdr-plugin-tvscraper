#ifndef __TVSCRAPER_TVDBSERIES_H
#define __TVSCRAPER_TVDBSERIES_H

using namespace std;
class cTVDBScraper;
struct searchResultTvMovie;


// --- cTVDBSeries -------------------------------------------------------------

class cTVDBSeries {
private:
    cTVScraperDB *m_db;
    cTVDBScraper *m_TVDBScraper;
    int m_seriesID;
    string name = "";
    string originalName = "";
    string overview = "";
    string firstAired = "";
    string genres = "";
    string networks = "";
    float popularity = 0.0; // this is the score;
    float rating = 0.0;
    int ratingCount = 0;
    string IMDB_ID = "";
    string status = "";
    set<int> episodeRunTimes;
    string banner = "";
    string fanart = "";
    string poster = "";
public:
    cTVDBSeries(cTVScraperDB *db, cTVDBScraper *TVDBScraper, int seriesID);
    virtual ~cTVDBSeries(void);
    bool ParseJson_all(json_t *data);
    bool ParseJson_Series(json_t *jSeries);
    bool ParseJson_Episode(json_t *jEpisode);
    void StoreDB();
    bool ParseJson_Character(json_t *jCharacter);
    bool ParseJson_Artwork(json_t *jArtwork, const std::map<int,int> &seasonIdNumber);
};

#endif //__TVSCRAPER_TVDBSERIES_H

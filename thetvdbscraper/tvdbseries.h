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
    const char *name = NULL;
    const char *originalName = NULL;
    const char *overview = NULL;
    const char *firstAired = NULL;
    std::string genres;
    const char *networks = NULL;
    float popularity = 0.0; // this is the score;
    float rating = 0.0;
    int ratingCount = 0;
    const char *IMDB_ID = NULL;
    const char *status = NULL;
    std::set<int> episodeRunTimes;
    const char *fanart = NULL;
    const char *poster = NULL;
    std::string translations;
    const char *translation(const rapidjson::Value &jTranslations, const char *arrayAttributeName, const char *textAttributeName);
public:
    cTVDBSeries(cTVScraperDB *db, cTVDBScraper *TVDBScraper, int seriesID);
    std::string m_language; // this is the default language, if name translations are available in the default language. Otherwise, the original language
    virtual ~cTVDBSeries(void);
    bool ParseJson_Series(const rapidjson::Value &series, const cLanguage *displayLanguage);
    int ParseJson_Episode(const rapidjson::Value &episode, cSql &insertEpisode);
    bool ParseJson_Episode(const rapidjson::Value &episode, const cLanguage *lang, cSql &insertEpisodeLang);
    bool ParseJson_Episode(const rapidjson::Value &jEpisode, cSql &insertEpisode2, cSql &insertRuntime, const cLanguage *lang, cSql &insertEpisodeLang);
    void StoreDB();
    bool ParseJson_Character(const rapidjson::Value &character);
    bool ParseJson_Artwork(const rapidjson::Value &series, const cLanguage *displayLanguage);
};

#endif //__TVSCRAPER_TVDBSERIES_H

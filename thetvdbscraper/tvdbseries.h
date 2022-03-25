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
    int seriesID = 0;
    string name = "";
    string overview = "";
    string firstAired = "";
//    string actors = "";
    string genres = "";
    string networks = "";
    float rating = 0.0;
    int ratingCount = 0;
    string IMDB_ID = "";
    int runtime = 0;
    string status = "";
    string banner = "";
    vector<int> episodeRunTimes;
    string fanart = "";
    string poster = "";
    void ParseXML_Series(xmlDoc *doc, xmlNode *node);
    void ParseXML_Episode(xmlDoc *doc, xmlNode *node);
    void ParseXML_searchSeries(xmlDoc *doc, xmlNode *node, vector<searchResultTvMovie> &resultSet, const string &SearchString);
    void ParseXML_search(xmlDoc *doc, vector<searchResultTvMovie> &resultSet, const string &SearchString);
public:
    cTVDBSeries(cTVScraperDB *db, cTVDBScraper *TVDBScraper);
    virtual ~cTVDBSeries(void);
    void ParseXML_all(xmlDoc *doc);
    int ID(void) { return seriesID; };
    void SetSeriesID(int id) { seriesID = id; }
    const char *Name(void) { return name.c_str(); };
    void StoreDB(cTVScraperDB *db);
    void StoreMedia(int tvID);
    void Dump();
    bool AddResults(vector<searchResultTvMovie> &resultSet, const string &SearchString, const string &SearchString_ext);
};


#endif //__TVSCRAPER_TVDBSERIES_H

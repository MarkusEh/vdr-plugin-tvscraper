#ifndef __TVSCRAPER_TVDBACTORS_H
#define __TVSCRAPER_TVDBACTORS_H

using namespace std; 

// --- cTVDBActor -------------------------------------------------------------
class cTVDBActor {
public:
    cTVDBActor(void) {
        path = "";
        name = "";
        role = "";
    };
    string path;
    string name;
    string role;
};

// --- cTVDBActors --------------------------------------------------------

class cTVDBActors {
private:
    string language;
    vector<cTVDBActor*> actors;
    void ReadEntry(xmlDoc *doc, xmlNode *node);
public:
    cTVDBActors(string language);
    virtual ~cTVDBActors(void);
    void ReadActors(xmlDoc *doc, xmlNode *nodeActors);
    void StoreDB(cTVScraperDB *db, int series_id);
    void Store(string baseUrl, string destDir);
    void Dump(bool verbose);
};

#endif //__TVSCRAPER_TVDBACTORS_H

#ifndef __TVSCRAPER_OVERRIDES_H
#define __TVSCRAPER_OVERRIDES_H

using namespace std;

// --- cOverRides --------------------------------------------------------

class cOverRides {
private:
    vector<string> ignores;
    map<string,scrapType> searchTypes;
    map<string,string> substitutes;
    map<string,int> m_thetvdbID;
    vector<string> ignorePath;
    vector<string> removePrefixes;
    void ReadConfigLine(string line);
public:
    cOverRides(void);
    virtual ~cOverRides(void);
    void ReadConfig(string confDir);
    bool Ignore(string title);
    bool Substitute(string &title);
    string RemovePrefix(string title);
    scrapType Type(string title);
    int thetvdbID(string title);
    bool IgnorePath(string path);
    void Dump(void);
}; 
#endif //__TVSCRAPER_OVERRIDES_H

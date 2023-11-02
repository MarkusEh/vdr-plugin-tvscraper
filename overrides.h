#ifndef __TVSCRAPER_OVERRIDES_H
#define __TVSCRAPER_OVERRIDES_H

using namespace std;

// --- cOverRides --------------------------------------------------------

class cOverRides {
private:
    vector<string> ignores;
    map<string,scrapType, less<>> searchTypes;
    map<string,string> substitutes;
    map<string,int,less<>> m_thetvdbID;
    vector<string> ignorePath;
    vector<string> removePrefixes;
    void ReadConfigLine(const char *line);
public:
    cOverRides(void);
    virtual ~cOverRides(void);
    void ReadConfig(cSv confDir);
    bool Ignore(cSv title);
    bool Substitute(string &title);
    void RemovePrefix(string &title);
    scrapType Type(cSv title);
    int thetvdbID(cSv title);
    bool IgnorePath(cSv path);
    void Dump(void);
}; 
#endif //__TVSCRAPER_OVERRIDES_H

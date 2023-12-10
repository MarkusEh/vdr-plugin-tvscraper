#ifndef __TVSCRAPER_OVERRIDES_H
#define __TVSCRAPER_OVERRIDES_H

using namespace std;

// --- cOverRides --------------------------------------------------------

class cOverRides {
private:
    std::vector<std::string> ignores;
    std::map<std::string,scrapType, less<>> searchTypes;
    std::map<std::string,string,less<>> substitutes;
    std::map<std::string,int,less<>> m_TheTVDB_SeriesID;
    std::map<std::string,int,less<>> m_TheMovieDB_SeriesID;
    std::map<std::string,int,less<>> m_TheMovieDB_MovieID;
    std::vector<std::string> ignorePath;
    std::vector<std::string> removePrefixes;
    void ReadConfigLine(const char *line);
public:
    cOverRides(void);
    virtual ~cOverRides(void);
    void ReadConfig(cSv confDir);
    bool Ignore(cSv title);
    bool Substitute(string &title);
    void RemovePrefix(string &title);
    scrapType Type(cSv title);
    int TheTVDB_SeriesID(cSv title);
    int TheMovieDB_SeriesID(cSv title);
    int TheMovieDB_MovieID(cSv title);
    bool IgnorePath(cSv path);
    void Dump(void);
};
#endif //__TVSCRAPER_OVERRIDES_H

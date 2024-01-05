#ifndef __TVSCRAPER_OVERRIDES_H
#define __TVSCRAPER_OVERRIDES_H

#include <regex>
enum class eMatchPurpouse {
  regexTitleChannel_id,
  regexTitleChannel_idEpisodeName,
  regexTitleChannel_idEpisodeAbsolutNumber, // TODO
  regexTitleShortTextChannel_idSeasonNumberEpisodeNumber,
};
class cRegexAction {
  friend class cOverRides;
  private:
    std::regex m_regex_title;
    std::regex m_regexShortText;
    std::regex m_regexChannel;
    bool m_matchChannel;
    int m_dbid;
    bool m_movie;
    eMatchPurpouse m_matchPurpouse;
  public:
    bool matches(cSv title, cSv shortText, cSv description, cSv channel, int &season, int &episode, std::string &episodeName) const;
};
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
    std::vector<cRegexAction> m_regex;
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
    bool regex(cSv title, cSv shortText, cSv description, cSv channel, eMatchPurpouse &matchPurpouse, int &dbid, bool &movie, int &season, int &episode, std::string &episodeName);
    bool IgnorePath(cSv path);
    void Dump(void);
};
#endif //__TVSCRAPER_OVERRIDES_H

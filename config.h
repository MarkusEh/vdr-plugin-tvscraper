#ifndef __TVSCRAPER_CONFIG_H
#define __TVSCRAPER_CONFIG_H
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <vdr/thread.h>

using namespace std;

enum scrapType {
    scrapSeries,
    scrapMovie,
    scrapNone
};
struct sMovieOrTv {
    scrapType type;
    int id;
    int season;
    int episode;
    int year;
    int episodeSearchWithShorttext;
};
class cLanguage {
  public:
    int m_id;
    const char *m_thetvdb;
    const char *m_themoviedb;
    const char *m_name;
    std::string getNames() const {
      std::stringstream result;
      result << m_thetvdb << " " << m_themoviedb << " " << m_name;
      return result.str();
    }
};

bool operator< (const tChannelID &c1, const tChannelID &c2) {
  if (c1.Source() != c2.Source() ) return c1.Source() < c2.Source();
  if (c1.Nid() != c2.Nid() ) return c1.Nid() < c2.Nid();
  if (c1.Tid() != c2.Tid() ) return c1.Tid() < c2.Tid();
  if (c1.Sid() != c2.Sid() ) return c1.Sid() < c2.Sid();
  return c1.Rid() < c2.Rid();
}
bool operator< (const cLanguage &l1, const cLanguage &l2) {
  return l1.m_id < l2.m_id;
}
bool operator< (int l1, const cLanguage &l2) {
  return l1 < l2.m_id;
}
bool operator< (const cLanguage &l1, int l2) {
  return l1.m_id < l2;
}

class cTVScraperConfigLock {
  private:
    cStateKey m_stateKey;
  public:
    cTVScraperConfigLock(bool Write = false);
    ~cTVScraperConfigLock();
};

class cTVScraperConfig {
    private:
// command line paramters
        string themoviedbSearchOption = "";
        bool readOnlyClient = false;
        bool m_autoTimersPathSet = false; // if true, all autoTimers will use m_autoTimersPath
        std::string m_autoTimersPath;
        string baseDir;       // /var/cache/vdr/plugins/tvscraper/
// "calculated" parameters, from command line paramters
        int baseDirLen;
        string baseDirSeries = "";
        string baseDirMovies = "";
        string baseDirMovieActors = "";
        string baseDirMovieCollections = "";
        string baseDirMovieTv = "";
        string EPG_UpdateFileName;       // file to touch in case of updated information on EPG
        string recordingsUpdateFileName; // file to touch in case of updated information on recordings
// list of data that can be changed in the setup menu
// we make these private, as access from several threads is possible. The methods to access the lists provide proper protection
// our friend cTVScraperSetup can still access the private members, an needs to take care of proper locking
        set<tChannelID> m_channels; 
        set<tChannelID> m_hd_channels; 
        set<string> m_excludedRecordingFolders;
        set<int> m_TV_Shows;  // TV_Shows where missing episodes will be recorded
        set<int> m_AdditionalLanguages;
        int m_enableAutoTimers;
        int m_defaultLanguage;
        map<tChannelID, int> m_channel_language; // if a channel is not in this map, it has the default language
// End of list of data that can be changed in the setup menu
        friend class cTVScraperConfigLock;
        friend class cTVScraperSetup;
        mutable cStateLock stateLock;
//        mutable cStateLock stateLock("tvscraper: config");
    public:
        cTVScraperConfig();
        ~cTVScraperConfig();
// static constant
        const std::set<cLanguage, std::less<>> m_languages =
{
{ 1, "dan", "da-DK", "dansk"},
{ 2, "deu", "de-AT", "Deutsch"},
{ 3, "deu", "de-CH", "Deutsch"},
{ 4, "deu", "de-DE", "Deutsch"},
{ 5, "eng", "en-GB", "English"},
{ 6, "eng", "en-US", "English"},
{ 7, "fra", "fr-FR", "français"},
{ 8, "ita", "it-IT", "italiano"},
{ 9, "nld", "nl-NL", "Nederlands"},
{10, "spa", "es-ES", "español"}
};
// list of data that can be changed in the setup menu
        int enableDebug;
// End of list of data that can be changed in the setup menu
        void Initialize(); // This is called during plugin initialize
        void setDefaultLanguage(); // set the default language from locale
// set values from VDRs config file
        bool SetupParse(const char *Name, const char *Value);
// get / set command line options
        void SetThemoviedbSearchOption(const string &option) { themoviedbSearchOption = option; };
        void SetAutoTimersPath(const string &option);
        void SetReadOnlyClient() { readOnlyClient = true; };
        void SetBaseDir(const string &dir);
        const string &GetBaseDir(void) const { return baseDir; };
        int GetBaseDirLen(void) const { return baseDirLen; };
        const string &GetBaseDirSeries(void) const { return baseDirSeries; };
        const string &GetBaseDirMovies(void) const { return baseDirMovies; };
        const string &GetBaseDirMovieActors(void) const { return baseDirMovieActors; };
        const string &GetBaseDirMovieCollections(void) const { return baseDirMovieCollections; };
        const string &GetBaseDirMovieTv(void) const { return baseDirMovieTv; };
        const string &GetThemoviedbSearchOption(void) const { return themoviedbSearchOption; };
        const string &GetAutoTimersPath() const { return m_autoTimersPath; };
        const bool GetAutoTimersPathSet() const { return m_autoTimersPathSet; };
        const string &GetEPG_UpdateFileName(void) const { return EPG_UpdateFileName; };
        const string &GetRecordingsUpdateFileName(void) const { return recordingsUpdateFileName; };
        bool GetReadOnlyClient() const { return readOnlyClient; }
// Methods to access parameters (lists) that can be changed in setup menu
// These methods are thread save
        set<tChannelID> GetScrapeChannels() const { cTVScraperConfigLock l; set<tChannelID> result = m_channels; return result; }
        bool ChannelActive(const tChannelID &channelID) const { cTVScraperConfigLock l; return m_channels.find(channelID) != m_channels.end(); }   // do we have to scrape this channel?
        bool ChannelHD(const tChannelID &channelID) const { cTVScraperConfigLock l; return m_hd_channels.find(channelID) != m_hd_channels.end(); }
        bool recordingFolderSelected(const std::string &recordingFolder) const { cTVScraperConfigLock l; return m_excludedRecordingFolders.find(recordingFolder) == m_excludedRecordingFolders.end(); }
        bool TV_ShowSelected(int TV_Show) const { cTVScraperConfigLock l; return m_TV_Shows.find(TV_Show) != m_TV_Shows.end(); }
        int getEnableAutoTimers() const { cTVScraperConfigLock l; int r = m_enableAutoTimers; return r; }
// languages
        int getDefaultLanguage() const { cTVScraperConfigLock l; int r = m_defaultLanguage; return r; }
        int numAdditionalLanguages() const { cTVScraperConfigLock l; int r = m_AdditionalLanguages.size(); return r; }
        const cLanguage *GetLanguage(int lang) const { auto r = m_languages.find(lang); if(r == m_languages.end()) return NULL; else return &(*r); }  // m_languages is constant -> no lock required
        int getLanguage(const tChannelID &channelID) const { int r; cTVScraperConfigLock lr; auto l = m_channel_language.find(channelID); if (l == m_channel_language.end()) r= m_defaultLanguage; else r = l->second; return r; }
};

#endif //__TVSCRAPER_CONFIG_H

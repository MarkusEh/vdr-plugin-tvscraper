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
    cLanguage(int id, const char*thetvdb, const char *themoviedb, const char *name):
      m_id(id), m_thetvdb(thetvdb), m_themoviedb(themoviedb), m_name(name) {}
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

bool operator< (const tChannelID &c1, const tChannelID &c2);
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
// our friend cTVScraperSetup can still access the private members, and needs to take care of proper locking
        set<tChannelID> m_channels;  // channels to be scraped
        map<tChannelID,int> m_HD_Channels;  // int = 0->SD, 1->HD, 2->UHD
        set<string> m_excludedRecordingFolders;
        set<int> m_TV_Shows;  // TV_Shows where missing episodes will be recorded
        set<int> m_AdditionalLanguages;
        int m_enableAutoTimers;
        int m_defaultLanguage = 0;
        map<tChannelID, int> m_channel_language; // if a channel is not in this map, it has the default language
// End of list of data that can be changed in the setup menu
        bool m_HD_ChannelsModified = false;
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
{ 2, "deu", "de-AT", "Deutsch, Österreich"},
{ 3, "deu", "de-CH", "Deutsch, Schweiz"},
{ 4, "deu", "de-DE", "Deutsch"},
{ 5, "eng", "en-GB", "English GB"},
{ 6, "eng", "en-US", "English US"},
{ 7, "fra", "fr-FR", "français"},
{ 8, "ita", "it-IT", "italiano"},
{ 9, "nld", "nl-NL", "Nederlands"},
{10, "spa", "es-ES", "español"}
};
        const cLanguage m_emergencyLanguage = {5, "eng", "en-GB", "English GB ERROR"};
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
        int ChannelHD(const tChannelID &channelID) const { cTVScraperConfigLock l; auto f = m_HD_Channels.find(channelID); if (f != m_HD_Channels.end()) return f->second; return 0; }
        map<tChannelID, int> GetChannelHD() const { map<tChannelID, int> r; cTVScraperConfigLock l; if (m_HD_ChannelsModified) r = m_HD_Channels; return r; }
        bool recordingFolderSelected(const std::string &recordingFolder) const { cTVScraperConfigLock l; return m_excludedRecordingFolders.find(recordingFolder) == m_excludedRecordingFolders.end(); }
        bool TV_ShowSelected(int TV_Show) const { cTVScraperConfigLock l; return m_TV_Shows.find(TV_Show) != m_TV_Shows.end(); }
        int getEnableAutoTimers() const { cTVScraperConfigLock l; int r = m_enableAutoTimers; return r; }
// languages
        map<tChannelID, int> GetChannelLanguages() const { cTVScraperConfigLock l; auto r = m_channel_language;  return r; }
        int numAdditionalLanguages() const { cTVScraperConfigLock l; int r = m_AdditionalLanguages.size(); return r; }
        set<int> GetAdditionalLanguages() const { cTVScraperConfigLock l; auto r = m_AdditionalLanguages; return r; }
        const cLanguage *GetDefaultLanguage() const; // this will ALLWAYS return a valid pointer to cLanguage
        const cLanguage *GetLanguage(const tChannelID &channelID) const;  // this will ALLWAYS return a valid pointer to cLanguage
        bool isDefaultLanguage(const cLanguage *l) const { if (!l) return true; cTVScraperConfigLock ll; bool r = l->m_id == m_defaultLanguage; return r; }
        int GetLanguage_n(const tChannelID &channelID) const;
};

#endif //__TVSCRAPER_CONFIG_H

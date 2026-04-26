#ifndef __TVSCRAPER_CONFIG_H
#define __TVSCRAPER_CONFIG_H
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <sqlite3.h>
#include <vdr/thread.h>
#include "tools/tvscraperhelpers.h"

#include "languages.h"

using namespace std;
class iExtEpg;
class cSql;
class cTVScraperDB;

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
    int episodeSearchWithShorttext; // 0: no; 1: yes; 3: episode match required for cache match
};
struct sChannelMapEpg {
      std::string extid;    // ARD
      std::string source;   // tvsp
      tChannelID channelID; // VDR channel ID
      int vps;
      int merge;
      std::shared_ptr<iExtEpg> extEpg;
};

bool operator< (const sChannelMapEpg &cm1, const sChannelMapEpg &cm2) { return cm1.channelID < cm2.channelID; }
bool operator==(const sChannelMapEpg &cm1, const sChannelMapEpg &cm2) { return cm1.channelID == cm2.channelID; }
bool operator< (const sChannelMapEpg &cm1, const tChannelID &c2) { return cm1.channelID < c2; }

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
    bool m_timersOnNumberOfTsFiles = false; // if true, autoTimers will be created if number of ts files != 1
    std::string m_autoTimersPath;
    string baseDir;       // /var/cache/vdr/plugins/tvscraper/
    vector<std::shared_ptr<iExtEpg>> m_extEpgs;
    vector<sChannelMapEpg> m_channelMap;
// "calculated" parameters, from command line paramters
    int baseDirLen = 0;
    string baseDirEpg = "";
    string baseDirRecordings = "";
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
    cSortedVector<tChannelID> m_channels;  // channels to be scraped
    std::map<tChannelID,int> m_HD_Channels;  // int = 0->SD, 1->HD, 2->UHD
    cSortedVector<std::string> m_excludedRecordingFolders;
    cSortedVector<int> m_TV_Shows;  // TV_Shows where missing episodes will be recorded
    int m_enableAutoTimers;
// End of list of data that can be changed in the setup menu
    bool m_HD_ChannelsModified = false;
    std::map<std::string,int,std::less<>> m_TheTVDB_company_name_networkID;
    std::map<std::string,int,std::less<>> m_channelName_networkID;
    cLanguages m_languages;
    friend class cTVScraperConfigLock;
    friend class cTVScraperLastMovieLock;
    friend class cTVScraperSetup;
    mutable cStateLock stateLock;
    mutable cStateLock stateLastMovieLock;
//        mutable cStateLock stateLock("tvscraper: config");
  public:
    cTVScraperConfig();
    ~cTVScraperConfig();
    cTVScraperDB *m_db = nullptr;
// static constant
    const std::string m_description_delimiter;
    const float minMatchFinal = 0.5;

// list of data that can be changed in the setup menu
    int enableDebug;
    int m_writeEpisodeToEpg;
// End of list of data that can be changed in the setup menu
    bool m_disable_images = false;
    bool m_disable_actor_images = false;
    void Initialize(); // This is called during plugin initialize
    void readNetworks();
    int Get_TheTVDB_company_ID_from_TheTVDB_company_name(cSv TheTVDB_company_name);
    int Get_TheTVDB_company_ID_from_channel_name(cSv channel_name);
// set values from VDRs config file
    bool SetupParse(const char *Name, const char *Value);
// get / set command line options
    void SetThemoviedbSearchOption(const string &option) { themoviedbSearchOption = option; };
    void SetAutoTimersPath(const string &option);
    void SetReadOnlyClient() { readOnlyClient = true; };
    void SetTimersOnNumberOfTsFiles() { m_timersOnNumberOfTsFiles = true; };
    void SetBaseDir(const string &dir);
    const string &GetBaseDir(void) const { return baseDir; };
    int GetBaseDirLen(void) const { return baseDirLen; };
    const string &GetBaseDirEpg(void) const { return baseDirEpg; };
    const string &GetBaseDirRecordings(void) const { return baseDirRecordings; };
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
    bool GetTimersOnNumberOfTsFiles() const { return m_timersOnNumberOfTsFiles; }
    bool isUpdateFromExternalDbRequired(time_t lastUpdate) { return difftime(time(0), lastUpdate) >= 60*60*24*1; }
    bool isUpdateFromExternalDbRequiredMR(time_t lastUpdate) { return difftime(time(0), lastUpdate) >= 60*60*24*4; }
// Methods to access parameters (lists) that can be changed in setup menu
// These methods are thread save
    cSortedVector<tChannelID> GetScrapeAndEpgChannels() const;
    bool ChannelActive(const tChannelID &channelID) const { cTVScraperConfigLock l; return m_channels.find(channelID) != m_channels.end(); }   // do we have to scrape this channel?
    int ChannelHD(const tChannelID &channelID) const { cTVScraperConfigLock l; auto f = m_HD_Channels.find(channelID); if (f != m_HD_Channels.end()) return f->second; return 0; }
    map<tChannelID, int> GetChannelHD() const { map<tChannelID, int> r; cTVScraperConfigLock l; if (m_HD_ChannelsModified) r = m_HD_Channels; return r; }
    bool recordingFolderSelected(const std::string &recordingFolder) const { cTVScraperConfigLock l; return m_excludedRecordingFolders.find(recordingFolder) == m_excludedRecordingFolders.end(); }
    bool TV_ShowSelected(int TV_Show) const { cTVScraperConfigLock l; return m_TV_Shows.find(TV_Show) != m_TV_Shows.end(); }
    int getEnableAutoTimers() const { cTVScraperConfigLock l; int r = m_enableAutoTimers; return r; }
    cLanguages &Languages() { return m_languages; }

    bool loadPlugins();
    std::shared_ptr<iExtEpgForChannel> GetExtEpgIf(const tChannelID &channelID) const;
    bool myDescription(const char *description) {
      for (auto &extEpg: m_extEpgs) if (extEpg->myDescription(description)) return true;
      return false;
    }
    cSv splitDescription(cSv description) {
      size_t pos = description.find(m_description_delimiter);
      if (pos == std::string::npos) return remove_trailing_whitespace(description);
      return remove_trailing_whitespace(description.substr(0, pos));
    }
/*
    cSv splitDescription(cSv description, cSv &secondPart) {
      size_t pos = description.find(m_description_delimiter);
      if (pos == std::string::npos) {
        secondPart = cSv();
        return description;
      }
      secondPart = description.substr(pos);
      return remove_trailing_whitespace(description.substr(0, pos));
    }
*/
    cMeasureTime timeSelectFromRecordings;
    cMeasureTime timeDownloadMedia;
    cMeasureTime timeDownloadActorsMovie;
};

#endif //__TVSCRAPER_CONFIG_H

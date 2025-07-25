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
class cLanguage {
  public:
    cLanguage(int id, const char *thetvdb, const char *themoviedb, const char *name):
      m_id(id), m_thetvdb(thetvdb), m_themoviedb(themoviedb), m_name(name) {}
    int m_id;
    const char *m_thetvdb;
    const char *m_themoviedb;
    const char *m_name;
    std::string getNames() const;
    void log() const;
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
bool operator< (const cLanguage &l1, const cLanguage &l2) { return l1.m_id < l2.m_id; }
bool operator< (int l1, const cLanguage &l2) { return l1 < l2.m_id; }
bool operator< (const cLanguage &l1, int l2) { return l1.m_id < l2; }
bool operator== (const cLanguage &l1, const cLanguage &l2) { return l1.m_id == l2.m_id; }

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
        std::vector<int> m_AdditionalLanguages;
        int m_enableAutoTimers;
        std::map<tChannelID, int> m_channel_language; // if a channel is not in this map, it has the default language
// End of list of data that can be changed in the setup menu
        bool m_HD_ChannelsModified = false;
        std::map<std::string,int,std::less<>> m_TheTVDB_company_name_networkID;
        std::map<std::string,int,std::less<>> m_channelName_networkID;
        friend class cTVScraperConfigLock;
        friend class cTVScraperLastMovieLock;
        friend class cTVScraperSetup;
        mutable cStateLock stateLock;
        mutable cStateLock stateLastMovieLock;
//        mutable cStateLock stateLock("tvscraper: config");
    public:
        cTVScraperConfig();
        ~cTVScraperConfig();
// see https://api.themoviedb.org/3/configuration/languages?api_key=abb01b5a277b9c2c60ec0302d83c5ee9
// see https://api.themoviedb.org/3/configuration/primary_translations?api_key=abb01b5a277b9c2c60ec0302d83c5ee9

// static constant
        const std::string m_description_delimiter;
        const cSortedVector<cLanguage, std::less<>> m_languages =
{
{ 1, "dan", "da-DK", "dansk"},
{ 2, "deu", "de-AT", "deutsch, Österreich"},
{ 3, "deu", "de-CH", "deutsch, Schweiz"},
{ 4, "deu", "de-DE", "deutsch"},
{ 5, "eng", "en-GB", "english GB"},
{ 6, "eng", "en-US", "english US"},
{ 7, "fra", "fr-FR", "français"},
{ 8, "ita", "it-IT", "italiano"},
{ 9, "nld", "nl-NL", "nederlands"},
{10, "spa", "es-ES", "español"},
{11, "fin", "fi-FI", "suomi"},     // Finnish
{12, "ita", "it-IT", "italiano"},  // DUPLICATE of 8
{13, "nld", "nl-NL", "nederlands"}, // DUPLICATE of 9
{14, "swe", "sv-SE", "svenska"}, // Swedish
{15, "por", "pt-PT", "português - Portugal"},
{16, "nor", "nb-NO", "norsk bokmål"}, // Norwegian
{17, "hrv", "hr-HR", "hrvatski jezik"},  // Croatian
{18, "ara", "ar-AE", "العربية"}, // Arabic
{19, "pt",  "pt-BR", "português - Brasil"},
{20, "zhtw", "zh-TW", "臺灣國語"},   // Chinese - Taiwan
{21, "fas", "fa-IR", "فارسی"},       // Persian
{22, "hin", "hi-IN", "हिन्दी", },       // Hindi
{23, "hun", "hu-HU", "Magyar"},      // Hungarian
{24, "kor", "ko-KR", "한국어"},      // Korean
{25, "slk", "sk-SK", "slovenčina"},  // Slovak
{26, "tur", "tr-TR", "Türkçe"},  // Turkish
{27, "ces", "cs-CZ", "čeština"},  // Czech
{28, "ell", "el-GR", "ελληνική γλώσσα"},  // Greek
{29, "zho", "zh-CN", "大陆简体"},  //  Chinese - China
{30, "ron", "ro-RO", "limba română"},  // Romanian
{31, "jpn", "ja-JP", "日本語"},  //  Japanese
{32, "mon", "mn-MN", "монгол"},  // Mongolian, note: not supported in themoviedb, "mn-MN" is a guess
{33, "pol", "pl-PL", "język polski"},  // Polish
{34, "msa", "ms-MY", "bahasa Melayu"},  //  Malay
{35, "ukr", "uk-UA", "українська мова"}, //  Ukrainian
{36, "tgl", "tl-PH", "Wikang Tagalog"},  //  Tagalog
{37, "tam", "ta-IN", "தமிழ்"},  /// Tamil
{38, "swa", "sw-SW", "Kiswahili"},  // Swahili
{39, "vie", "vi-VN", "Tiếng Việt"},  // Vietnamese
{40, "tha", "th-TH", "ภาษาไทย"},  //  	Thai
{41, "eus", "eu-ES", "euskera"},  //  	Basque
{42, "yue", "zh-HK", "粤语"},  //  	branch of the Sinitic languages primarily spoken in Southern China
{43, "rus", "ru-RU", "Pусский"},  //  	Russian
{44, "heb", "he-IL", "עִבְרִית"},  //  	Hebrew
{45, "zul", "zu-ZA", "isiZulu"},  //  	Zulu
{46, "ben", "bn-BD", "বাংলা"},    //  	Bengali
{47, "srp", "sr-RS", "Srpski"},    //  	Serbian
{48, "mal", "ml-IN", "മലയാളം"},    //  	Malayalam
{49, "kat", "ka-GE", "ქართული"},   //  	Georgian
{50, "lav", "lv-LV", "Latviešu"},  //  	Latvian
{51, "urd", "ur-PK", "اردو"},  //  Urdu
{52, "mlt", "mt-", "Malti"},  //  Maltese
{53, "afr", "af-ZA", "Afrikaans"},  //  Afrikaans
// {54, "", "", ""},  //
};
        cTVScraperDB *m_db = nullptr;
        const cLanguage m_emergencyLanguage = {5, "eng", "en-GB", "English GB ERROR"};
        const cLanguage *m_defaultLanguage = &m_emergencyLanguage;
// static constant
        const float minMatchFinal = 0.5;

// list of data that can be changed in the setup menu
        int enableDebug;
        int m_writeEpisodeToEpg;
// End of list of data that can be changed in the setup menu
        void Initialize(); // This is called during plugin initialize
        void readNetworks();
        int Get_TheTVDB_company_ID_from_TheTVDB_company_name(cSv TheTVDB_company_name);
        int Get_TheTVDB_company_ID_from_channel_name(cSv channel_name);
        void setDefaultLanguage(); // set the default language from locale
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
        bool isUpdateFromExternalDbRequired(time_t lastUpdate) { return difftime(time(0), lastUpdate) >= 60*60*24*7; }
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
// languages
        map<tChannelID, int> GetChannelLanguages() const { cTVScraperConfigLock l; auto r = m_channel_language;  return r; }
        int numAdditionalLanguages() const { cTVScraperConfigLock l; int r = m_AdditionalLanguages.size(); return r; }
        vector<int> GetAdditionalLanguages() const { cTVScraperConfigLock l; auto r = m_AdditionalLanguages; return r; }
        const cLanguage *GetDefaultLanguage() const { return m_defaultLanguage; } // this will ALLWAYS return a valid pointer to cLanguage
        const cLanguage *GetLanguage(const tChannelID &channelID) const;  // this will ALLWAYS return a valid pointer to cLanguage
        bool isDefaultLanguage(const cLanguage *l) const { if (!l) return false; return l->m_id == m_defaultLanguage->m_id; }
        bool isDefaultLanguageThetvdb(const cLanguage *l) const { if (!l) return false; return strcmp(l->m_thetvdb, m_defaultLanguage->m_thetvdb) == 0; }
        int GetLanguage_n(const tChannelID &channelID) const;
        const cLanguage *GetLanguageThetvdb(const cLanguage *l) const;
        const cLanguage *GetLanguage(int l) const;
        bool loadPlugins();
        std::shared_ptr<iExtEpgForChannel> GetExtEpgIf(const tChannelID &channelID) const;
        bool myDescription(const char *description) {
          for (auto &extEpg: m_extEpgs) if (extEpg->myDescription(description)) return true;
          return false;
        }
        cSv splitDescription(cSv description) {
          size_t pos = description.find(m_description_delimiter);
          if (pos == std::string::npos) return description;
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

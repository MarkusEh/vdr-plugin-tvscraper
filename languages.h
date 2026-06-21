#ifndef __TVSCRAPER_LANGUAGES_H
#define __TVSCRAPER_LANGUAGES_H
#include <string>
#include <vector>
#include <map>
#include <vdr/thread.h>
#include "tools/tvscraperhelpers.h"

class cLanguage {
  public:
    int Id() const { return m_id; }
    int Prio() const { return m_prio; }
    const char *Iso() const { return m_ISO_639_2_T; }
    const char *Thetvdb() const { return m_thetvdb; }
    const char *Themoviedb() const { return m_themoviedb; }
    bool equal_iso(const cLanguage *other) const { return memcmp(Iso(), other->Iso(), 4) == 0; }
    bool less_iso(const cLanguage *other) const { return memcmp(Iso(), other->Iso(), 4) < 0; }
    const char *Name() const { return m_name; }
    std::string getNames() const { return std::string(cToSvConcat(m_thetvdb, " ", m_themoviedb, " ", m_name)); }
    void log() const {
      esyslog2("language: m_id ", m_id, "m_thetvdb ", m_thetvdb?m_thetvdb:"null", " m_themoviedb ", m_themoviedb?m_themoviedb:"null", " m_name ", m_name?m_name:"null");
    }
    friend class cLanguages;
  private:
    cLanguage(int id, int prio, const char *ISO_639_2_T, const char *thetvdb, const char *themoviedb, const char *name):
      m_id(id), m_prio(prio), m_ISO_639_2_T(ISO_639_2_T), m_thetvdb(thetvdb), m_themoviedb(themoviedb), m_name(name) {}
    int m_id;   // unique ID, must never change. Used in DB
    int m_prio; // in case of several matches, use the match with highest prio; negative prio -> duplicate
    const char *m_ISO_639_2_T;   // ISO 639-2/T
    const char *m_thetvdb;
    const char *m_themoviedb;
    const char *m_name;
};

bool operator< (const cLanguage &l1, const cLanguage &l2) { return l1.Id() < l2.Id(); }
bool operator< (int l1, const cLanguage &l2) { return l1 < l2.Id(); }
bool operator< (const cLanguage &l1, int l2) { return l1.Id() < l2; }
bool operator== (const cLanguage &l1, const cLanguage &l2) { return l1.Id() == l2.Id(); }

struct gt_iso {
  bool operator ()(const cLanguage *l1, const cLanguage *l2) const {
    int ie = memcmp(l1->Iso(), l2->Iso(), 4);
    return ie?ie>0:l1->Prio() > l2->Prio();
  }
  bool operator ()(const cLanguage *l1, const char *iso) const {
    return memcmp(l1->Iso(), iso, 4) > 0;
  }
  bool operator ()(const char *iso, const cLanguage *l2) const {
    return memcmp(iso, l2->Iso(), 4) > 0;
  }
};
struct gt_thetvdb {
  bool operator ()(const cLanguage *l1, const cLanguage *l2) const {
    int ie = strcmp(l1->Thetvdb(), l2->Thetvdb() );
    return ie?ie>0:l1->Prio() > l2->Prio();
  }
  bool operator ()(const cLanguage *l1, const char *thetvdb) const {
    return strcmp(l1->Thetvdb(), thetvdb) > 0;
  }
  bool operator ()(const char *thetvdb, const cLanguage *l2) const {
    return strcmp(thetvdb, l2->Thetvdb() ) > 0;
  }
};
struct gt_themoviedb {
  bool operator ()(const cLanguage *l1, const cLanguage *l2) const {
    int ie = strcmp(l1->Themoviedb(), l2->Themoviedb() );
    return ie?ie>0:l1->Prio() > l2->Prio();
  }
  bool operator ()(const cLanguage *l1, const char *themoviedb) const {
    return strcmp(l1->Themoviedb(), themoviedb) > 0;
  }
  bool operator ()(const char *themoviedb, const cLanguage *l2) const {
    return strcmp(themoviedb, l2->Themoviedb() ) > 0;
  }
};

class cLanguages;
class cLanguagesLock {
  private:
    cStateKey m_stateKey;
  public:
    cLanguagesLock(const cLanguages *languages, bool Write = false);
    ~cLanguagesLock() {
      m_stateKey.Remove();
    }
};

class cLanguages {
  public:
// get language
    const cLanguage *GetLanguage(const cRecording *recording) const;
template<class E>
    const cLanguage *GetLanguage(const E *event) const;

// search m_languages (list of all supported languages)
    const cLanguage *GetLanguage(int l) const;
    const cLanguage *GetLanguageIso(const char *iso) const;
    const cLanguage *GetLanguageThetvdb(const char *thetvdb, const char *context = nullptr) const;
    const cLanguage *GetLanguageThemoviedb(const char *themoviedb) const;

// search EPG of this channel
    const cLanguage *GetLanguageFromEPG(const tChannelID &channelID) const;

// === setup: read / data stored in setup.conf   ==================================================
// default language
    void SetDefaultLanguage(const cLanguage *l) { m_defaultLanguage = l; } // use with care, from config.c reading setup values only. Note m_defaultLanguage must not change!
    const cLanguage *GetDefaultLanguage() const { return m_defaultLanguage; } // this will ALLWAYS return a valid pointer to cLanguage
    bool isDefaultLanguageThetvdb(const cLanguage *l) const { if (!l) return false; return strcmp(l->m_thetvdb, m_defaultLanguage->m_thetvdb) == 0; }
// AdditionalLanguages: in addition to the default language
    int numAdditionalLanguages() const { cLanguagesLock l(this); int r = m_AdditionalLanguages.size(); return r; }
    std::vector<int> GetAdditionalLanguages() const { cLanguagesLock l(this); auto r = m_AdditionalLanguages; return r; }
    void SwapAdditionalLanguages(std::vector<int> &additionalLanguages) { cLanguagesLock l(this, true); m_AdditionalLanguages.swap(additionalLanguages); }

// channel languages
    int GetLanguageFromSetup_n(const tChannelID &channelID) const;  // return language if this is explicitly maintained for channelID; otherwise: -1
    const cLanguage *GetLanguageFromSetup(const tChannelID &channelID) const;  // only return language if this is explicitly maintained for channelID; otherwise: nullptr
    void SwapLanguagesFromSetup(std::map<tChannelID, int> &channel_language) { cLanguagesLock l(this, true); m_setup_channel_language.swap(channel_language); }
    bool AddLanguageFromSetup(int language, cSv channel_list);

// others
    int Max_name_length() const { return m_max_name_length; }
    int Max_id() const { return m_max_id; }
  private:
// list of data that can be changed in the setup menu
// we make these private, as access from several threads is possible. The methods to access the lists provide proper protection
    std::vector<int> m_AdditionalLanguages;
    std::map<tChannelID, int> m_setup_channel_language; // this is the map saved in setup.conf. If a channel is not in this map, it has the default language
// End of list of data that can be changed in the setup menu

    cSortedVector<cLanguage*, gt_iso> m_languages_iso;
    cSortedVector<cLanguage*, gt_thetvdb> m_languages_thetvdb;
    cSortedVector<cLanguage*, gt_themoviedb> m_languages_themoviedb;

    std::map<tChannelID, const cLanguage*> m_station_channel_language; // languages as sent by stations
    const cLanguage m_emergencyLanguage = {6, 1, "eng", "eng", "en-US", "English ERROR"};
    const cLanguage *m_defaultLanguage = &m_emergencyLanguage;
    size_t m_max_name_length; // strlen(..) of longest m_name
    int m_max_id;             // max Id() of any language
    mutable int m_errors_no_language_reported = 0;

    friend class cLanguagesLock;
#if !defined test_stringhelpers
    mutable cStateLock stateLock;
#endif
    void setDefaultLanguage(); // set the default language from locale
    const cLanguage *GetLanguageFromEPG_int(const cChannel *Channel, unsigned max_epg_entries = std::numeric_limits<unsigned>::max()) const;
    const cLanguage *GetLanguage_int(int l) const;
///< if l is not in the list, write error to syslog and return nullptr
///< if l is duplicate language: return pointer to nun-duplicate language

  public:
    cLanguages() {
      m_languages_iso.reserve(m_languages.size() );
      m_languages_thetvdb.reserve(m_languages.size() );
      m_languages_themoviedb.reserve(m_languages.size() );
      m_max_name_length = 0;
      m_max_id = 0;
      for (const cLanguage &l: m_languages) {
        m_languages_iso.insert((cLanguage *)&l);
        m_languages_thetvdb.insert((cLanguage *)&l);
        m_languages_themoviedb.insert((cLanguage *)&l);
        m_max_name_length = std::max(m_max_name_length, strlen(l.m_name));
        m_max_id = std::max(m_max_id, l.Id());
      }
      Update_channel_station_language();
    }
    ~cLanguages() {}
    void Initialize() { if (m_defaultLanguage == &m_emergencyLanguage) setDefaultLanguage(); }
    void Update_channel_station_language();

// see https://api.themoviedb.org/3/configuration/languages?api_key=abb01b5a277b9c2c60ec0302d83c5ee9
// see https://api.themoviedb.org/3/configuration/primary_translations?api_key=abb01b5a277b9c2c60ec0302d83c5ee9
    const cSortedVector<cLanguage, std::less<>> m_languages =
{
{ 1, 0, "dan", "dan", "da-DK", "dansk"},
{ 2, 0, "deu", "deu", "de-AT", "deutsch, Österreich"},
{ 3, 0, "deu", "deu", "de-CH", "deutsch, Schweiz"},
{ 4, 1, "deu", "deu", "de-DE", "deutsch"},
{ 5, 0, "eng", "eng", "en-GB", "english GB"},
{ 6, 1, "eng", "eng", "en-US", "english"},
{ 7, 0, "fra", "fra", "fr-FR", "français"},
{ 8, 0, "ita", "ita", "it-IT", "italiano"},
{ 9, 0, "nld", "nld", "nl-NL", "nederlands"},
{10, 0, "spa", "spa", "es-ES", "español"},
{11, 0, "fin", "fin", "fi-FI", "suomi"},     // Finnish
{12, -8, "ita", "ita", "it-IT", "italiano"},  // DUPLICATE of 8
{13, -9, "nld", "nld", "nl-NL", "nederlands"}, // DUPLICATE of 9
{14, 0, "swe", "swe", "sv-SE", "svenska"}, // Swedish
{15, 0, "por", "por", "pt-PT", "português - Portugal"},
{16, 0, "nor", "nor", "nb-NO", "norsk bokmål"}, // Norwegian
{17, 0, "hrv", "hrv", "hr-HR", "hrvatski jezik"},  // Croatian
{18, 0, "ara", "ara", "ar-AE", "العربية"}, // Arabic
{19, 0, "por", "pt",  "pt-BR", "português - Brasil"},
{20, 0, "zho", "zhtw", "zh-TW", "臺灣國語"},   // Chinese - Taiwan
{21, 0, "fas", "fas", "fa-IR", "فارسی"},       // Persian
{22, 0, "hin", "hin", "hi-IN", "हिन्दी", },       // Hindi
{23, 0, "hun", "hun", "hu-HU", "Magyar"},      // Hungarian
{24, 0, "kor", "kor", "ko-KR", "한국어"},      // Korean
{25, 0, "slk", "slk", "sk-SK", "slovenčina"},  // Slovak
{26, 0, "tur", "tur", "tr-TR", "Türkçe"},  // Turkish
{27, 0, "ces", "ces", "cs-CZ", "čeština"},  // Czech
{28, 0, "ell", "ell", "el-GR", "ελληνική γλώσσα"},  // Greek
{29, 1, "zho", "zho", "zh-CN", "大陆简体"},  //  Chinese - China
{30, 0, "ron", "ron", "ro-RO", "limba română"},  // Romanian
{31, 0, "jpn", "jpn", "ja-JP", "日本語"},  //  Japanese
{32, 0, "mon", "mon", "mn-MN", "монгол"},  // Mongolian, note: not supported in themoviedb, "mn-MN" is a guess
{33, 0, "pol", "pol", "pl-PL", "język polski"},  // Polish
{34, 0, "msa", "msa", "ms-MY", "bahasa Melayu"},  //  Malay
{35, 0, "ukr", "ukr", "uk-UA", "українська мова"}, //  Ukrainian
{36, 0, "tgl", "tgl", "tl-PH", "Wikang Tagalog"},  //  Tagalog
{37, 0, "tam", "tam", "ta-IN", "தமிழ்"},  /// Tamil
{38, 0, "swa", "swa", "sw-SW", "Kiswahili"},  // Swahili
{39, 0, "vie", "vie", "vi-VN", "Tiếng Việt"},  // Vietnamese
{40, 0, "tha", "tha", "th-TH", "ภาษาไทย"},  //  	Thai
{41, 0, "eus", "eus", "eu-ES", "euskera"},  //  	Basque
{42, 0, "zho", "yue", "zh-HK", "粤语"},  //  	branch of the Sinitic languages primarily spoken in Southern China
{43, 0, "rus", "rus", "ru-RU", "Pусский"},  //  	Russian
{44, 0, "heb", "heb", "he-IL", "עִבְרִית"},  //  	Hebrew
{45, 0, "zul", "zul", "zu-ZA", "isiZulu"},  //  	Zulu
{46, 0, "ben", "ben", "bn-BD", "বাংলা"},    //  	Bengali
{47, 0, "srp", "srp", "sr-RS", "Srpski"},    //  	Serbian
{48, 0, "mal", "mal", "ml-IN", "മലയാളം"},    //  	Malayalam
{49, 0, "kat", "kat", "ka-GE", "ქართული"},   //  	Georgian
{50, 0, "lav", "lav", "lv-LV", "Latviešu"},  //  	Latvian
{51, 0, "urd", "urd", "ur-PK", "اردو"},  //  Urdu
{52, 0, "mlt", "mlt", "mt-", "Malti"},  //  Maltese
{53, 0, "afr", "afr", "af-ZA", "Afrikaans"},  //  Afrikaans
{54, 0, "guj", "guj", "gu-", "ગુજરાતી (Gujarātī)"},  // Gujarati
// {55, 0, "", "", "", ""},  //
};

};

// implement class cLanguagesLock *************************
inline cLanguagesLock::cLanguagesLock(const cLanguages *languages, bool Write) {
#if !defined test_stringhelpers
  languages->stateLock.Lock(m_stateKey, Write);
#endif
}

// implement class cLanguages  ****************************
inline bool cLanguages::AddLanguageFromSetup(int language, cSv channel_list) {
// initialize m_setup_channel_language from setup.conf
// true if language exists
  const cLanguage *l = GetLanguage_int(language);
  if (!l) return false;
  cLanguagesLock lw(this, true);
  for (cSv c: cSplit(channel_list, ';', eSplitDelimBeginEnd::optional)) m_setup_channel_language.insert({lexical_cast<tChannelID>(c), l->Id() });
  return true;
}

// get methods for languages
inline const cLanguage *cLanguages::GetLanguageIso(const char *iso) const {
// return nullptr if there is no language with this iso code.
// otherwise, return matching language with highet Prio()
  if (!iso || strlen(iso) != 3) return nullptr;
  auto f = m_languages_iso.find(iso);
  if (f == m_languages_iso.end() ) return nullptr;
  return *f;
}
inline const cLanguage *cLanguages::GetLanguageThetvdb(const char *thetvdb, const char *context) const {
// return nullptr if there is no language with this thetvdb code.
// otherwise, return matching language with highet Prio()
  if (!thetvdb || !*thetvdb) {
    if (context) esyslog3("no language provided, context ", context);
    return nullptr;
  }
  auto f = m_languages_thetvdb.find(thetvdb);
  if (f == m_languages_thetvdb.end() ) {
    if (context) esyslog3("language ", thetvdb, " in m_languages missing, context ", context);
    return nullptr;
  }
  return *f;
}
inline const cLanguage *cLanguages::GetLanguageThemoviedb(const char *themoviedb) const {
// return nullptr if there is no language with this themoviedb code.
// otherwise, return matching language with highet Prio()
  if (!themoviedb || !*themoviedb) return nullptr;
  auto f = m_languages_themoviedb.find(themoviedb);
  if (f == m_languages_themoviedb.end() ) return nullptr;
  return *f;
}
inline const cLanguage *cLanguages::GetLanguage_int(int l) const
// if l is not in the list, write error to syslog and return nullptr
// if l is duplicate language: return pointer to nun-duplicate language
{
  auto lIt = m_languages.find(l);
  if (lIt == m_languages.end() ) {
    esyslog3("Language number ", l, " not found, m_languages.size() = ", m_languages.size());
    return nullptr;
  }
  if (lIt->Prio() < 0) {
    auto lIt_nd = m_languages.find(-lIt->Prio());
    if (lIt_nd == m_languages.end() )
      esyslog3("Language number ", l, " nun-duplicate language ", -lIt->Prio(), " not found");
    else
      lIt = lIt_nd;
  }
  return &(*lIt);
}
inline const cLanguage *cLanguages::GetLanguage(int l) const
// return ALLWAYS a valid cLanguage pointer
// if l is not in the list: write ERROR to syslog, and return default language
{
  const cLanguage *result = GetLanguage_int(l);  // already writes error to syslog if l not found
  if (!result) result = m_defaultLanguage;
  return result;
}
inline int cLanguages::GetLanguageFromSetup_n(const tChannelID &channelID) const {
// only return language if this is explicitly maintained for channelID; otherwise: -1
  int lang;
  {
    cLanguagesLock lr(this);
    auto l = m_setup_channel_language.find(channelID);
    if (l == m_setup_channel_language.end()) lang = -1;
    else lang = l->second;
  }
  return lang;
}
inline const cLanguage *cLanguages::GetLanguageFromSetup(const tChannelID &channelID) const {
// only return language if this is explicitly maintained for channelID; otherwise: nullptr
  int lang = GetLanguageFromSetup_n(channelID);
  if (lang == -1) return nullptr;
  return GetLanguage_int(lang);
}

inline void cLanguages::setDefaultLanguage() {
  std::string loc = setlocale(LC_NAME, NULL);
  size_t index = loc.find_first_of("_");
  if (index != 2) {
    esyslog3("language ", loc, " index = ", index, " use en-US as default language");
    m_defaultLanguage = GetLanguage(6); // en-GB
    return;
  }
  const cLanguage *li = nullptr;
  const cLanguage *lc = nullptr;
  int prio_li = -100000;
  int prio_lc = -100000;
  for (const cLanguage &lang: m_languages) {
    if (strncmp(lang.m_themoviedb, loc.c_str(), 2) != 0) continue;
    if (!li || lang.m_prio > prio_li) {
      li = &lang;
      prio_li = lang.m_prio;
    }
    if (strncmp(lang.m_themoviedb+3, loc.c_str()+3, 2) == 0) if (!lc || lang.m_prio > prio_lc) {
      lc = &lang;
      prio_lc = lang.m_prio;
    }
  }
  if (li == nullptr) {
    esyslog3("language ", loc, " not found, use en-US as default language");
    m_defaultLanguage = GetLanguage(6); // en-US
    return;
  }
  if (lc != nullptr) m_defaultLanguage = lc;
  else m_defaultLanguage = li;
  esyslog("tvscraper: set default language to %s", m_defaultLanguage->getNames().c_str() );
}

inline const cLanguage *cLanguages::GetLanguageFromEPG(const tChannelID &channelID) const {
  const cLanguage *result;
  {
    cLanguagesLock lr(this);
    auto f = m_station_channel_language.find(channelID);
    if (f == m_station_channel_language.end() ) result = nullptr;
    else result = f->second;
  }
  return result;
}
inline void cLanguages::Update_channel_station_language() {
#if VDRVERSNUM >= 20709
  cLanguagesLock lw(this, true);
  m_station_channel_language.clear();
  LOCK_CHANNELS_READ;
  for (const cChannel *channel = Channels->First(); channel; channel = Channels->Next(channel)) {
    const cLanguage *l = GetLanguageFromEPG_int(channel, 10);
    if (l) m_station_channel_language.insert({channel->GetChannelID(), l});
  }
#endif
}
inline const cLanguage *cLanguages::GetLanguageFromEPG_int(const cChannel *Channel, unsigned max_epg_entries) const {
// WARNING: VDR object SCHEDULES will be locked!

// check the EPG of this channel, and the language of the EPG entries
// return language of these
// if unknown, return nullptr. Reasons for nullptr:
//   - vdr version too old
//   - no schedule found for this channel (e.g. old channel used in recordings)
//   - no EPG in this schedule
//   - missing or non-standard language information in EPG
#if VDRVERSNUM >= 20709
  LOCK_SCHEDULES_READ;
  const cSchedule *schedule = Schedules->GetSchedule(Channel, false);
  if (!schedule) return nullptr;
  unsigned epg_entries = 0;
  for (const cEvent *e = schedule->Events()->First(); e; e = schedule->Events()->Next(e), ++epg_entries) {
    if (strlen(e->Language()) == 3) return GetLanguageIso(e->Language() );
    if (epg_entries >= max_epg_entries) {
      if (++m_errors_no_language_reported < 4) esyslog3("no language for channel ", Channel->Number(), " ", Channel->Name(), " giving up after checking ", epg_entries, " epg entries");
      break;
    }
  }
#endif
  return nullptr;
}
template<class E>
inline const cLanguage *cLanguages::GetLanguage(const E *event) const {
// for events only. For recordings, use GetLanguage(const cRecording *recording)
// we try in the following order:
// 1 language explicitly maintained by user for channel (prio 1 -> User can override broken data from station)
// 2 iso (provided by station for this event/recording)
// 3 use system/default language
  const cLanguage *l = GetLanguageFromSetup(event->ChannelID() );
  if (l) return l;
#if VDRVERSNUM >= 20709
  l = GetLanguageIso(event->Language() );
  if (l) return l;
#endif
  return GetDefaultLanguage();
}
inline const cLanguage *cLanguages::GetLanguage(const cRecording *recording) const {
// we try in the following order:
// 1 language explicitly maintained by user for channel (prio 1 -> User can override broken data from station)
// 2 iso (provided by station for this event/recording)
// 3 look for epg entires in channelID, and use language of these (if available).
//   This is for recordings done before vdr 2.7.9 where language is missing in the recording, but might be available now for the channel
// 4 use system/default language
  const cLanguage *l = GetLanguageFromSetup(recording->Info()->ChannelID() );
  if (l) return l;
#if VDRVERSNUM >= 20709
  l = GetLanguageIso(recording->Info()->GetEvent()->Language() );
  if (l) return l;
  l = GetLanguageFromEPG(recording->Info()->ChannelID() );
  if (l) return l;
#endif
  return GetDefaultLanguage();
}
#endif //__TVSCRAPER_LANGUAGES_H

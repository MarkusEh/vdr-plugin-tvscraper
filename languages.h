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
  private:
// list of data that can be changed in the setup menu
// we make these private, as access from several threads is possible. The methods to access the lists provide proper protection
    std::vector<int> m_AdditionalLanguages;
    std::map<tChannelID, int> m_channel_language; // if a channel is not in this map, it has the default language
// End of list of data that can be changed in the setup menu

    const cLanguage m_emergencyLanguage = {5, 1, "eng", "eng", "en-GB", "English GB ERROR"};
    const cLanguage *m_defaultLanguage = &m_emergencyLanguage;

    friend class cLanguagesLock;
#if !defined test_stringhelpers
    mutable cStateLock stateLock;
#endif
    const cLanguage *GetLanguage_int(int l) const;

  public:
    cLanguages() {
      m_languages_iso.reserve(m_languages.size() );
      m_languages_thetvdb.reserve(m_languages.size() );
      m_languages_themoviedb.reserve(m_languages.size() );
      for (const cLanguage &l: m_languages) {
        m_languages_iso.insert((cLanguage *)&l);
        m_languages_thetvdb.insert((cLanguage *)&l);
        m_languages_themoviedb.insert((cLanguage *)&l);
      }
    }
    ~cLanguages() {}
    void Initialize() { if (m_defaultLanguage == &m_emergencyLanguage) setDefaultLanguage(); }
    cSortedVector<cLanguage*, gt_iso> m_languages_iso;
    cSortedVector<cLanguage*, gt_thetvdb> m_languages_thetvdb;
    cSortedVector<cLanguage*, gt_themoviedb> m_languages_themoviedb;

// see https://api.themoviedb.org/3/configuration/languages?api_key=abb01b5a277b9c2c60ec0302d83c5ee9
// see https://api.themoviedb.org/3/configuration/primary_translations?api_key=abb01b5a277b9c2c60ec0302d83c5ee9
    const cSortedVector<cLanguage, std::less<>> m_languages =
{
{ 1, 0, "dan", "dan", "da-DK", "dansk"},
{ 2, 0, "deu", "deu", "de-AT", "deutsch, Österreich"},
{ 3, 0, "deu", "deu", "de-CH", "deutsch, Schweiz"},
{ 4, 1, "deu", "deu", "de-DE", "deutsch, Deutschland"},
{ 5, 1, "eng", "eng", "en-GB", "english GB"},
{ 6, 0, "eng", "eng", "en-US", "english US"},
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

    void setDefaultLanguage(); // set the default language from locale
    void SetDefaultLanguage(const cLanguage *l) { m_defaultLanguage = l; } // use with care, from config.c reading setup values only. Note m_defaultLanguage must not change!

// AdditionalLanguages ( in addition to the default language)
    int numAdditionalLanguages() const { cLanguagesLock l(this); int r = m_AdditionalLanguages.size(); return r; }
    std::vector<int> GetAdditionalLanguages() const { cLanguagesLock l(this); auto r = m_AdditionalLanguages; return r; }
    void SwapAdditionalLanguages(std::vector<int> &additionalLanguages) { cLanguagesLock l(this, true); m_AdditionalLanguages.swap(additionalLanguages); }

// channel languages: if the user maintains explicitly languages for some channels
    std::map<tChannelID, int> GetChannelLanguages() const { cLanguagesLock l(this); auto r = m_channel_language;  return r; }
    void SwapChannelLanguage(std::map<tChannelID, int> &channel_language) { cLanguagesLock l(this, true); m_channel_language.swap(channel_language); }
    const cLanguage *GetLanguage(const tChannelID &channelID) const;  // only return language if this is explicitly maintained for channelID; otherwise: nullptr
    int GetLanguage_n(const tChannelID &channelID) const;  // return language if this is explicitly maintained for channelID; otherwise: default language


    const cLanguage *GetDefaultLanguage() const { return m_defaultLanguage; } // this will ALLWAYS return a valid pointer to cLanguage
    const cLanguage *GetLanguageIso(const char *iso) const;
    const cLanguage *GetLanguageThetvdb(const char *thetvdb, const char *context = nullptr) const;
    const cLanguage *GetLanguageThemoviedb(const char *themoviedb) const;
    // for external use:   always return a valid language
    //    use language explicitly maintaned by user for this channel. If no language for this channel is maintained,
    //    use language from iso code
    //    if there is no language for the iso code (or if there is no iso code), use default language
    const cLanguage *GetLanguageIso(const char *iso, const tChannelID &channelID) const;
    bool isDefaultLanguageThetvdb(const cLanguage *l) const { if (!l) return false; return strcmp(l->m_thetvdb, m_defaultLanguage->m_thetvdb) == 0; }
    const cLanguage *GetLanguage(int l) const;
};

// implement class cLanguagesLock *************************
inline cLanguagesLock::cLanguagesLock(const cLanguages *languages, bool Write) {
#if !defined test_stringhelpers
  languages->stateLock.Lock(m_stateKey, Write);
#endif
}

// implement class cLanguages  ****************************
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
// return ALLWAYS a valid cLanguage pointer
// if l is not in the list: write ERROR to syslog, and return a default language
{
  auto lIt = m_languages.find(l);
  if (lIt != m_languages.end() ) return &(*lIt);
  if (m_languages.size() == 0)
    esyslog3("l = ", l, " m_languages.size() == 0");
  else
    esyslog3("l = ", l, " not found");
  return &m_emergencyLanguage;
}
inline const cLanguage *cLanguages::GetLanguage(int l) const
{
  const cLanguage *result = GetLanguage_int(l);
  if (result->Prio() < 0) return GetLanguage_int(-result->Prio() );
  return result;
}
inline int cLanguages::GetLanguage_n(const tChannelID &channelID) const {
// only return language if this is explicitly maintained for channelID; otherwise: defaultLanguage
  int lang;
  {
    cLanguagesLock lr(this);
    auto l = m_channel_language.find(channelID);
    if (l == m_channel_language.end()) lang = m_defaultLanguage->Id();
    else lang = l->second;
  }
  return lang;
}
inline const cLanguage *cLanguages::GetLanguage(const tChannelID &channelID) const {
// only return language if this is explicitly maintained for channelID; otherwise: nullptr
  int lang;
  {
    cLanguagesLock lr(this);
    auto l = m_channel_language.find(channelID);
    if (l == m_channel_language.end()) lang = -1;
    else lang = l->second;
  }
  if (lang == -1) return nullptr;
  auto r = m_languages.find(lang);
  if(r != m_languages.end()) return &(*r);
  esyslog3("language ", lang, " not found");
  return nullptr;
}
inline const cLanguage *cLanguages::GetLanguageIso(const char *iso, const tChannelID &channelID) const {
  const cLanguage *l = GetLanguage(channelID);
  if (l) return l;
  l = GetLanguageIso(iso);
  if (l) return l;
  return GetDefaultLanguage();
}

inline void cLanguages::setDefaultLanguage() {
  std::string loc = setlocale(LC_NAME, NULL);
  size_t index = loc.find_first_of("_");
  if (index != 2) {
    esyslog3("language ", loc, " index = ", index, " use en-GB as default language");
    m_defaultLanguage = GetLanguage(5); // en-GB
    return;
  }
  const cLanguage *li = nullptr;
  const cLanguage *lc = nullptr;
  for (const cLanguage &lang: m_languages) {
    if (strncmp(lang.m_themoviedb, loc.c_str(), 2) != 0) continue;
    if (!li) li = &lang;
    if (strncmp(lang.m_themoviedb+3, loc.c_str()+3, 2) == 0) if (!lc) lc = &lang;
  }
  if (li == nullptr) {
    esyslog3("language ", loc, " not found, use en-GB as default language");
    m_defaultLanguage = GetLanguage(5); // en-GB
    return;
  }
  if (lc != nullptr) m_defaultLanguage = lc;
  else m_defaultLanguage = li;
  esyslog("tvscraper: set default language to %s", m_defaultLanguage->getNames().c_str() );
}
#endif //__TVSCRAPER_LANGUAGES_H

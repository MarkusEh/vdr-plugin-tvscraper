#ifndef __TVSCRAPER_SETUP_H
#define __TVSCRAPER_SETUP_H

template<class T = int, class Allocator = std::allocator<T> >
class cMapBi: public std::vector<T, Allocator> {
  typedef typename std::vector<T, Allocator>::size_type size_type;
  typedef typename std::vector<T, Allocator>::reference reference;
  typedef typename std::vector<T, Allocator>::const_reference const_reference;
  static const size_type npos = std::string_view::npos;
  public:
    bool isSecond(const T &second) const {
      for (const T &elem: *this) if (elem == second) return true;
      return false;
    }
    void push_back_if_new(const T &second) { if (!isSecond(second)) this->push_back(second); }
    size_type getFirst(const T &second) const {
      for (size_type i= 0; i < this->size(); ++i) if ((*this)[i] == second) return i;
      return npos;
    }
    const_reference getSecond(size_type first) const { return this->at(first); }
};

using namespace std;

class langOsd {
  public:
    langOsd(const std::vector<std::pair<int, std::string>> *lang_text):
      m_lang_text(lang_text) { m_osdTexts = new const char*[lang_text->size()];}
    ~langOsd() { delete [] m_osdTexts; }
    void clear() { m_numLang = 0; m_osdMap.clear(); m_selectedLanguage.clear(); }
    void addLanguage(int lang) { m_osdTexts[m_numLang] = getLangText(lang); m_maxOsdTextLength = std::max(m_maxOsdTextLength, (int)strlen(m_osdTexts[m_numLang])); m_osdMap.push_back(lang); ++m_numLang; }
    void addLine(int currentLanguage) { m_selectedLanguage.push_back(m_osdMap.getFirst(currentLanguage)); }
    void addLineNoSelection() { m_selectedLanguage.push_back(-1); }
    int getLanguage(int line) const { return m_osdMap.getSecond(m_selectedLanguage[line]); }
    int m_numLang = 0;  // number of languages in this menu
    cMapBi<int> m_osdMap;
    std::vector<int> m_selectedLanguage; // for the OSD, one entry for each line
    const char **m_osdTexts;
    int m_maxOsdTextLength = 0;  // length of longest osd text
  private:
    const char *getLangText(int lang) const { for (int i = 0; i < (int)m_lang_text->size(); i++) if ((*m_lang_text)[i].first == lang) return (*m_lang_text)[i].second.c_str(); return "no lang text";}
    const std::vector<std::pair<int, std::string>> *m_lang_text;
};

class cSetupChannel {
  // everything about a channel which is needed in the setup:
  friend class cSetupChannels;
  public:
    cSetupChannel(const cChannel *channel, unsigned channel_number_width);
    cSetupChannel(const cRecording *recording, unsigned channel_number_width);
    cSetupChannel(cSv group_separator);
  private:
    bool m_groupSep;
    tChannelID m_channelID;
    int m_number;           // as used by VDR. < 0 for channels only used in recordings
    std::string m_name;
    const cLanguage *m_language;  // the language of the EPG provided by the station

// Data as saved in setup.conf:
    int m_setup_hd;     // sd/hd/uhd
    int m_setup_scrape; // 1 = yes/, 0 = no
    int m_setup_lang; // language as saved in setup.conf. -1 if no language is saved in setup.conf for this channel
    void set_data_from_setup();

// values displayed / changes in osd
    int m_osd_hd;     // sd/hd/uhd
    int m_osd_scrape; // 1 = yes/, 0 = no
    int m_osd_lang;
};
class cTVScraperSetup;
class cSetupChannels {
  public:
    cSetupChannels();
    ~cSetupChannels() { clear_language_select_options(); }
    bool is_in_list(tChannelID channelID) const;
    unsigned firstColumnWidth() { return m_channel_number_width + m_max_channel_name_length + 1; }
    cOsdMenu *getSetupMenuLanguage(const char *Title, int num_additional_languages, langOsd &additional_languages);
    cOsdMenu *getSetupMenuHD(const char *Title);
    cOsdMenu *getSetupMenuScrape(const char *Title);
    void Store(cTVScraperSetup *TVScraperSetup);
  private:
    void clear_language_select_options();
    unsigned m_channel_number_width;
    unsigned m_max_channel_name_length;
    std::vector<cSetupChannel> m_setup_channels;
    cSortedVector<tChannelID> m_sorted_channels;
// for languages:
    std::vector<std::string> m_texts_from_epg;
    std::vector<const char **> m_language_select_options;
    cSortedVector<int> m_setup_languages; // all languages saved in setup
    cMapBi<int> m_map_lang_osd;
    bool m_SetupMenu_displayed_language = false;
// for SD/HD
    bool m_SetupMenu_displayed_hd = false;
    static const int m_numOptions_hd = 3;
    const char *m_select_options_hd[m_numOptions_hd];
    bool m_SetupMenu_displayed_scrape = false;
    const char *m_select_options_scrape[2];
};


class cTVScraperSetup: public cMenuSetupPage {
  friend class cSetupChannels;
  public:
    cTVScraperSetup(cTVScraperWorker *workerThread, const cTVScraperDB &db);
    virtual ~cTVScraperSetup() { delete m_setupChannels; }
    const char *m_Set_language_for_each_channel;
    const char *m_Configure_channels_to_be_scraped;
    const char *m_HD_channels;
    const char *m_Recording_folders_to_improve;
    const char *m_TV_shows_to_record;
    const char *m_Trigger_scraping_Video_Directory;
    const char *m_Trigger_EPG_scraping;
  private:
    cTVScraperWorker *worker;
    const cTVScraperDB &m_db;
    cSetupChannels *m_setupChannels = nullptr;
    cSetupChannels *SetupChannels() { if (!m_setupChannels) m_setupChannels = new cSetupChannels(); return m_setupChannels; }
    std::vector<std::pair<int, std::string>> m_all_languages;
// data changed in the menu. Will be initialized from config, and written back to config and setup.conf if changes are confirmed with "OK"
    langOsd langDefault;
    langOsd langAdditional;
    int m_enableDebug;
    int m_enableAutoTimers;
    std::vector<int> m_selectedRecordingFolders;
    std::vector<int> m_selectedTV_Shows;
    int m_NumberOfAdditionalLanguages;
    int m_writeEpisodeToEpg;
// END data changed in the menu
    int m_recordings_width;
    cSortedVector<std::string> m_allRecordingFolders;
    cSortedVector<int> m_allTV_Shows;
    void Setup(void);
    std::string StoreExcludedRecordingFolders();
    std::string StoreTV_Shows();
    cSortedVector<int> getAllTV_Shows();
    cSortedVector<std::string> getAllRecordingFolders(int &max_width);
  protected:
    virtual eOSState ProcessKey(eKeys Key);
    virtual void Store(void);
};

class cTVScraperOsdMenu: public cOsdMenu {
  public:
    cTVScraperOsdMenu(const char *Title, int c0 = 0, int c1 = 0, int c2 = 0, int c3 = 0, int c4 = 0):
      cOsdMenu(Title, c0, c1, c2, c3, c4) {}
    virtual ~cTVScraperOsdMenu() {}
  protected:
    virtual eOSState ProcessKey(eKeys Key);
};

class cTVScraperListSetup: public cOsdMenu {
  public:
    cTVScraperListSetup(vector<int> &listSelections, const cSortedVector<std::string> &listEntries, const char *headline, int width);
    virtual ~cTVScraperListSetup();
  protected:
    virtual eOSState ProcessKey(eKeys Key);
};

class cTVScraperTV_ShowsSetup: public cOsdMenu {
  public:
    cTVScraperTV_ShowsSetup(vector<int> &listSelections, const cSortedVector<int> &TV_Shows, const cTVScraperDB &db);
    virtual ~cTVScraperTV_ShowsSetup();
  protected:
    virtual eOSState ProcessKey(eKeys Key);
};

#endif //__TVSCRAPER_SETUP_H

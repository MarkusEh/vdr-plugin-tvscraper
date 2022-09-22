#ifndef __TVSCRAPER_SETUP_H
#define __TVSCRAPER_SETUP_H

using namespace std;

class mapIntBi {
  public:
    mapIntBi() {}
    void clear() { m_map.clear(); }
    bool insert(int first, int second);
    int getFirst(int second) const;
    int getSecond(int first) const;
    bool isSecond(int second) const; // true, if second is in m_map (as second)
  private:
    vector<int> m_map;
};
class langOsd {
  public:
    langOsd(const std::vector<std::pair<int, std::string>> *lang_text):
      m_lang_text(lang_text) { m_osdTexts = new const char*[lang_text->size()];}
    ~langOsd() { delete [] m_osdTexts; }
    void clear() { m_numLang = 0; m_osdMap.clear(); m_selectedLanguage.clear(); }
    void addLanguage(int lang) { m_osdTexts[m_numLang] = getLangText(lang); m_osdMap.insert(m_numLang++, lang);}
    void addLine(int currentLanguage) { m_selectedLanguage.push_back(m_osdMap.getFirst(currentLanguage)); }
    int getLanguage(int line) const { return m_osdMap.getSecond(m_selectedLanguage[line]); }
    int m_numLang = 0;
    mapIntBi m_osdMap;
    std::vector<int> m_selectedLanguage; // for the OSD, one entry for each line
    const char **m_osdTexts;
  private:
    const char *getLangText(int lang) const { for (int i = 0; i < (int)m_lang_text->size(); i++) if ((*m_lang_text)[i].first == lang) return (*m_lang_text)[i].second.c_str(); return "no lang text";}
    const std::vector<std::pair<int, std::string>> *m_lang_text;
};

class cTVScraperSetup: public cMenuSetupPage {
    public:
        cTVScraperSetup(cTVScraperWorker *workerThread, const cTVScraperDB &db);
        virtual ~cTVScraperSetup();      
    private:
        cTVScraperWorker *worker;
        const cTVScraperDB &m_db;
        std::vector<std::pair<int, std::string>> m_all_languages;
// data changed in the menu. Will be initialized from config, and written back to config and setup.conf if changes are confirmed with "OK"
        langOsd langDefault;
        langOsd langAdditional;
        langOsd langChannels;
        int m_enableDebug;
        int m_enableAutoTimers;
        vector<int> channelsScrap;
        vector<int> channelsHD;
        std::vector<int> m_selectedRecordingFolders;
        std::vector<int> m_selectedTV_Shows;
        int m_NumberOfAdditionalLanguages;
// END data changed in the menu
        map<tChannelID, int> m_allChannels;  // second is channel number - 1
        map<tChannelID, const char*> m_recChannels;  // second is the channel name
        std::vector<const char*> m_channelNames;
        int m_recordings_width;
        std::set<std::string> m_allRecordingFolders;
        set<int> m_allTV_Shows;
        void Setup(void);
        std::string StoreExcludedRecordingFolders();
        std::string StoreTV_Shows();
        std::set<int> getAllTV_Shows();
        std::set<std::string> getAllRecordingFolders(int &max_width);
        std::set<tChannelID> GetChannelsFromSetup(const vector<int> &channels);
        map<tChannelID, int> GetChannelsFromSetup(const vector<int> &channels, const mapIntBi *langIds);
    protected:
        virtual eOSState ProcessKey(eKeys Key);
        virtual void Store(void);
};

class cTVScraperChannelSetup : public cOsdMenu {
    public:
        cTVScraperChannelSetup(const vector<const char*> &channelNames, vector<int> *channels, const char *headline, const char *null, const char *eins);
        cTVScraperChannelSetup(const vector<const char*> &channelNames, vector<int> *channels, const char *headline, int numOptions, const char**selectOptions);
        virtual ~cTVScraperChannelSetup();       
    private:
        void Setup(const vector<const char*> &channelNames, vector<int> *channels, int numOptions, const char**selectOptions);
        const char **m_selectOptions = NULL;
    protected:
        virtual eOSState ProcessKey(eKeys Key);
};

class cTVScraperListSetup: public cOsdMenu {
  public:
    cTVScraperListSetup(vector<int> &listSelections, const std::set<string> &listEntries, const char *headline, int width);
    virtual ~cTVScraperListSetup();
  protected:
    virtual eOSState ProcessKey(eKeys Key);
};

class cTVScraperTV_ShowsSetup: public cOsdMenu {
  public:
    cTVScraperTV_ShowsSetup(vector<int> &listSelections, const std::set<int> &TV_Shows, const cTVScraperDB &db);
    virtual ~cTVScraperTV_ShowsSetup();
  protected:
    virtual eOSState ProcessKey(eKeys Key);
};

#endif //__TVSCRAPER_SETUP_H

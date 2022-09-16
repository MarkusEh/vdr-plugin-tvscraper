#ifndef __TVSCRAPER_SETUP_H
#define __TVSCRAPER_SETUP_H

using namespace std;

class cTVScraperSetup: public cMenuSetupPage {
    public:
        cTVScraperSetup(cTVScraperWorker *workerThread, const cTVScraperDB &db);
        virtual ~cTVScraperSetup();      
    private:
// data changed in the menu. Will be initialized from config, and wirtten back to condig and setup.conf if changes are confirmed with "OK"
        int m_enableDebug;
        int m_enableAutoTimers;
        vector<int> channelsScrap;
        vector<int> channelsHD;
        std::vector<int> m_selectedRecordingFolders;
        std::vector<int> m_selectedTV_Shows;
        int m_selected_language_line;
        int m_NumberOfAdditionalLanguages;
        int *m_AdditionalLanguages;
// END data changed in the menu
        int m_recordings_width;
        std::set<std::string> m_allRecordingFolders;
        set<int> m_allTV_Shows;
// for languages
        std::vector<std::pair<int, std::string>> m_all_languages;
        const char **m_language_strings;
        cTVScraperWorker *worker;
        const cTVScraperDB &m_db;
        void Setup(void);
        std::string StoreExcludedRecordingFolders();
        std::string StoreTV_Shows();
        std::set<int> getAllTV_Shows();
    protected:
        virtual eOSState ProcessKey(eKeys Key);
        virtual void Store(void);
};

class cTVScraperChannelSetup : public cOsdMenu {
    public:
        cTVScraperChannelSetup(vector<int> *channelsScrap, bool hd);
        virtual ~cTVScraperChannelSetup();       
    private:
        vector<int> *channelsScrap;
        void Setup(bool hd);
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

#ifndef __TVSCRAPER_SETUP_H
#define __TVSCRAPER_SETUP_H

using namespace std;

class cTVScraperSetup: public cMenuSetupPage {
    public:
        cTVScraperSetup(cTVScraperWorker *workerThread, const cTVScraperDB &db);
        virtual ~cTVScraperSetup();      
    private:
        vector<int> channelsScrap;
        vector<int> channelsHD;
        std::vector<int> m_selectedRecordingFolders;
        int m_recordings_width;
        std::set<std::string> m_allRecordingFolders;
        std::vector<int> m_selectedTV_Shows;
        set<int> m_allTV_Shows;
        cTVScraperWorker *worker;
        const cTVScraperDB &m_db;
        void Setup(void);
        std::string StoreExcludedRecordingFolders();
        std::string StoreChannels(const vector<int> &channels, bool hd);
        std::string StoreTV_Shows();
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

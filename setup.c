#include "setup.h"

using namespace std;

std::set<std::string> getAllRecordingFolders(int &max_width) {
  std::set<std::string> result;
  max_width = 0;
#if APIVERSNUM < 20301
  for (cRecording *rec = Recordings.First(); rec; rec = Recordings.Next(rec))
#else
  LOCK_RECORDINGS_READ;
  for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec))
#endif
  {
    const char *pos_delim = strrchr(rec->Name(), '~');
    if (pos_delim != 0) {
      result.insert(std::string(rec->Name(), pos_delim - rec->Name() ));
      max_width = std::max(max_width, (int)(pos_delim - rec->Name()) );
    }
  }
  return result;
}
std::set<int> getAllTV_Shows(const cTVScraperDB &db) {
  std::set<int> result;
//const char sql[] = "select tv_id from tv2";
  const char sql[] = "select movie_tv_id from recordings2 where season_number != -100";
  int tvID;
  for (sqlite3_stmt *statement = db.QueryPrepare(sql, "");                                                                        
       db.QueryStep(statement, "i", &tvID);) {                                                                                                         
    result.insert(tvID);
  }
  for (const int &id: config.GetTV_Shows() ) result.insert(id);
  return result;
}
/* cTVScraperSetup */

cTVScraperSetup::cTVScraperSetup(cTVScraperWorker *workerThread, const cTVScraperDB &db):
  m_db(db)
  {
  worker = workerThread;
#if APIVERSNUM < 20301
  for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel))
#else
  LOCK_CHANNELS_READ;
  for (const cChannel *channel = Channels->First(); channel; channel = Channels->Next(channel))
#endif
  if (!channel->GroupSep()) {
    channelsScrap.push_back(config.ChannelActive(channel)?1:0);
    channelsHD.push_back(config.ChannelHD(channel)?1:0);
  }
// Recording folders
  m_allRecordingFolders = getAllRecordingFolders(m_recordings_width);
  for (const std::string &recordingFolder: m_allRecordingFolders)
    m_selectedRecordingFolders.push_back(config.recordingFolderSelected(recordingFolder)?1:0);
// TV Shows
  m_allTV_Shows = getAllTV_Shows(db);
  for (const int &TV_Show: m_allTV_Shows)
    m_selectedTV_Shows.push_back(config.TV_ShowSelected(TV_Show)?1:0);
  Setup();
}

cTVScraperSetup::~cTVScraperSetup() {
}


void cTVScraperSetup::Setup(void) {
    int currentItem = Current();
    Clear();
    Add(new cOsdItem(tr("Configure channels to be scraped")));
    Add(new cMenuEditBoolItem(tr("Create timers to improve and complement recordings"), &config.enableAutoTimers));
    if (config.enableAutoTimers) {
      Add(new cOsdItem(tr("HD channels")));
      Add(new cOsdItem(tr("Recording folders to improve")));
      Add(new cOsdItem(tr("TV shows to record")));
    }
    Add(new cOsdItem(tr("Trigger scraping Video Directory")));
    Add(new cOsdItem(tr("Trigger EPG scraping")));
    Add(new cMenuEditBoolItem(tr("Enable Debug Logging"), &config.enableDebug));
    
    SetCurrent(Get(currentItem));
    Display();
}

eOSState cTVScraperSetup::ProcessKey(eKeys Key) {
    bool hadSubMenu = HasSubMenu();
    if (hadSubMenu && Key == kOk)
        Store();
    eOSState state = cMenuSetupPage::ProcessKey(Key);
    if (!hadSubMenu && (Key == kOk)) {
        const char* ItemText = Get(Current())->Text();
        if (strcmp(ItemText, tr("Configure channels to be scraped")) == 0)
            state = AddSubMenu(new cTVScraperChannelSetup(&channelsScrap, false));
        else if (strcmp(ItemText, tr("HD channels")) == 0)
            state = AddSubMenu(new cTVScraperChannelSetup(&channelsHD, true));
        else if (strcmp(ItemText, tr("Recording folders to improve")) == 0)
            state = AddSubMenu(new cTVScraperListSetup(m_selectedRecordingFolders, m_allRecordingFolders, tr("Recording folders to improve"), m_recordings_width));
        else if (strcmp(ItemText, tr("TV shows to record")) == 0)
            state = AddSubMenu(new cTVScraperTV_ShowsSetup(m_selectedTV_Shows, m_allTV_Shows, m_db));
        else if (strcmp(ItemText, tr("Trigger scraping Video Directory")) == 0) {
            Skins.Message(mtInfo, "Scraping Video Directory started");
            worker->InitVideoDirScan();
            state = osContinue;
        } else if (strcmp(ItemText, tr("Trigger EPG scraping")) == 0) {
            Skins.Message(mtInfo, "EPG Scraping started");
            worker->InitManualScan();
            state = osContinue;
        }
    }
    return state;
}

std::string cTVScraperSetup::StoreChannels(const vector<int> &channels, bool hd) {
  config.ClearChannels(hd);
  stringstream channelsStrm;
  int numChannels = channels.size();
  for (int i=0; i<numChannels; i++) {
    if (channels[i] == 1) {
#if APIVERSNUM < 20301
      cChannel *channel = Channels.GetByNumber(i+1);
#else
      LOCK_CHANNELS_READ;
      const cChannel *channel = Channels->GetByNumber(i+1);
#endif
      if (channel) {
        string channelID = *(channel->GetChannelID().ToString());
        channelsStrm << channelID << ";";
        config.AddChannel(channelID, hd);
      }
    }
  }
  return channelsStrm.str();
}

std::string cTVScraperSetup::StoreExcludedRecordingFolders() {
  config.ClearExcludedRecordingFolders();
  stringstream foldersStrm;
  int i = 0;
  for (const std::string &recordingFolder: m_allRecordingFolders)
    if (m_selectedRecordingFolders[i++] == 0) {
      config.AddExcludedRecordingFolder(recordingFolder);
      foldersStrm << recordingFolder << "/";
    }
  return foldersStrm.str();
}

std::string cTVScraperSetup::StoreTV_Shows() {
  config.ClearTV_Shows();
  stringstream TV_ShowsStrm;
  int i = 0;
  for (const int &TV_Show: m_allTV_Shows)
    if (m_selectedTV_Shows[i++] == 1) {
      config.AddTV_Show(TV_Show);
      TV_ShowsStrm << TV_Show << ";";
    }
  return TV_ShowsStrm.str();
}

void cTVScraperSetup::Store(void) {
    SetupStore("ScrapChannels", StoreChannels(channelsScrap, false).c_str());
    SetupStore("HDChannels", StoreChannels(channelsHD, true).c_str());
    SetupStore("ExcludedRecordingFolders", StoreExcludedRecordingFolders().c_str());
    SetupStore("TV_Shows", StoreTV_Shows().c_str());
    SetupStore("enableDebug", config.enableDebug);
    SetupStore("enableAutoTimers", config.enableAutoTimers);
}


/* cTVScraperChannelSetup */

cTVScraperChannelSetup ::cTVScraperChannelSetup (vector<int> *channelsScrap, bool hd) : cOsdMenu(hd?tr("HD channels"):tr("Configure channels to be scraped"), 30) {
    this->channelsScrap = channelsScrap;
    SetMenuCategory(mcSetupPlugins);
    Setup(hd);
}

cTVScraperChannelSetup ::~cTVScraperChannelSetup () {
}


void cTVScraperChannelSetup ::Setup(bool hd) {
    int currentItem = Current();
    Clear();
    int i=0;
#if APIVERSNUM < 20301
    for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel)) {
#else
    LOCK_CHANNELS_READ;
    for (const cChannel *channel = Channels->First(); channel; channel = Channels->Next(channel)) {
#endif
        if (!channel->GroupSep()) {
            Add(new cMenuEditBoolItem(channel->Name(), &channelsScrap->at(i), hd?tr("SD"):tr("don't scrap"), hd?tr("HD"):tr("scrap")));
            i++;
        }
    }
    SetCurrent(Get(currentItem));
    Display();
}

eOSState cTVScraperChannelSetup ::ProcessKey(eKeys Key) {
    eOSState state = cOsdMenu::ProcessKey(Key);
    switch (Key) {
        case kOk:
            return osBack;
        default:
            break;
    }
    return state;
}

/* cTVScraperListSetup */
cTVScraperListSetup::cTVScraperListSetup(vector<int> &listSelections, const std::set<string> &listEntries, const char *headline, int width): cOsdMenu(headline, width + 2, 5) {
  SetMenuCategory(mcSetupPlugins);
  int currentItem = Current();
  Clear();
  int i = 0;
  for (const std::string &listEntry: listEntries)
    Add(new cMenuEditBoolItem(listEntry.c_str(), &(listSelections[i++]), tr("No"), tr("Yes")));
  SetCurrent(Get(currentItem));
  Display();
}
cTVScraperListSetup ::~cTVScraperListSetup () {
}
eOSState cTVScraperListSetup::ProcessKey(eKeys Key) {
  eOSState state = cOsdMenu::ProcessKey(Key);
  if (Key == kOk) return osBack;
  return state;
}

/* cTVScraperTV_ShowsSetup */
cTVScraperTV_ShowsSetup::cTVScraperTV_ShowsSetup(vector<int> &listSelections, const std::set<int> &TV_Shows, const cTVScraperDB &db): cOsdMenu(tr("Create timers for missing episodes of this TV show?"), 40, 5) {
  const char sql[] = "select tv_name from tv2 where tv_id = ?";
  SetMenuCategory(mcSetupPlugins);
  int currentItem = Current();
  Clear();
  int i = 0;
  for (const int &TV_show: TV_Shows) {
    sqlite3_stmt *statement = db.QueryPrepare(sql, "i", TV_show);
    const char *name;
    if (db.QueryStep(statement, "s", &name))
      Add(new cMenuEditBoolItem(name, &(listSelections[i]), tr("No"), tr("Yes")));
    sqlite3_finalize(statement);
    i++;
  }
  SetCurrent(Get(currentItem));
  Display();
}
cTVScraperTV_ShowsSetup ::~cTVScraperTV_ShowsSetup () {
}
eOSState cTVScraperTV_ShowsSetup::ProcessKey(eKeys Key) {
  eOSState state = cOsdMenu::ProcessKey(Key);
  if (Key == kOk) return osBack;
  return state;
}

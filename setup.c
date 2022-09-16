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
std::set<int> cTVScraperSetup::getAllTV_Shows() {
  std::set<int> result;
//const char sql[] = "select tv_id from tv2";
  const char sql[] = "select DISTINCT movie_tv_id from recordings2 where season_number != -100";
  int tvID;
  for (sqlite3_stmt *statement = m_db.QueryPrepare(sql, "");                                                                        
       m_db.QueryStep(statement, "i", &tvID);) {                                                                                                         
    result.insert(tvID);
  }
  cTVScraperConfigLock l;
  for (const int &id: config.m_TV_Shows ) result.insert(id);
  return result;
}

/* cTVScraperSetup */
cTVScraperSetup::cTVScraperSetup(cTVScraperWorker *workerThread, const cTVScraperDB &db):
  m_db(db)
  {
  if (config.enableDebug) esyslog("tvscraper: Start of cTVScraperSetup::cTVScraperSetup");
  worker = workerThread;
// copy data from config to local data, which will be directly changed in OSD
  m_enableDebug = config.enableDebug;
  m_enableAutoTimers = config.m_enableAutoTimers;
  {
#if APIVERSNUM < 20301
    for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel))
#else
    LOCK_CHANNELS_READ;
    for (const cChannel *channel = Channels->First(); channel; channel = Channels->Next(channel))
#endif
    if (!channel->GroupSep()) {
      channelsScrap.push_back(config.ChannelActive(channel->GetChannelID())?1:0);
      channelsHD.push_back(config.ChannelHD(channel->GetChannelID())?1:0);
    }
  }
  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after ChannelActive / ChannelHD");
// Recording folders
  m_allRecordingFolders = getAllRecordingFolders(m_recordings_width);
  for (const std::string &recordingFolder: m_allRecordingFolders)
    m_selectedRecordingFolders.push_back(config.recordingFolderSelected(recordingFolder)?1:0);
  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after selectedRecordingFolders");
// TV Shows
  m_allTV_Shows = getAllTV_Shows();
  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after TV shows (get from db)");
  for (const int &TV_Show: m_allTV_Shows)
    m_selectedTV_Shows.push_back(config.TV_ShowSelected(TV_Show)?1:0);
  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after TV shows (final)");
// languages. Note m_languages is const, no locking required
  for (const cLanguage &lang: config.m_languages)
    m_all_languages.push_back({lang.m_id, lang.getNames()});
  m_language_strings = new const char*[config.m_languages.size()];
  int i = 0;
  m_selected_language_line = 5;
  for (const auto &l: m_all_languages) {
    if (l.first == config.getDefaultLanguage() ) m_selected_language_line = i;
    m_language_strings[i++] = l.second.c_str();
  }
  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup num all languages: %li", config.m_languages.size());
  {
    cTVScraperConfigLock l;
    m_NumberOfAdditionalLanguages = config.m_AdditionalLanguages.size();
    m_AdditionalLanguages = new int[config.m_languages.size()];  // this is the maximum
    i = 0;
    for (const int &addLang: config.m_AdditionalLanguages)
      m_AdditionalLanguages[i++] = addLang;
    for (; i < (int)config.m_languages.size(); i++)
      m_AdditionalLanguages[i++] = 4;
  }

  Setup();
}

cTVScraperSetup::~cTVScraperSetup() {
  delete[] m_language_strings;
  delete[] m_AdditionalLanguages;
}
void cTVScraperSetup::Setup(void) {
    int currentItem = Current();
    Clear();
    Add(new cOsdItem(tr("Configure channels to be scraped")));
    Add(new cMenuEditStraItem(tr("Main language"), &m_selected_language_line, config.m_languages.size(), m_language_strings));
    Add(new cMenuEditIntItem(tr("Number of additional languages"), &m_NumberOfAdditionalLanguages));
    for (int i = 0; i < m_NumberOfAdditionalLanguages; i++)
      Add(new cMenuEditStraItem(tr("Additional language"), &m_AdditionalLanguages[i], config.m_languages.size(), m_language_strings));
    Add(new cMenuEditBoolItem(tr("Create timers to improve and complement recordings"), &m_enableAutoTimers));
    if (m_enableAutoTimers) {
      Add(new cOsdItem(tr("HD channels")));
      Add(new cOsdItem(tr("Recording folders to improve")));
      Add(new cOsdItem(tr("TV shows to record")));
    }
    Add(new cOsdItem(tr("Trigger scraping Video Directory")));
    Add(new cOsdItem(tr("Trigger EPG scraping")));
    Add(new cMenuEditBoolItem(tr("Enable Debug Logging"), &m_enableDebug));
    
    SetCurrent(Get(currentItem));
    Display();
}

eOSState cTVScraperSetup::ProcessKey(eKeys Key) {
    bool hadSubMenu = HasSubMenu();
    if (hadSubMenu && Key == kOk)
        Store();
    eOSState state = cMenuSetupPage::ProcessKey(Key);
    if (!hadSubMenu && Current() == 2 && (Key == kLeft || Key == kRight)) Setup();  // refresh screen if "Create timers to ..." is changed
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

std::set<tChannelID> GetChannelsFromSetup(const vector<int> &channels) {
  std::set<tChannelID> result;
  int numChannels = channels.size();
  for (int i=0; i<numChannels; i++) {
    if (channels[i] == 1) {
#if APIVERSNUM < 20301
      cChannel *channel = Channels.GetByNumber(i+1);
#else
      LOCK_CHANNELS_READ;
      const cChannel *channel = Channels->GetByNumber(i+1);
#endif
      if (channel) result.insert(channel->GetChannelID() );
    }
  }
  return result;
}

template<class T>
std::set<T> menuSelectionsToSet(const std::set<T> &allItems, const std::vector<int> &selectedItems) {
  std::set<T> result;
  int i = 0;
  for (const T &item: allItems)
    if (selectedItems[i++] == 0) {
      result.insert(item);
    }
  return result;
}

void cTVScraperSetup::Store(void) {
  {
    set<tChannelID> channels = GetChannelsFromSetup(channelsScrap);
    set<tChannelID> hd_channels = GetChannelsFromSetup(channelsHD);
// note: GetChannelsFromSetup will call LOCK_CHANNELS_READ; -> Locking order!!! -> we lock after these calls
    cTVScraperConfigLock lw(true);
    config.enableDebug = m_enableDebug;
    config.m_enableAutoTimers = m_enableAutoTimers;
    config.m_channels = std::move(channels);
    config.m_hd_channels = std::move(hd_channels);
    config.m_defaultLanguage = m_all_languages[m_selected_language_line].first;
    config.m_AdditionalLanguages.clear();
    for (int i = 0; i < m_NumberOfAdditionalLanguages; i++)
      config.m_AdditionalLanguages.insert(m_all_languages[m_AdditionalLanguages[i]].first);
    config.m_excludedRecordingFolders = menuSelectionsToSet<string>(m_allRecordingFolders, m_selectedRecordingFolders);
    config.m_TV_Shows = menuSelectionsToSet<int>(m_allTV_Shows, m_selectedTV_Shows);
  }

  cTVScraperConfigLock lr;
// getStringFromSet<> sets no Locks
  SetupStore("ScrapChannels", getStringFromSet<tChannelID>(config.m_channels).c_str());
  SetupStore("HDChannels", getStringFromSet<tChannelID>(config.m_hd_channels).c_str());
  SetupStore("ExcludedRecordingFolders", getStringFromSet<string>(config.m_excludedRecordingFolders).c_str());
  SetupStore("TV_Shows", getStringFromSet<int>(config.m_TV_Shows).c_str());
  SetupStore("enableDebug", config.enableDebug);
  SetupStore("enableAutoTimers", config.m_enableAutoTimers);
  SetupStore("defaultLanguage", config.getDefaultLanguage() );
  SetupStore("additionalLanguages", getStringFromSet<int>(config.m_AdditionalLanguages).c_str() );
}

/* cTVScraperChannelSetup */

cTVScraperChannelSetup ::cTVScraperChannelSetup (vector<int> *channelsScrap, bool hd) : cOsdMenu(hd?tr("HD channels"):tr("Configure channels to be scraped"), 30) {
    this->channelsScrap = channelsScrap;
    SetMenuCategory(mcSetupPlugins);
    Setup(hd);
}

cTVScraperChannelSetup ::~cTVScraperChannelSetup () {
}


void cTVScraperChannelSetup::Setup(bool hd) {
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
    Add(new cMenuEditBoolItem(listEntry.c_str(), &(listSelections[i++])));
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
      Add(new cMenuEditBoolItem(name, &(listSelections[i]) ));
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

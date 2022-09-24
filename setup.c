#include "setup.h"

using namespace std;

std::set<std::string> cTVScraperSetup::getAllRecordingFolders(int &max_width) {
  std::set<std::string> result;
  m_recChannels.clear();
  max_width = 0;
#if APIVERSNUM < 20301
  for (cRecording *rec = Recordings.First(); rec; rec = Recordings.Next(rec))
#else
  LOCK_RECORDINGS_READ;
  for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec))
#endif
  {
    if (rec->Info() ) m_recChannels.insert({rec->Info()->ChannelID(), rec->Info()->ChannelName()});
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
// add all TV Shows from recordings & from config, where we create recordings for missing episodes.
// don't add TV Shows from tv2 / EPG events, as these are too many
// add all TV Shows from recordings
  const char sql[] = "select DISTINCT movie_tv_id from recordings2 where season_number != -100";
  int tvID;
  for (sqlite3_stmt *statement = m_db.QueryPrepare(sql, "");
       m_db.QueryStep(statement, "i", &tvID);) result.insert(tvID);
// add all TV Shows from config, where we create recordings for missing episodes
  cTVScraperConfigLock l;
  for (const int &id: config.m_TV_Shows ) result.insert(id);
  return result;
}

std::vector<std::pair<int, std::string>> getLanguageTexts() {
  std::vector<std::pair<int, std::string>> result;
  for (const cLanguage &lang: config.m_languages)
    result.push_back({lang.m_id, lang.getNames()});
  return result;
}
/* cTVScraperSetup */
cTVScraperSetup::cTVScraperSetup(cTVScraperWorker *workerThread, const cTVScraperDB &db):
  worker(workerThread),
  m_db(db),
  m_all_languages(getLanguageTexts()),
  langDefault(&m_all_languages),
  langAdditional(&m_all_languages),
  langChannels(&m_all_languages)
  {
  if (config.enableDebug) esyslog("tvscraper: Start of cTVScraperSetup::cTVScraperSetup");
// copy data from config to local data, which will be directly changed in OSD
  m_enableDebug = config.enableDebug;
  m_enableAutoTimers = config.m_enableAutoTimers;
// Recording folders
  m_allRecordingFolders = getAllRecordingFolders(m_recordings_width);
  for (const std::string &recordingFolder: m_allRecordingFolders)
    m_selectedRecordingFolders.push_back(config.recordingFolderSelected(recordingFolder)?1:0);
  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after selectedRecordingFolders");
// languages  =============================================
  if (m_all_languages.size() == 0) {
    esyslog("tvscraper: ERROR cTVScraperSetup::cTVScraperSetup m_all_languages.size() == 0");
    return;
  }
// default language
  int defaultLanguage;
  { cTVScraperConfigLock l; defaultLanguage = config.m_defaultLanguage; }
  bool defaultLanguageInList = false;
  for (const auto &l: m_all_languages)
    if (l.first == defaultLanguage) defaultLanguageInList = true;
  if (!defaultLanguageInList) {
    esyslog("tvscraper: ERROR cTVScraperSetup::cTVScraperSetup default language %i not in list", defaultLanguage);
    return;
  }
// menue for default language: just one entry, cannot be changed
  langDefault.addLanguage(defaultLanguage);
  langDefault.addLine(defaultLanguage);
  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after langDefault / langAdditional");
// additional languages
  int defaultAdditionalLanguage = 0;
  for (const auto &l: m_all_languages)
    if (l.first != defaultLanguage) {
      langAdditional.addLanguage(l.first);
      if (defaultAdditionalLanguage == 0) defaultAdditionalLanguage = l.first;
    }
// langues for channels, and lines for additional languages
  {
    cTVScraperConfigLock l;
    langChannels.addLanguage(defaultLanguage);
    int i = 0;
    for (const int &addLang: config.m_AdditionalLanguages) {
      langAdditional.addLine(addLang);
      langChannels.addLanguage(addLang);
    }
    for (; i < (int)m_all_languages.size() -1; i++) langAdditional.addLine(defaultAdditionalLanguage);
    m_NumberOfAdditionalLanguages = config.m_AdditionalLanguages.size();
  }
  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after langAdditional / langChannels");
  
  m_allChannels.clear();
  m_channelNames.clear();
  m_maxChannelNameLength = 0;
  {
#if APIVERSNUM < 20301
    for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel))
#else
    LOCK_CHANNELS_READ;
    for (const cChannel *channel = Channels->First(); channel; channel = Channels->Next(channel))
#endif
    if (!channel->GroupSep()) {
      tChannelID channelID = channel->GetChannelID();
      m_allChannels.insert({channelID, (int)m_channelNames.size()});
      m_channelNames.push_back(channel->Name() );
      m_maxChannelNameLength = std::max(m_maxChannelNameLength, (int)strlen(channel->Name() ));
      channelsScrap.push_back(config.ChannelActive(channelID)?1:0);
      m_channelsHD.push_back(config.ChannelHD(channelID)?1:0);
      int lang = config.GetLanguage_n(channelID);
      if (!langChannels.m_osdMap.isSecond(lang)) langChannels.addLanguage(lang);
      langChannels.addLine(lang);
    }
  }
  for (const auto recChannel: m_recChannels) {
    if (m_allChannels.find(recChannel.first) != m_allChannels.end() ) continue;
    if (!recChannel.second || !*recChannel.second) continue; // no channel name
    m_allChannels.insert({recChannel.first, (int)m_channelNames.size()});
    m_channelNames.push_back(recChannel.second);
    m_maxChannelNameLength = std::max(m_maxChannelNameLength, (int)strlen(recChannel.second));
    m_channelsHD.push_back(config.ChannelHD(recChannel.first)?1:0);
    int lang = config.GetLanguage_n(recChannel.first);
    if (!langChannels.m_osdMap.isSecond(lang)) langChannels.addLanguage(lang);
    langChannels.addLine(lang);
  }
  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after ChannelActive / ChannelHD");
// TV Shows
  m_allTV_Shows = getAllTV_Shows();
  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after TV shows (get from db)");
  for (const int &TV_Show: m_allTV_Shows)
    m_selectedTV_Shows.push_back(config.TV_ShowSelected(TV_Show)?1:0);
  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after TV shows (final)");

  Setup();
}

cTVScraperSetup::~cTVScraperSetup() {
}
void cTVScraperSetup::Setup(void) {
    m_NumberOfAdditionalLanguages = std::max(m_NumberOfAdditionalLanguages, 0);
    m_NumberOfAdditionalLanguages = std::min(m_NumberOfAdditionalLanguages, (int)langAdditional.m_selectedLanguage.size());
    int currentItem = Current();
    Clear();
    Add(new cOsdItem(tr("Configure channels to be scraped")));
    Add(new cMenuEditStraItem(tr("System language"), &langDefault.m_selectedLanguage[0], langDefault.m_numLang, langDefault.m_osdTexts));
    Add(new cMenuEditIntItem(tr("Number of additional languages"), &m_NumberOfAdditionalLanguages));
    for (int i = 0; i < m_NumberOfAdditionalLanguages; i++)
      Add(new cMenuEditStraItem((string(tr("Additional language")) + " " + to_string(i+1)).c_str(), &langAdditional.m_selectedLanguage[i], langAdditional.m_numLang, langAdditional.m_osdTexts));
    if (config.numAdditionalLanguages() > 0) Add(new cOsdItem(tr("Set language for each channel")));
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
    if (!hadSubMenu && Current() == 2 && (Key == kLeft || Key == kRight)) Setup();  // refresh screen if "Number of additional languages " is changed
    if (!hadSubMenu && (Key == kOk)) {
        const char* ItemText = Get(Current())->Text();
        if (strcmp(ItemText, tr("Configure channels to be scraped")) == 0)
            state = AddSubMenu(new cTVScraperChannelSetup(m_channelNames, m_maxChannelNameLength, &channelsScrap, tr("Configure channels to be scraped"), tr("don't scrap"), tr("scrap")));
        else if (strcmp(ItemText, tr("HD channels")) == 0)
            state = AddSubMenu(new cTVScraperChannelSetup(m_channelNames, m_maxChannelNameLength, &m_channelsHD, tr("HD channels"), tr("SD"), tr("HD"), tr("UHD") ));
        else if (strcmp(ItemText, tr("Set language for each channel")) == 0)
            state = AddSubMenu(new cTVScraperChannelSetup(m_channelNames, m_maxChannelNameLength, &langChannels.m_selectedLanguage, tr("Select language for channels"), langChannels.m_numLang, langChannels.m_osdTexts, langChannels.m_maxOsdTextLength));
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

std::set<tChannelID> cTVScraperSetup::GetChannelsFromSetup(const vector<int> &channels) {
  std::set<tChannelID> result;
  int size = channels.size();
  for (const auto &cha : m_allChannels) if (cha.second < size && channels[cha.second] > 0)
    result.insert(cha.first);
  return result;
}

std::map<tChannelID, int> cTVScraperSetup::GetChannelsMapFromSetup(const vector<int> &channels, const mapIntBi *langIds) {
  map<tChannelID, int> result;
  for (const auto &cha : m_allChannels) if (channels[cha.second] > 0)
    result.insert(std::make_pair(cha.first, langIds->getSecond(channels[cha.second])));
  return result;
}

std::map<tChannelID, int> cTVScraperSetup::GetChannelsMapFromSetup(const vector<int> &channels) {
  std::map<tChannelID, int> result;
  int size = channels.size();
  for (const auto &cha : m_allChannels) if (cha.second < size && channels[cha.second] > 0)
    result.insert({cha.first, channels[cha.second]});
  return result;
}

bool cTVScraperSetup::ChannelsFromSetupChanged(std::map<tChannelID, int> oldChannels, const vector<int> &channels) {
// return true in case of any change
  int size = channels.size();
  for (const auto &cha: m_allChannels) if (cha.second < size) {
    int oldV = 0;
    auto f = oldChannels.find(cha.first);
    if (f!= oldChannels.end() ) oldV = f->second;
    if (oldV != channels[cha.second]) return true;
  }
  return false;
}

std::string getStringFromMap(const map<tChannelID, int> &in, int lang) {
  std::string result;
  for (const auto &i: in) if (i.second == lang){
    result.append(objToString(i.first));
    result.append(";");
  }
  return result;
}

template<class T>
std::set<T> menuSelectionsToSet(const std::set<T> &allItems, const std::vector<int> &selectedItems, bool reverse) {
  std::set<T> result;
  int i = 0;
  for (const T &item: allItems) {
    if ( reverse && selectedItems[i] == 0) result.insert(item);
    if (!reverse && selectedItems[i] != 0) result.insert(item);
    i++;
  }
  return result;
}

void cTVScraperSetup::Store(void) {
  bool channelsHD_Change;
  {
    cTVScraperConfigLock l;
    channelsHD_Change = ChannelsFromSetupChanged(config.m_HD_Channels, m_channelsHD);
  }
  {
    set<tChannelID> channels = GetChannelsFromSetup(channelsScrap);
    map<tChannelID, int> hd_channels;
    if (channelsHD_Change) hd_channels = GetChannelsMapFromSetup(m_channelsHD);
    map<tChannelID, int> channel_language = GetChannelsMapFromSetup(langChannels.m_selectedLanguage, &langChannels.m_osdMap);
// note: GetChannelsFromSetup will call LOCK_CHANNELS_READ; -> Locking order!!! -> we lock after these calls
    cTVScraperConfigLock lw(true);
    config.enableDebug = m_enableDebug;
    config.m_enableAutoTimers = m_enableAutoTimers;
    config.m_channels = std::move(channels);
    if (channelsHD_Change) {
      config.m_HD_ChannelsModified = true;
      config.m_HD_Channels = std::move(hd_channels);
    }
    config.m_defaultLanguage = langDefault.getLanguage(0);
    config.m_AdditionalLanguages.clear();
    for (int i = 0; i < m_NumberOfAdditionalLanguages; i++)
      config.m_AdditionalLanguages.insert(langAdditional.getLanguage(i));
    config.m_channel_language = std::move(channel_language);
    config.m_excludedRecordingFolders = menuSelectionsToSet<string>(m_allRecordingFolders, m_selectedRecordingFolders, true);
    config.m_TV_Shows = menuSelectionsToSet<int>(m_allTV_Shows, m_selectedTV_Shows, false);
  }

  cTVScraperConfigLock lr;
// getStringFromSet<> sets no Locks
  SetupStore("ScrapChannels", getStringFromSet<tChannelID>(config.m_channels).c_str());
  if (channelsHD_Change) {
    SetupStore("HDChannels", getStringFromMap(config.m_HD_Channels, 1).c_str());
    SetupStore("UHDChannels", getStringFromMap(config.m_HD_Channels, 2).c_str());
  }
  SetupStore("ExcludedRecordingFolders", getStringFromSet<string>(config.m_excludedRecordingFolders, '/').c_str());
  SetupStore("TV_Shows", getStringFromSet<int>(config.m_TV_Shows).c_str());
  SetupStore("enableDebug", config.enableDebug);
  SetupStore("enableAutoTimers", config.m_enableAutoTimers);
  SetupStore("defaultLanguage", config.m_defaultLanguage);
  SetupStore("additionalLanguages", getStringFromSet<int>(config.m_AdditionalLanguages).c_str() );
  for (const int &lang: config.m_AdditionalLanguages)
    SetupStore(("additionalLanguage"s + std::to_string(lang)).c_str(), getStringFromMap(config.m_channel_language, lang).c_str() );
}

/* cTVScraperChannelSetup */
int cTVScraperChannelSetup::GetColumnWidth(int maxChannelNameLength, vector<int> *channels) {
// we combine channel number and channel name in one column, as some sknis only support 2 columns :(
// return width for channel number + 1 + width for channel name + 1
// +1: space between texts ...
  m_chanNumberWidth = 2;
  if ((int) channels->size() > 99) m_chanNumberWidth++;
  if ((int) channels->size() > 999) m_chanNumberWidth++;
  return m_chanNumberWidth + 1 + maxChannelNameLength + 1;
}

cTVScraperChannelSetup::cTVScraperChannelSetup(const vector<const char*> &channelNames, int maxChannelNameLength, vector<int> *channels, const char *headline, const char *null, const char *eins, const char *zwei): cOsdMenu(headline, GetColumnWidth(maxChannelNameLength, channels), 20) {
  int numOptions = zwei?3:2;
  m_selectOptions = new const char*[numOptions];
  m_selectOptions[0] = null;
  m_selectOptions[1] = eins;
  if (zwei) m_selectOptions[2] = zwei;
  Setup(channelNames, channels, numOptions, m_selectOptions);
}

cTVScraperChannelSetup::cTVScraperChannelSetup(const vector<const char*> &channelNames, int maxChannelNameLength, vector<int> *channels, const char *headline, int numOptions, const char**selectOptions, int maxSelectOptionsLength): cOsdMenu(headline, GetColumnWidth(maxChannelNameLength, channels), maxSelectOptionsLength) {
  Setup(channelNames, channels, numOptions, selectOptions);
}
void cTVScraperChannelSetup::Setup(const vector<const char*> &channelNames, vector<int> *channels, int numOptions, const char**selectOptions) {
  SetMenuCategory(mcSetupPlugins);
  int currentItem = Current();
  Clear();
  for (int i=0; i < (int)channels->size(); i++) {
// note: channels is limited to the channels relevant here, and can be smaller than channelNames
    stringstream name;
    name << std::setw(m_chanNumberWidth+1) << (i+1) << " " << channelNames[i];
    Add(new cMenuEditStraItem(name.str().c_str(), &channels->at(i), numOptions, selectOptions));
  }
  SetCurrent(Get(currentItem));
  Display();
}

cTVScraperChannelSetup ::~cTVScraperChannelSetup () {
  if (m_selectOptions) delete [] m_selectOptions;
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
    if (db.QueryStep(statement, "s", &name) && name && *name)
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
bool mapIntBi::insert(int first, int second) {
  int s = m_map.size();
  if (first == s) { m_map.push_back(second); return true; }
  if (first <  s && first >= 0) { m_map[first] = second; return true; }
  esyslog("tvscraper: ERROR, mapIntBi::insert, first = %i, second = %i, size = %i", first, second, s);
  return false;
}
int mapIntBi::getFirst(int second) const {
  for (int i = 0; i < (int)m_map.size(); i++) if (m_map[i] == second) return i;
  esyslog("tvscraper: ERROR, mapIntBi::getFirst, second = %i, size = %i", second, (int)m_map.size() );
  return 0;
}
int mapIntBi::getSecond(int first) const {
  int s = m_map.size();
  if (first <  s && first >= 0) return m_map[first];
  esyslog("tvscraper: ERROR, mapIntBi::getSecond, first = %i, size = %i", first, s);
  return 0;
}
bool mapIntBi::isSecond(int second) const {
  for (int i = 0; i < (int)m_map.size(); i++) if (m_map[i] == second) return true;
  return false;
}


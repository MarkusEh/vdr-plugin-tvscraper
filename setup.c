#include "setup.h"
#include <iomanip>

cSetupChannel::cSetupChannel(const cChannel *channel, unsigned channel_number_width):
  m_groupSep(channel->GroupSep())
{
  if (m_groupSep) {
    m_number = 0;
    m_language = nullptr;
    m_name = cSv(channel->Name());
  } else {
    m_channelID = channel->GetChannelID();
    m_number = channel->Number();
    m_language = config.Languages().GetLanguageFromEPG(m_channelID);
    cToSvConcat name;
    name.appendIntFillBlanks(m_number, channel_number_width);
    name << ' ' << cSv(channel->Name());
    m_name = name;
  }
  set_data_from_setup();
}
cSetupChannel::cSetupChannel(const cRecording *recording, unsigned channel_number_width):
// use only for recordings with channels which are not channels any more
  m_groupSep(false),
  m_channelID(recording->Info()->ChannelID()),
  m_number(-1),
#if VDRVERSNUM >= 20709
  // note: Currently, this information is ignored in the setup
  //       reason: what if 2 recordings have same channel but different languages?
  m_language(config.Languages().GetLanguageIso(recording->Info()->GetEvent()->Language()))
#else
  m_language(nullptr)
#endif
{
  cToSvConcat name;
  name.append(channel_number_width+1, ' ');
  name << recording->Info()->ChannelName();
  m_name = name;
  set_data_from_setup();
}
cSetupChannel::cSetupChannel(cSv group_separator):
  // add a line to the list
  m_groupSep(true),
  m_number(-1),
  m_language(nullptr)
{
  m_name = group_separator;
}
void cSetupChannel::set_data_from_setup() {
  m_setup_hd = config.ChannelHD(m_channelID);
  m_setup_scrape = config.ChannelActive(m_channelID)?1:0;
  m_setup_lang = config.Languages().GetLanguageFromSetup_n(m_channelID);
}
cSetupChannels::cSetupChannels():
  m_texts_from_epg(config.Languages().Max_id() + 1),
  m_language_select_options(m_texts_from_epg.size(), nullptr) {
// create m_texts_from_epg: test from EPG ... for each language
  for (const cLanguage &l: config.Languages().m_languages) {
    if (m_texts_from_epg.size() < (size_t)l.Id() ) {
      esyslog3("m_texts_from_epg too small, m_texts_from_epg.size() = ", m_texts_from_epg.size(), " l.Id() = ", l.Id());
    } else {
      cToSvConcat text(tr("from EPG"), " [", l.Name(), ']');
      m_texts_from_epg[l.Id()] = text;
    }
  }
  m_select_options_hd[0] = tr("SD");
  m_select_options_hd[1] = tr("HD");
  m_select_options_hd[2] = tr("UHD");

  m_select_options_scrape[0] = tr("don't scrap");
  m_select_options_scrape[1] = tr("scrap");
// create m_setup_channels: List of all channels, including channels only used in recordings
  {
    LOCK_CHANNELS_READ;
    m_channel_number_width = stringhelpers_internal::numChars(Channels->MaxNumber());
    m_max_channel_name_length = Channels->MaxChannelNameLength();
    for (const cChannel *channel = Channels->First(); channel; channel = Channels->Next(channel)) {
      if (channel->GroupSep())
        m_setup_channels.emplace_back(channel, m_channel_number_width);
      else if (!(channel->GetChannelID() == tChannelID())) {
        m_setup_channels.emplace_back(channel, m_channel_number_width);
        m_sorted_channels.insert(channel->GetChannelID());
      }
    }
  }
#if VDRVERSNUM >= 20709
/*
  {
    isyslog2("Set language of recordings");
    bool updated = false;
    LOCK_RECORDINGS_WRITE;
    for (cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec))
      if (rec->Info() && rec->Info()->ChannelName() && !(rec->Info()->ChannelID() == tChannelID()) &&
          !*rec->Info()->GetEvent()->Language() && is_in_list(rec->Info()->ChannelID())) {
// this recording has no language, but a channel which is still in use
//      isyslog2("Set language of recording \"", rec->Name());
        for (const cSetupChannel &setupChannel: m_setup_channels)
          if (setupChannel.m_channelID == rec->Info()->ChannelID()) {
            // todo: also compare channel->name with rec->Info()->ChannelName()
            if (!setupChannel.m_language) {
              isyslog2("No language for channel ", setupChannel.m_name);
            } else {
              isyslog2("Set language of recording \"", rec->Name(), " channel ", setupChannel.m_name, " to ", setupChannel.m_language->Iso());
              cEvent *event = const_cast<cEvent *>(rec->Info()->GetEvent());
              event->SetLanguage(setupChannel.m_language->Iso() );
              rec->WriteInfo();
              updated = true;
            }
            break;
        }
      }
    if (updated) Recordings->TouchUpdate();
  }
*/
#endif
  m_setup_channels.emplace_back(cSv(tr("Channels used only in recordings")));
  {
    LOCK_RECORDINGS_READ;
    for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec))
      if (rec->Info() && rec->Info()->ChannelName() && !(rec->Info()->ChannelID() == tChannelID()) ) {
        if (!is_in_list(rec->Info()->ChannelID()) ) {
          m_setup_channels.emplace_back(rec, m_channel_number_width);
          m_sorted_channels.insert(rec->Info()->ChannelID());
          m_max_channel_name_length = std::max(m_max_channel_name_length, (unsigned)strlen(rec->Info()->ChannelName() ));
        }
      }
  }
// now the list m_setup_channels is complete
// create m_setup_languages: list of all languages (as saved in setup) used in any of m_setup_channels
  for (const cSetupChannel &setupChannel: m_setup_channels)
    if (!setupChannel.m_groupSep) m_setup_languages.insert(setupChannel.m_setup_lang);
}
bool cSetupChannels::is_in_list(tChannelID channelID) const {
  return m_sorted_channels.find(channelID) != m_sorted_channels.end();
}
cOsdMenu *cSetupChannels::getSetupMenuLanguage(const char *Title, int num_additional_languages, langOsd &additional_languages) {
  m_SetupMenu_displayed_language = true;
  // list of languages, together with number used in OSD for this language
  clear_language_select_options();
  m_map_lang_osd.clear();
  m_map_lang_osd.push_back(0); // auto language
  for (int i = 0; i < num_additional_languages; ++i)
    m_map_lang_osd.push_back(additional_languages.getLanguage(i));
  for (int setup_lang: m_setup_languages)
    if (setup_lang != -1) m_map_lang_osd.push_back_if_new(setup_lang);
  m_map_lang_osd.push_back_if_new(config.Languages().GetDefaultLanguage()->Id());
  int numOptions = m_map_lang_osd.size();


  m_language_select_options[0] = new const char*[numOptions];
  m_language_select_options[0][0] = tr("default");
  for (int i = 1; i < numOptions; ++i)
    m_language_select_options[0][i] = config.Languages().GetLanguage(m_map_lang_osd.getSecond(i))->Name();
  for (const cLanguage &l: config.Languages().m_languages) {
    if (m_language_select_options.size() < (size_t)l.Id() ) {
      esyslog3("m_language_select_options too small, m_language_select_options.size() = ", m_language_select_options.size(), " l.Id() = ", l.Id());
    } else if (m_language_select_options[l.Id()] != nullptr) {
      esyslog3("m_language_select_options[", l.Id(), "] != nullptr");
    } else {
      m_language_select_options[l.Id()] = new const char*[numOptions];
      m_language_select_options[l.Id()][0] = m_texts_from_epg[l.Id()].c_str();
      for (int i = 1; i < numOptions; ++i)
        m_language_select_options[l.Id()][i] = config.Languages().GetLanguage(m_map_lang_osd.getSecond(i))->Name();
    }
  }
  size_t sec_col_size = std::max(strlen(tr("from EPG")), strlen(tr("default"))) + config.Languages().Max_name_length() + 3;

  cOsdMenu *result = new cTVScraperOsdMenu(Title, firstColumnWidth(), sec_col_size);
  result->SetMenuCategory(mcSetupPlugins);
  for (cSetupChannel &setupChannel: m_setup_channels) {
    if (setupChannel.m_groupSep) {
      result->Add(new cOsdItem(setupChannel.m_name.c_str()));
    } else {
      if (setupChannel.m_setup_lang == -1)
        setupChannel.m_osd_lang = 0;
      else
        setupChannel.m_osd_lang = m_map_lang_osd.getFirst(setupChannel.m_setup_lang);
      const char **select_options;
      if (setupChannel.m_language && setupChannel.m_number > 0)
        select_options = m_language_select_options[setupChannel.m_language->Id()];
      else
        select_options = m_language_select_options[0];
      result->Add(new cMenuEditStraItem(setupChannel.m_name.c_str(), &setupChannel.m_osd_lang, numOptions, select_options));
    }
  }
  result->Display();
  return result;
}
cOsdMenu *cSetupChannels::getSetupMenuHD(const char *Title) {
  m_SetupMenu_displayed_hd = true;

  cOsdMenu *result = new cTVScraperOsdMenu(Title, firstColumnWidth(), 15);
  result->SetMenuCategory(mcSetupPlugins);
  for (cSetupChannel &setupChannel: m_setup_channels) {
    if (setupChannel.m_groupSep) {
      result->Add(new cOsdItem(setupChannel.m_name.c_str()));
    } else {
      setupChannel.m_osd_hd = setupChannel.m_setup_hd;
      result->Add(new cMenuEditStraItem(setupChannel.m_name.c_str(), &setupChannel.m_osd_hd, m_numOptions_hd, m_select_options_hd));
    }
  }
  result->Display();
  return result;
}
cOsdMenu *cSetupChannels::getSetupMenuScrape(const char *Title) {
  m_SetupMenu_displayed_scrape = true;

  cOsdMenu *result = new cTVScraperOsdMenu(Title, firstColumnWidth(), 15);
  result->SetMenuCategory(mcSetupPlugins);
  for (cSetupChannel &setupChannel: m_setup_channels) {
    if (setupChannel.m_groupSep) {
      if (setupChannel.m_number == -1) break;
      result->Add(new cOsdItem(setupChannel.m_name.c_str()));
    } else {
      setupChannel.m_osd_scrape = setupChannel.m_setup_scrape;
      result->Add(new cMenuEditStraItem(setupChannel.m_name.c_str(), &setupChannel.m_osd_scrape, 2, m_select_options_scrape));
    }
  }
  result->Display();
  return result;
}
void cSetupChannels::clear_language_select_options() {
  for (size_t i = 0; i < m_language_select_options.size(); ++i) {
    delete[] m_language_select_options[i];
    m_language_select_options[i] = nullptr;
  }
}
std::string getStringFromMap(const map<tChannelID, int> &in, int lang) {
  cToSvConcat result;
  for (const auto &i: in) if (i.second == lang) {
    result << i.first << ';';
  }
  return std::string(result);
}
void cSetupChannels::Store(cTVScraperSetup *TVScraperSetup) {
  if (m_SetupMenu_displayed_language) {
    m_SetupMenu_displayed_language = false;
    map<tChannelID, int> channel_language;
    for (cSetupChannel &setupChannel: m_setup_channels) if (!setupChannel.m_groupSep) {
      if (setupChannel.m_osd_lang == 0)
        setupChannel.m_setup_lang = -1;
      else {
        setupChannel.m_setup_lang = m_map_lang_osd.getSecond(setupChannel.m_osd_lang);
        channel_language.insert({setupChannel.m_channelID, setupChannel.m_setup_lang});
      }
    }

    for (int i = 1; i < (int)m_map_lang_osd.size(); ++i)
      TVScraperSetup->SetupStore((cToSvConcat("additionalLanguage", m_map_lang_osd.getSecond(i))).c_str(), getStringFromMap(channel_language, m_map_lang_osd.getSecond(i)).c_str() );
    config.Languages().SwapLanguagesFromSetup(channel_language);
  }
// SD / HD / UHD
  if (m_SetupMenu_displayed_hd) {
    m_SetupMenu_displayed_hd = false;
    bool channelsHD_Change = false;
    for (cSetupChannel &setupChannel: m_setup_channels) if (!setupChannel.m_groupSep) {
      if (setupChannel.m_osd_hd != setupChannel.m_setup_hd) {
        channelsHD_Change = true;
        break;
      }
    }
    if (channelsHD_Change) {
      map<tChannelID, int> hd_channels;
      for (cSetupChannel &setupChannel: m_setup_channels) if (!setupChannel.m_groupSep) {
        setupChannel.m_setup_hd = setupChannel.m_osd_hd;
        if (setupChannel.m_osd_hd > 0)
          hd_channels.insert({setupChannel.m_channelID, setupChannel.m_osd_hd});
      }
      TVScraperSetup->SetupStore("HDChannels", getStringFromMap(hd_channels, 1).c_str());
      TVScraperSetup->SetupStore("UHDChannels", getStringFromMap(hd_channels, 2).c_str());
      cTVScraperConfigLock lw(true);
      config.m_HD_ChannelsModified = true;
      config.m_HD_Channels = std::move(hd_channels);
    }
  }
// scrape / don't scrape
  if (m_SetupMenu_displayed_scrape) {
    m_SetupMenu_displayed_scrape = false;
    cSortedVector<tChannelID> channels;
    for (cSetupChannel &setupChannel: m_setup_channels) if (!setupChannel.m_groupSep) {
      setupChannel.m_setup_scrape = setupChannel.m_osd_scrape;
      if (setupChannel.m_osd_scrape > 0) channels.insert(setupChannel.m_channelID);
    }
    TVScraperSetup->SetupStore("ScrapChannels", getStringFromSet(channels).c_str());
    cTVScraperConfigLock lw(true);
    config.m_channels = std::move(channels);
  }
}

cSortedVector<std::string> cTVScraperSetup::getAllRecordingFolders(int &max_width) {
  cSortedVector<std::string> result;
  max_width = 0;
  LOCK_RECORDINGS_READ;
  for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec))
  {
    const char *pos_delim = strrchr(rec->Name(), '~');
    if (pos_delim != 0) {
      result.insert(std::string(rec->Name(), pos_delim - rec->Name() ));
      max_width = std::max(max_width, (int)(pos_delim - rec->Name()) );
    }
  }
  return result;
}
cSortedVector<int> cTVScraperSetup::getAllTV_Shows() {
  cSortedVector<int> result;
// add all TV Shows from recordings & from config, where we create recordings for missing episodes.
// don't add TV Shows from tv2 / EPG events, as these are too many
// add all TV Shows from recordings
  cSqlValue<int> sql(&m_db, "SELECT DISTINCT movie_tv_id FROM recordings2 WHERE season_number != -100 AND season_number != -101 AND season_number != -102");
  for (int movie_tv_id: sql) result.insert(movie_tv_id);
// add all TV Shows from config, where we create recordings for missing episodes
  cTVScraperConfigLock l;
  for (const int &id: config.m_TV_Shows ) result.insert(id);
  return result;
}

std::vector<std::pair<int, std::string>> getLanguageTexts() {
  std::vector<std::pair<int, std::string>> result;
  for (const cLanguage &lang: config.Languages().m_languages)
    if (lang.Prio() >= 0) result.push_back({lang.Id(), lang.getNames()});
  return result;
}
/* cTVScraperSetup */
cTVScraperSetup::cTVScraperSetup(cTVScraperWorker *workerThread, const cTVScraperDB &db):
  worker(workerThread),
  m_db(db),
  m_all_languages(getLanguageTexts()),
  langDefault(&m_all_languages),
  langAdditional(&m_all_languages)
  {
  if (config.enableDebug) esyslog("tvscraper: Start of cTVScraperSetup::cTVScraperSetup");
  m_Set_language_for_each_channel = tr("Set language for each channel");
  m_Configure_channels_to_be_scraped = tr("Configure channels to be scraped");
  m_HD_channels = tr("HD channels");
  m_Recording_folders_to_improve = tr("Recording folders to improve");
  m_TV_shows_to_record = tr("TV shows to record");
  m_Trigger_scraping_Video_Directory = tr("Trigger scraping Video Directory");
  m_Trigger_EPG_scraping = tr("Trigger EPG scraping");
// copy data from config to local data, which will be directly changed in OSD
  m_enableDebug = config.enableDebug;
  m_writeEpisodeToEpg = config.m_writeEpisodeToEpg;
  m_enableAutoTimers = config.m_enableAutoTimers;
// Recording folders
  m_allRecordingFolders = getAllRecordingFolders(m_recordings_width);
  for (const std::string &recordingFolder: m_allRecordingFolders)
    m_selectedRecordingFolders.push_back(config.recordingFolderSelected(recordingFolder)?1:0);
//  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after selectedRecordingFolders");
// languages  =============================================
  if (m_all_languages.size() == 0) {
    esyslog("tvscraper: ERROR cTVScraperSetup::cTVScraperSetup m_all_languages.size() == 0");
    return;
  }
// default language
  int defaultLanguage = config.Languages().GetDefaultLanguage()->Id();
  bool defaultLanguageInList = false;
  for (const auto &l: m_all_languages)
    if (l.first == defaultLanguage) defaultLanguageInList = true;
  if (!defaultLanguageInList) {
    esyslog("tvscraper: ERROR cTVScraperSetup::cTVScraperSetup default language %i not in list", defaultLanguage);
    return;
  }
// menu for default language: just one entry, cannot be changed
  langDefault.addLanguage(defaultLanguage);
  langDefault.addLine(defaultLanguage);
//  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after langDefault / langAdditional");
// additional languages
  int defaultAdditionalLanguage = 0;
  for (const auto &l: m_all_languages)
    if (l.first != defaultLanguage) {
      langAdditional.addLanguage(l.first);
      if (defaultAdditionalLanguage == 0) defaultAdditionalLanguage = l.first;
    }
// lines for additional languages
  auto additional_languages = config.Languages().GetAdditionalLanguages();
  for (const int &addLang: additional_languages) {
    langAdditional.addLine(addLang);
  }
  int i = 0;
  for (; i < (int)m_all_languages.size() -1; ++i) langAdditional.addLine(defaultAdditionalLanguage);
  m_NumberOfAdditionalLanguages = additional_languages.size();

// TV Shows
  m_allTV_Shows = getAllTV_Shows();
//  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after TV shows (get from db)");
  for (const int &TV_Show: m_allTV_Shows)
    m_selectedTV_Shows.push_back(config.TV_ShowSelected(TV_Show)?1:0);
//  if (config.enableDebug) esyslog("tvscraper: cTVScraperSetup::cTVScraperSetup after TV shows (final)");

  Setup();
}

void cTVScraperSetup::Setup(void) {
  m_NumberOfAdditionalLanguages = std::max(m_NumberOfAdditionalLanguages, 0);
  m_NumberOfAdditionalLanguages = std::min(m_NumberOfAdditionalLanguages, (int)langAdditional.m_selectedLanguage.size());
  int currentItem = Current();
  Clear();
  Add(new cOsdItem(m_Configure_channels_to_be_scraped));
  Add(new cOsdItem(cToSvConcat(tr("System language"), ":\t", langDefault.m_osdTexts[langDefault.m_selectedLanguage[0]]).c_str(), osUnknown, false));
  Add(new cMenuEditIntItem(tr("Number of additional languages"), &m_NumberOfAdditionalLanguages));
  for (int i = 0; i < m_NumberOfAdditionalLanguages; i++)
    Add(new cMenuEditStraItem(cToSvConcat(tr("Additional language"), " ", i+1).c_str(), &langAdditional.m_selectedLanguage[i], langAdditional.m_numLang, langAdditional.m_osdTexts));
  if (config.Languages().numAdditionalLanguages() > 0) Add(new cOsdItem(m_Set_language_for_each_channel));
  Add(new cOsdItem(m_HD_channels));
  Add(new cMenuEditBoolItem(tr("Create timers to improve and complement recordings"), &m_enableAutoTimers));
  if (m_enableAutoTimers) {
    Add(new cOsdItem(m_Recording_folders_to_improve));
    Add(new cOsdItem(m_TV_shows_to_record));
  }
  Add(new cOsdItem(m_Trigger_scraping_Video_Directory));
  Add(new cOsdItem(m_Trigger_EPG_scraping));
  Add(new cMenuEditBoolItem(tr("Write episode to EPG"), &m_writeEpisodeToEpg));
  Add(new cMenuEditBoolItem(tr("Enable Debug Logging"), &m_enableDebug));

  SetCurrent(Get(currentItem));
  Display();
}

eOSState cTVScraperSetup::ProcessKey(eKeys Key) {
  bool hadSubMenu = HasSubMenu();
  if (hadSubMenu && Key == kOk) Store();
  eOSState state = cMenuSetupPage::ProcessKey(Key);
  if (!hadSubMenu && Current() == 2 && (Key == kLeft || Key == kRight)) Setup();  // refresh screen if "Number of additional languages " is changed
  if (!hadSubMenu && (Key == kOk)) {
    const char* ItemText = Get(Current())->Text();
    if (strcmp(ItemText, m_Configure_channels_to_be_scraped) == 0)
      state = AddSubMenu(SetupChannels()->getSetupMenuScrape(m_Configure_channels_to_be_scraped));
    else if (strcmp(ItemText, m_HD_channels) == 0)
      state = AddSubMenu(SetupChannels()->getSetupMenuHD(m_HD_channels));
    else if (strcmp(ItemText, m_Set_language_for_each_channel) == 0)
      state = AddSubMenu(SetupChannels()->getSetupMenuLanguage(m_Set_language_for_each_channel, m_NumberOfAdditionalLanguages, langAdditional));
    else if (strcmp(ItemText, m_Recording_folders_to_improve) == 0)
      state = AddSubMenu(new cTVScraperListSetup(m_selectedRecordingFolders, m_allRecordingFolders, m_Recording_folders_to_improve, m_recordings_width));
    else if (strcmp(ItemText, m_TV_shows_to_record) == 0)
      state = AddSubMenu(new cTVScraperTV_ShowsSetup(m_selectedTV_Shows, m_allTV_Shows, m_db));
    else if (strcmp(ItemText, m_Trigger_scraping_Video_Directory) == 0) {
      Skins.Message(mtInfo, "Scraping Video Directory started");
      worker->InitVideoDirScan();
      state = osContinue;
    } else if (strcmp(ItemText, m_Trigger_EPG_scraping) == 0) {
      Skins.Message(mtInfo, "EPG Scraping started");
      worker->InitManualScan();
      state = osContinue;
    }
  }
  return state;
}

template<class T>
cSortedVector<T> menuSelectionsToSet(const cSortedVector<T> &allItems, const std::vector<int> &selectedItems, bool reverse) {
  cSortedVector<T> result;
  int i = 0;
  for (const T &item: allItems) {
    if ( reverse && selectedItems[i] == 0) result.insert(item);
    if (!reverse && selectedItems[i] != 0) result.insert(item);
    i++;
  }
  return result;
}

void cTVScraperSetup::Store(void) {
  {
    cTVScraperConfigLock lw(true);
    config.enableDebug = m_enableDebug;
    config.m_writeEpisodeToEpg = m_writeEpisodeToEpg;
    config.m_enableAutoTimers = m_enableAutoTimers;

    config.m_excludedRecordingFolders = menuSelectionsToSet<string>(m_allRecordingFolders, m_selectedRecordingFolders, true);
    config.m_TV_Shows = menuSelectionsToSet<int>(m_allTV_Shows, m_selectedTV_Shows, false);
//  for (int tv_show: config.m_TV_Shows) dsyslog("tvscraper, cTVScraperSetup::Store, add tv show %d", tv_show);
  }
  {
    cTVScraperConfigLock lr;
// getStringFromSet<> sets no Locks
    SetupStore("ExcludedRecordingFolders", getStringFromSet(config.m_excludedRecordingFolders, '/').c_str());
    SetupStore("TV_Shows", getStringFromSet(config.m_TV_Shows).c_str());
    SetupStore("enableDebug", config.enableDebug);
    SetupStore("writeEpisodeToEpg", config.m_writeEpisodeToEpg);
    SetupStore("enableAutoTimers", config.m_enableAutoTimers);
  }
//  Languages() has it's own lock -> outside the cTVScraperConfigLock lw(true);
  std::vector<int> additionalLanguages;
  for (int i = 0; i < m_NumberOfAdditionalLanguages; i++)
    additionalLanguages.push_back(langAdditional.getLanguage(i));
  SetupStore("additionalLanguages", getStringFromSet(additionalLanguages).c_str() );
  config.Languages().SwapAdditionalLanguages(additionalLanguages);
  if (m_setupChannels) m_setupChannels->Store(this);
}

eOSState cTVScraperOsdMenu::ProcessKey(eKeys Key) {
    eOSState state = cOsdMenu::ProcessKey(Key);
    if (Key == kOk) return osBack;
    return state;
}

/* cTVScraperListSetup */
cTVScraperListSetup::cTVScraperListSetup(vector<int> &listSelections, const cSortedVector<string> &listEntries, const char *headline, int width): cOsdMenu(headline, width + 2, 5) {
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
cTVScraperTV_ShowsSetup::cTVScraperTV_ShowsSetup(vector<int> &listSelections, const cSortedVector<int> &TV_Shows, const cTVScraperDB &db): cOsdMenu(tr("Create timers for missing episodes of this TV show?"), 40, 5) {
  SetMenuCategory(mcSetupPlugins);
  int currentItem = Current();
  Clear();
  int i = 0;
  cSql statement(&db, "SELECT tv_name FROM tv2 WHERE tv_id = ?");
  for (const int &TV_show: TV_Shows) {
    statement.resetBindStep(TV_show);
    const char *name = NULL;
    if (statement.readRow(name) && name && *name)
      Add(new cMenuEditBoolItem(name, &(listSelections[i]) ));
    else
      Add(new cMenuEditBoolItem(to_string(TV_show).c_str(), &(listSelections[i]) ));
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

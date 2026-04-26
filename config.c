#include <filesystem>
#include <fstream>
#include <dlfcn.h>
#include "config.h"

const cLanguage *GetLanguageFromEPG(const tChannelID &channelID) {
// WARNING: VDR object SCHEDULES will be locked!

// check the EPG of this channel, nad the language of the EPG entries
// return language of these
// if unknown, return nullptr. Reasons for nullptr:
//   - vdr version too old
//   - no schedule found for this channel (e.g. old channel used in recordings)
//   - no EPG in this schedule
//   - missing or non-standard language information in EPG
#if VDRVERSNUM >= 20709
  LOCK_SCHEDULES_READ;
  const cSchedule *schedule = Schedules->GetSchedule(channelID);
  if (!schedule) return nullptr;
  for (const cEvent *e = schedule->Events()->First(); e; e = schedule->Events()->Next(e))
    if (strlen(e->Language()) == 3) {
      return config.Languages().GetLanguageIso(e->Language() );
  }
#endif
  return nullptr;
}
using namespace std;
// getDefaultHD_Channels  ********************************
std::map<tChannelID, int> getDefaultHD_Channels() {
  std::map<tChannelID, int> channelsHD;
  LOCK_CHANNELS_READ;
  for (const cChannel *listChannel = Channels->First(); listChannel; listChannel = Channels->Next(listChannel) )
  {
    if (listChannel->GroupSep() || listChannel->Name() == NULL ) continue;
    int len = strlen(listChannel->Name());
    if (len < 2) continue;
    if (strcmp (listChannel->Name() + (len - 2), "HD") != 0) continue;
    channelsHD.insert({listChannel->GetChannelID(),1});
  }
  return channelsHD;
}

// getDefaultChannels  ********************************
cSortedVector<tChannelID> getDefaultChannels() {
  cSortedVector<tChannelID> channels;
  int i = 0;
  LOCK_CHANNELS_READ;
  for (const cChannel *listChannel = Channels->First(); listChannel; listChannel = Channels->Next(listChannel) )
  {
    if (listChannel->GroupSep() || listChannel->Name() == NULL ) continue;
    if (strlen(listChannel->Name())  < 2) continue;
    channels.insert(listChannel->GetChannelID());
    if (i++ > 30) break;
  }
  return channels;
}

cTVScraperConfig::cTVScraperConfig():
  m_description_delimiter(tr("Name in external database:"))
{
    enableDebug = 0;
    m_writeEpisodeToEpg = 1;
    m_enableAutoTimers = 0;
}

cTVScraperConfig::~cTVScraperConfig() {
}

void cTVScraperConfig::SetBaseDir(const string &dir) {
  if (dir.empty() || dir[dir.length()-1] == '/') baseDir = dir;
  else baseDir = dir + "/";
  baseDirLen = baseDir.length();
  if (baseDirLen) {
    baseDirEpg = baseDir + "epg/";
    baseDirRecordings = baseDir + "recordings/";
    baseDirSeries = baseDir + "series/";
    baseDirMovies = baseDir + "movies/";
    baseDirMovieActors = baseDirMovies + "actors/";
    baseDirMovieCollections = baseDirMovies + "collections/";
    baseDirMovieTv = baseDirMovies + "tv/";
    EPG_UpdateFileName = baseDir + ".EPG_Update";
    recordingsUpdateFileName = baseDir + ".recordingsUpdate";
    if (!std::filesystem::exists(EPG_UpdateFileName) ) std::ofstream output(EPG_UpdateFileName);
    if (!std::filesystem::exists(recordingsUpdateFileName) ) std::ofstream output(recordingsUpdateFileName);
  }
}

bool cTVScraperConfig::loadPlugins()
{
   DIR* dir;
   dirent* dp;

   if (!(dir = opendir(PLGDIR)))
   {
      esyslog("tvscraper: cTVScraperConfig::loadPlugins cannot open plugin dir \"%s\"", PLGDIR);
      return false;
   }
   esyslog("tvscraper: loading plugins, plugin dir: \"%s\"", PLGDIR);

   while ((dp = readdir(dir)))
   {
      if (strncmp(dp->d_name, "libtvscraper-", 13) == 0 && strstr(dp->d_name, ".so"))
      {
         cToSvConcat path(PLGDIR, "/", dp->d_name);
         void *handle = dlopen(path.c_str(), RTLD_NOW || RTLD_GLOBAL);
         const char* error = dlerror();

         if (!error)
         {
            iExtEpg *(*creator)(bool debug);
            *(void**)(&creator) = dlsym(handle, "extEpgPluginCreator");
            error = dlerror();
            if (!error) {
              iExtEpg* plugin = creator(enableDebug);
              std::shared_ptr<iExtEpg> sp_plugin(plugin);
              m_extEpgs.push_back(sp_plugin);
            }
         }
         if (error)
            esyslog("tvscraper: ERROR cTVScraperConfig::loadPlugins file %s, error %s", path.c_str(), error);
      }
   }
   closedir(dir);
   return true;
}
void cTVScraperConfig::readNetworks() {
  cToSvConcat fname_networks(cPlugin::ResourceDirectory(PLUGIN_NAME_I18N), "/networks.json");
  struct stat buffer;
  if (stat (fname_networks.c_str(), &buffer) != 0) {
    esyslog("tvscraper: ERROR file %s not found (continue without networks)", fname_networks.c_str());
    return;
  }
  cJsonDocumentFromFile j_networks(fname_networks);
  if (!j_networks.IsArray() ) {
    esyslog("tvscraper: ERROR file %s is not a json array (continue without networks)", fname_networks.c_str());
    return;
  }

  for (auto &j_network: j_networks.GetArray()) {
    int TheTVDB_company_ID = getValueInt(j_network, "TheTVDB company ID");
    const char *TheTVDB_company_name = getValueCharS(j_network, "TheTVDB company name");
    if (!TheTVDB_company_ID || !TheTVDB_company_name || !*TheTVDB_company_name) {
      esyslog("tvscraper: ERROR in networks.json, TheTVDB company ID %d and/or TheTVDB company name %s missing", TheTVDB_company_ID, TheTVDB_company_name?TheTVDB_company_name:"missing");
      continue;
    }
    m_TheTVDB_company_name_networkID.insert({std::string(cToSvToLower(TheTVDB_company_name)), TheTVDB_company_ID});
    for (auto &j_channelName: cJsonArrayIterator(j_network, "TV channel names")) {
      m_channelName_networkID.insert({std::string(cToSvToLower(j_channelName.GetString())), TheTVDB_company_ID});
    }
  }
}
int cTVScraperConfig::Get_TheTVDB_company_ID_from_TheTVDB_company_name(cSv TheTVDB_company_name) {
  if (TheTVDB_company_name.empty() ) return 0;
  auto f = m_TheTVDB_company_name_networkID.find(std::string_view(cToSvToLower(TheTVDB_company_name)));
  if (f == m_TheTVDB_company_name_networkID.end()) return 0;
  return f->second;
}

int cTVScraperConfig::Get_TheTVDB_company_ID_from_channel_name(cSv channel_name) {
  channel_name = removeSuffix(channel_name, "+1");
  channel_name = removeSuffix(channel_name, " HD");
  channel_name = removeSuffix(channel_name, " UHD");
  if (channel_name.empty() ) return 0;
  cToSvConcat cn_lc;
  auto f_HD = channel_name.find(" HD ");
  if (f_HD == std::string_view::npos) {
    auto f_UHD = channel_name.find(" UHD ");
    if (f_UHD == std::string_view::npos) cn_lc.appendToLower(channel_name);
    else cn_lc.appendToLower(channel_name.substr(0, f_UHD)).appendToLower(channel_name.substr(f_UHD+4));
  } else cn_lc.appendToLower(channel_name.substr(0, f_HD)).appendToLower(channel_name.substr(f_HD+3));
  auto f = m_channelName_networkID.find(std::string_view(cn_lc));
  if (f == m_channelName_networkID.end()) return 0;
  return f->second;
}

void cTVScraperConfig::Initialize() {
// we don't lock here. This is called during plugin initialize, and there are no other threads at this point in time

// these values are in setup.conf. Set here defaults, if values in setup.conf are missing.
  if (m_HD_Channels.empty() ) {m_HD_Channels = getDefaultHD_Channels(); m_HD_ChannelsModified = false; }
  if (   m_channels.empty() )     m_channels = getDefaultChannels();
  m_languages.Initialize();
// load plugins from ext. epgs
  loadPlugins();
//  m_extEpgs.push_back(std::make_shared<cExtEpgTvsp>());
// read from /var/lib/vdr/plugins/tvscraper/channelmap.conf
  loadChannelmap(m_channelMap, m_extEpgs);
  readNetworks();
}
void cTVScraperConfig::SetAutoTimersPath(const string &option) {
  m_autoTimersPathSet = true;
// make sure that m_autoTimersPath is empty or ends with ~
  m_autoTimersPath = option;
  if (option.empty() ) return;
  if (*option.rbegin() == '~') return;
  m_autoTimersPath.append(1, '~');
}

bool cTVScraperConfig::SetupParse(const char *Name, const char *Value) {
    cTVScraperConfigLock lw(true);
    if (strcmp(Name, "ScrapChannels") == 0) {
        m_channels = getSetFromString<tChannelID>(Value);
        return true;
    } else if (strcmp(Name, "HDChannels") == 0) {
        config.m_HD_ChannelsModified = true;
        for (cSv c: cSplit(Value, ';', eSplitDelimBeginEnd::optional)) m_HD_Channels.insert({lexical_cast<tChannelID>(c), 1});
        return true;
    } else if (strcmp(Name, "UHDChannels") == 0) {
        config.m_HD_ChannelsModified = true;
        for (cSv c: cSplit(Value, ';', eSplitDelimBeginEnd::optional)) m_HD_Channels.insert({lexical_cast<tChannelID>(c), 2});
        return true;
    } else if (strcmp(Name, "ExcludedRecordingFolders") == 0) {
        m_excludedRecordingFolders = getSetFromString<std::string>(Value, '/');
        return true;
    } else if (strcmp(Name, "TV_Shows") == 0) {
        m_TV_Shows = getSetFromString<int>(Value);
        for (int tv_show: m_TV_Shows) dsyslog("tvscraper, SetupParse, add tv show %d", tv_show);
        return true;
    } else if (strcmp(Name, "enableDebug") == 0) {
        enableDebug = atoi(Value);
        return true;
    } else if (strcmp(Name, "writeEpisodeToEpg") == 0) {
        m_writeEpisodeToEpg = atoi(Value);
        return true;
    } else if (strcmp(Name, "enableAutoTimers") == 0) {
        m_enableAutoTimers = atoi(Value);
        return true;
    } else if (strcmp(Name, "defaultLanguage") == 0) {
        Languages().SetDefaultLanguage(Languages().GetLanguage(atoi(Value)));
        return true;
    } else if (strcmp(Name, "additionalLanguages") == 0) {
        std::vector<int> additionalLanguages = getSetFromString<int, std::vector<int> >(Value);
        Languages().SwapAdditionalLanguages(additionalLanguages);
        return true;
    } else if (strncmp(Name, "additionalLanguage", 18) == 0) {
        int num_lang = atoi(Name + 18);
        if (num_lang <= 0) return false;
        std::map<tChannelID, int> channel_language;
        for (cSv c: cSplit(Value, ';', eSplitDelimBeginEnd::optional)) channel_language.insert({lexical_cast<tChannelID>(c), num_lang});
        Languages().SwapChannelLanguage(channel_language);
        return true;
    }
    return false;
}

std::shared_ptr<iExtEpgForChannel> cTVScraperConfig::GetExtEpgIf(const tChannelID &channelID) const {
  vector<sChannelMapEpg>::const_iterator channelMapEpg_it = std::lower_bound(m_channelMap.begin(), m_channelMap.end(), channelID,
     [](const sChannelMapEpg &cm, const tChannelID &c)
            {
                return cm.channelID < c;
            });
  std::shared_ptr<iExtEpgForChannel> extEpg;
  if (channelMapEpg_it == m_channelMap.end() ) return extEpg;
  if (!(channelMapEpg_it->channelID == channelID)) return extEpg;
  if (channelMapEpg_it->extEpg)
    extEpg = channelMapEpg_it->extEpg->getHandlerForChannel(channelMapEpg_it->extid);
  return extEpg;
}

cSortedVector<tChannelID> cTVScraperConfig::GetScrapeAndEpgChannels() const {
  cSortedVector<tChannelID> result;
  {
    cTVScraperConfigLock l;
    result = m_channels;
  }
  for (const sChannelMapEpg &channelEpg: m_channelMap) result.insert(channelEpg.channelID);
  return result;
}


// implement class cTVScraperConfigLock *************************
cTVScraperConfigLock::cTVScraperConfigLock(bool Write) {
  config.stateLock.Lock(m_stateKey, Write);
}
cTVScraperConfigLock::~cTVScraperConfigLock() {
  m_stateKey.Remove();
}

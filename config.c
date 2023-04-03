#include <filesystem>
#include <fstream>
#include <dlfcn.h>
#include "config.h"

using namespace std;
// getDefaultHD_Channels  ********************************
std::map<tChannelID, int> getDefaultHD_Channels() {
  std::map<tChannelID, int> channelsHD;
#if VDRVERSNUM >= 20301
  LOCK_CHANNELS_READ;
  for ( cChannel *listChannel = (cChannel *)Channels->First(); listChannel; listChannel = (cChannel *)Channels->Next( listChannel ) )
#else
  ReadLock channelsLock( Channels );
  if ( !channelsLock ) return channelsHD;
  for ( cChannel *listChannel = Channels.First(); listChannel; listChannel = Channels.Next( listChannel ) )
#endif
  {
    if ( listChannel->GroupSep() || listChannel->Name() == NULL ) continue;
    int len = strlen(listChannel->Name());
    if (len < 2) continue;
    if (strcmp (listChannel->Name() + (len - 2), "HD") != 0) continue;
    channelsHD.insert({listChannel->GetChannelID(),1});
  }
  return channelsHD;
}

// getDefaultChannels  ********************************
std::set<tChannelID> getDefaultChannels() {
  std::set<tChannelID> channels;
  int i = 0;
#if VDRVERSNUM >= 20301
  LOCK_CHANNELS_READ;
  for ( cChannel *listChannel = (cChannel *)Channels->First(); listChannel; listChannel = (cChannel *)Channels->Next( listChannel ) )
#else
  ReadLock channelsLock( Channels );
  if ( !channelsLock ) return channelsHD;
  for ( cChannel *listChannel = Channels.First(); listChannel; listChannel = Channels.Next( listChannel ) )
#endif
  {
    if ( listChannel->GroupSep() || listChannel->Name() == NULL ) continue;
    if (strlen(listChannel->Name())  < 2) continue;
    channels.insert(listChannel->GetChannelID());
    if (i++ > 30) break;
  }
  return channels;
}

cTVScraperConfig::cTVScraperConfig() {
    enableDebug = 0;
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
      esyslog("tvscraper: cTVScraperConfig::loadPlugins cannot open plugin dir %s", PLGDIR);
      return false;
   }

   while ((dp = readdir(dir)))
   {
      if (strncmp(dp->d_name, "libtvscraper-", 13) == 0 && strstr(dp->d_name, ".so"))
      {
         std::string path = concatenate(PLGDIR, "/", dp->d_name);
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
void cTVScraperConfig::Initialize() {
// we don't lock here. This is called during plugin initialize, and there are no other threads at this point in time

// these values are in setup.conf. Set here defaults, if values in setup.conf are missing.
  if (m_HD_Channels.empty() ) {m_HD_Channels = getDefaultHD_Channels(); m_HD_ChannelsModified = false; }
  if (   m_channels.empty() )     m_channels = getDefaultChannels();
  if (m_defaultLanguage == 0) setDefaultLanguage();
// load plugins from ext. epgs
  loadPlugins();
//  m_extEpgs.push_back(std::make_shared<cExtEpgTvsp>());
// read from /var/lib/vdr/plugins/tvscraper/channelmap.conf
  loadChannelmap(m_channelMap, m_extEpgs);
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
        for (const tChannelID &c: getSetFromString<tChannelID>(Value)) m_HD_Channels.insert({c, 1});
        return true;
    } else if (strcmp(Name, "UHDChannels") == 0) {
        config.m_HD_ChannelsModified = true;
        for (const tChannelID &c: getSetFromString<tChannelID>(Value)) m_HD_Channels.insert({c, 2});
        return true;
    } else if (strcmp(Name, "ExcludedRecordingFolders") == 0) {
        m_excludedRecordingFolders = getSetFromString<std::string>(Value, '/');
        return true;
    } else if (strcmp(Name, "TV_Shows") == 0) {
        m_TV_Shows = getSetFromString<int>(Value);
        return true;
    } else if (strcmp(Name, "enableDebug") == 0) {
        enableDebug = atoi(Value);
        return true;
    } else if (strcmp(Name, "enableAutoTimers") == 0) {
        m_enableAutoTimers = atoi(Value);
        return true;
    } else if (strcmp(Name, "defaultLanguage") == 0) {
        m_defaultLanguage = atoi(Value);
        return true;
    } else if (strcmp(Name, "additionalLanguages") == 0) {
        m_AdditionalLanguages = getSetFromString<int>(Value);
        return true;
    } else if (strncmp(Name, "additionalLanguage", 18) == 0) {
        int num_lang = atoi(Name + 18);
        if (num_lang <= 0) return false;
        for (const tChannelID &c: getSetFromString<tChannelID>(Value)) m_channel_language.insert({c, num_lang});
        return true;
    }
    return false;
}

int cTVScraperConfig::GetLanguage_n(const tChannelID &channelID) const {
  int lang;
  {
    cTVScraperConfigLock lr;
    auto l = m_channel_language.find(channelID);
    if (l == m_channel_language.end()) lang = m_defaultLanguage;
    else lang = l->second;
  }
  return lang;
}

const cLanguage *cTVScraperConfig::GetLanguage(const tChannelID &channelID) const {
  int lang;
  {
    cTVScraperConfigLock lr;
    auto l = m_channel_language.find(channelID);
    if (l == m_channel_language.end()) lang = -1;
    else lang = l->second;
  }
  if (lang == -1) return GetDefaultLanguage(); // this is not an error. A channel without explicit language assignment has the default language
  auto r = m_languages.find(lang);
  if(r != m_languages.end()) return &(*r);
  esyslog("tvscraper: ERROR cTVScraperConfig::GetLanguage language %i not found", lang);
  return GetDefaultLanguage();
}

const cLanguage *cTVScraperConfig::GetDefaultLanguage() const {
  int lang;
  { cTVScraperConfigLock l; lang = m_defaultLanguage; }
  auto r = m_languages.find(lang);
  if(r != m_languages.end()) return &(*r);
  const cLanguage *result;
  if (m_languages.size() == 0) result = &m_emergencyLanguage;
  else result = &(*m_languages.begin());
  esyslog("tvscraper: ERROR cTVScraperConfig::getDefaultLanguage r == NULL, m_defaultLanguage = %i, set default language to %s", m_defaultLanguage, result->getNames().c_str() );
  return result;
}
void cTVScraperConfig::setDefaultLanguage() {
  cTVScraperConfigLock lw(true);
  m_defaultLanguage = 5; // en-GB, in case we find nothing ...
  string loc = setlocale(LC_NAME, NULL);
  size_t index = loc.find_first_of("_");
  if (index != 2) {
    esyslog("tvscraper: ERROR cTVScraperConfig::setDefaultLanguage, language %s, index = %zu, use en-GB as default language", loc.c_str(), index);
    return;
  }
  int li = 0;
  int lc = 0;
  for (const cLanguage &lang: m_languages) {
    if (strncmp(lang.m_themoviedb, loc.c_str(), 2) != 0) continue;
    li = lang.m_id;
    if (strncmp(lang.m_themoviedb+3, loc.c_str()+3, 2) == 0) lc = lang.m_id;
  }
  if (li == 0) {
    esyslog("tvscraper: ERROR cTVScraperConfig::setDefaultLanguage, language %s not found, use en-GB as default language", loc.c_str() );
    return;
  }
  if (lc != 0) m_defaultLanguage = lc;
  else m_defaultLanguage = li;
  esyslog("tvscraper: set default language to %s", m_languages.find(m_defaultLanguage)->getNames().c_str() );
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

set<tChannelID> cTVScraperConfig::GetScrapeAndEpgChannels() const {
  set<tChannelID> result;
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

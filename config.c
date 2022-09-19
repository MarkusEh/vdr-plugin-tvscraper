#include <filesystem>
#include <fstream>
#include "config.h"

using namespace std;
// getDefaultHD_Channels  ********************************
std::set<tChannelID> getDefaultHD_Channels() {
  std::set<tChannelID> channelsHD;
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
    channelsHD.insert(listChannel->GetChannelID());
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

void cTVScraperConfig::Initialize() {
// we don't lock here. This is called during plugin initialize, and there are no other threads at this point in time
  if (m_hd_channels.empty() ) m_hd_channels = getDefaultHD_Channels();
  if (   m_channels.empty() )    m_channels = getDefaultChannels();
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
        m_hd_channels = getSetFromString<tChannelID>(Value);
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

void cTVScraperConfig::setDefaultLanguage() {
  cTVScraperConfigLock lw(true);
  m_defaultLanguage = 5; // en-GB, in case we find nothing ...
  string loc = setlocale(LC_NAME, NULL);
  size_t index = loc.find_first_of("_");
  if (index != 2) {
    esyslog("tvscraper: ERROR cTVScraperConfig::setDefaultLanguage, language %s, index = %li, use en-GB as default language", loc.c_str(), index);
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

// implement class cTVScraperConfigLock *************************
cTVScraperConfigLock::cTVScraperConfigLock(bool Write) {
  config.stateLock.Lock(m_stateKey, Write);
}
cTVScraperConfigLock::~cTVScraperConfigLock() {
  m_stateKey.Remove();
}

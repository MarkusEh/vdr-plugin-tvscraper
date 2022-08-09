#include "config.h"

using namespace std;

// getDefaultHD_Channels  ********************************
std::set<std::string> getDefaultHD_Channels() {
  std::set<std::string> channelsHD;
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
    channelsHD.insert(*listChannel->GetChannelID().ToString());
  }
  return channelsHD;
}

// getDefaultChannels  ********************************
std::set<std::string> getDefaultChannels() {
  std::set<std::string> channels;
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
    channels.insert(*listChannel->GetChannelID().ToString());
    if (i++ > 30) break;
  }
  return channels;
}

cTVScraperConfig::cTVScraperConfig() {
    enableDebug = 0;
    enableAutoTimers = 0;
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
  }
}

void cTVScraperConfig::ClearChannels(bool hd) {
    if (hd) hd_channels.clear();
    else channels.clear();
}

void cTVScraperConfig::AddChannel(const string &channelID, bool hd) {
    if (hd) hd_channels.insert(channelID);
    else channels.insert(channelID);   
}

void cTVScraperConfig::Initialize() {
  if (hd_channels.empty() ) hd_channels = getDefaultHD_Channels();
  if (   channels.empty() )    channels = getDefaultChannels();
}

bool cTVScraperConfig::SetupParse(const char *Name, const char *Value) {
    if (strcmp(Name, "ScrapChannels") == 0) {
        channels = stringToSet(Value, ';');
        return true;
    } else if (strcmp(Name, "HDChannels") == 0) {
        hd_channels = stringToSet(Value, ';');
        return true;
    } else if (strcmp(Name, "ExcludedRecordingFolders") == 0) {
        m_excludedRecordingFolders = stringToSet(Value, '/');
        return true;
    } else if (strcmp(Name, "TV_Shows") == 0) {
        m_TV_Shows = stringToIntSet(Value, ';');
        return true;
    } else if (strcmp(Name, "enableDebug") == 0) {
        enableDebug = atoi(Value);
        return true;
    } else if (strcmp(Name, "enableAutoTimers") == 0) {
        enableAutoTimers = atoi(Value);
        return true;
    }
    return false;
}

void cTVScraperConfig::PrintChannels(void) {
    int numChannels = channels.size();
    esyslog("tvscraper: %d channel to be scrapped", numChannels);
    for (const std::string &chan: channels) {
        esyslog("tvscraper: channel to be scrapped: %s", chan.c_str());
    }
}

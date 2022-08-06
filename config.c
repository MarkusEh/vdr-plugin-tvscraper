#include "config.h"

using namespace std;

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

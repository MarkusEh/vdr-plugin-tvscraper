#include <locale.h>
#include <vdr/menu.h>
#include "worker.h"

using namespace std;

cTVScraperWorker::cTVScraperWorker(cTVScraperDB *db, cOverRides *overrides) : cThread("tvscraper", true) {
    startLoop = true;
    scanVideoDir = false;
    manualScan = false;
    this->db = db;
    this->overrides = overrides;
    moviedbScraper = NULL;
    tvdbScraper = NULL;
//    initSleep = 2 * 60 * 1000;
    initSleep =     10 * 1000;
    loopSleep = 5 * 60 * 1000;
    language = "";
}

cTVScraperWorker::~cTVScraperWorker() {
    if (moviedbScraper)
        delete moviedbScraper;
    if (tvdbScraper)
        delete tvdbScraper;
}

void cTVScraperWorker::SetLanguage(void) {
    string loc = setlocale(LC_NAME, NULL);
    size_t index = loc.find_first_of("_");
    string langISO = "";
    if (index > 0) {
        langISO = loc.substr(0, index);
    }
    if (langISO.size() == 2) {
        language = langISO.c_str();
        dsyslog("tvscraper: using language %s", language.c_str());
        return;
    }
    language = "en";
    dsyslog("tvscraper: using fallback language %s", language.c_str());
}

void cTVScraperWorker::Stop(void) {
    waitCondition.Broadcast();    // wakeup the thread
    Cancel(5);                    // wait up to 5 seconds for thread was stopping
    db->BackupToDisc();
}

void cTVScraperWorker::InitVideoDirScan(void) {
    scanVideoDir = true;
    waitCondition.Broadcast();
}

void cTVScraperWorker::InitManualScan(void) {
    manualScan = true;
    waitCondition.Broadcast();
}

void cTVScraperWorker::SetDirectories(void) {
    plgBaseDir = config.GetBaseDir();
    stringstream strSeriesDir;
    strSeriesDir << plgBaseDir << "/series";
    seriesDir = strSeriesDir.str();
    stringstream strMovieDir;
    strMovieDir << plgBaseDir << "/movies";
    movieDir = strMovieDir.str();
    bool ok = false;
    ok = CreateDirectory(plgBaseDir);
    if (ok)
        ok = CreateDirectory(seriesDir);
    if (ok)
        ok = CreateDirectory(movieDir);
    if (ok)
        ok = CreateDirectory(movieDir + "/tv");
    if (!ok) {
        esyslog("tvscraper: ERROR: check %s for write permissions", plgBaseDir.c_str());
        startLoop = false;
    } else {
        dsyslog("tvscraper: using base directory %s", plgBaseDir.c_str());
    }
}

scrapType cTVScraperWorker::GetScrapType(const cEvent *event) {
  scrapType type = scrapNone;
  string title = (event->Title())?event->Title():"";
  type = overrides->Type(title);
  if (type != scrapNone) return type;
  int duration = event->Duration() / 60;
  if ((duration > 4) && (duration <= 75)) {
    type = scrapSeries;
  } else if (duration > 75) {
    type = scrapMovie;
  }
  return type;
}

bool cTVScraperWorker::ConnectScrapers(void) {
if (!moviedbScraper) {
moviedbScraper = new cMovieDBScraper(movieDir, db, language, overrides);
if (!moviedbScraper->Connect()) {
    esyslog("tvscraper: ERROR, connection to TheMovieDB failed");
    delete moviedbScraper;
    moviedbScraper = NULL;
    return false;
}
}
if (!tvdbScraper) {
tvdbScraper = new cTVDBScraper(seriesDir, db, language);
if (!tvdbScraper->Connect()) {
    esyslog("tvscraper: ERROR, connection to TheTVDB failed");
    delete tvdbScraper;
    tvdbScraper = NULL;
    return false;
}
}
return true;
}

void cTVScraperWorker::DisconnectScrapers(void) {
if (moviedbScraper) {
delete moviedbScraper;
moviedbScraper = NULL;
}
if (tvdbScraper) {
delete tvdbScraper;
tvdbScraper = NULL;
}
}

void cTVScraperWorker::ScrapEPG(void) {
vector<string> channels = config.GetChannels();
int numChannels = channels.size();
for (int i=0; i<numChannels; i++) {
string channelID = channels[i];
#if APIVERSNUM < 20301
const cChannel *channel = Channels.GetByChannelID(tChannelID::FromString(channelID.c_str()));
#else
LOCK_CHANNELS_READ;
const cChannel *channel = Channels->GetByChannelID(tChannelID::FromString(channelID.c_str()));
#endif
if (!channel) {
    dsyslog("tvscraper: Channel %s %s is not availible, skipping", channel->Name(), channelID.c_str());
    continue;
}
dsyslog("tvscraper: scraping Channel %s %s", channel->Name(), channelID.c_str());
#if APIVERSNUM < 20301
cSchedulesLock schedulesLock;
const cSchedules *schedules = cSchedules::Schedules(schedulesLock);
const cSchedule *Schedule = schedules->GetSchedule(channel);
#else
LOCK_SCHEDULES_READ;
const cSchedule *Schedule = Schedules->GetSchedule(channel);
#endif
if (Schedule) {
    const cEvent *event = NULL;
    time_t now = time(0);
    for (event = Schedule->Events()->First(); event; event =  Schedule->Events()->Next(event)) {
//	if(event->EventID() == 14724) dsyslog("tvscraper: scraping event %i, %s", event->EventID(), event->Title() );
        if (event->EndTime() < now) continue; // do not scrap past events. Avoid to scrap them, and delete directly afterwards
	if (!Running())
	    return;
	scrapType type = GetScrapType(event);
	if (type != scrapNone) {
	    if( Scrap(event, NULL) ) waitCondition.TimedWait(mutex, 100);
	}
    }
} else {
    dsyslog("tvscraper: Schedule is not availible, skipping");
}
}
}

void cTVScraperWorker::ScrapRecordings(void) {
db->ClearRecordings2();
#if APIVERSNUM < 20301
for (cRecording *rec = Recordings.First(); rec; rec = Recordings.Next(rec)) {
#else
LOCK_RECORDINGS_READ;
for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec)) {
#endif
if (overrides->IgnorePath(rec->FileName()))
    continue;
const cRecordingInfo *recInfo = rec->Info();
const cEvent *recEvent = recInfo->GetEvent();
if (recEvent) {
    scrapType type = GetScrapType(recEvent);
    if (type != scrapNone) Scrap(recEvent, rec);
}
}
}

void cTVScraperWorker::CheckRunningTimers(void) {
#if APIVERSNUM < 20301
for (cTimer *timer = Timers.First(); timer; timer = Timers.Next(timer)) {
#else
LOCK_TIMERS_READ;
for (const cTimer *timer = Timers->First(); timer; timer = Timers->Next(timer)) if (timer->Local() ) {
#endif
if (timer->Recording()) {
    const cEvent *event = timer->Event();
    if (!event)
	continue;
    scrapType type = GetScrapType(event);
    if (type != scrapNone) {
// figure out recording
        const cRecording *recording = NULL;
        cRecordControl *rc = cRecordControls::GetRecordControl(timer);
        if (!rc) {
          esyslog("tvscraper: ERROR cTVScraperWorker::CheckRunningTimers: Timer is recording, but there is no cRecordControls::GetRecordControl(timer)");
        } else {
#if APIVERSNUM < 20301
        recording = Recordings.GetByName(rc->FileName() );
#else
LOCK_RECORDINGS_READ;
        recording = Recordings->GetByName(rc->FileName() );
#endif
          if (!recording) esyslog("tvscraper: ERROR cTVScraperWorker::CheckRunningTimers: no recording for file \"%s\"", rc->FileName() );
        }
	if (!db->SetRecording(event, recording)) {
	    if (ConnectScrapers() && recording) Scrap(event, recording);
	}
    }
}
}
DisconnectScrapers();
}

bool cTVScraperWorker::StartScrapping(void) {
if (manualScan) {
manualScan = false;
return true;
}
//wait at least one day from last scrapping to scrap again
int minTime = 24 * 60 * 60;
return db->CheckStartScrapping(minTime);
}


void cTVScraperWorker::Action(void) {
if (!startLoop)
return;

mutex.Lock();
dsyslog("tvscraper: waiting %d minutes to start main loop", initSleep / 1000 / 60);
waitCondition.TimedWait(mutex, initSleep);

while (Running()) {
if (scanVideoDir) {
    scanVideoDir = false;
    dsyslog("tvscraper: scanning video dir");
    if (ConnectScrapers()) {
	ScrapRecordings();
    }
    DisconnectScrapers();
    continue;
}
CheckRunningTimers();
if (StartScrapping()) {
    dsyslog("tvscraper: start scraping epg");
    if (ConnectScrapers()) {
	ScrapEPG();
    }
    DisconnectScrapers();
    db->ClearOutdated(movieDir, seriesDir);
    db->BackupToDisc();
    dsyslog("tvscraper: epg scraping done");
}
waitCondition.TimedWait(mutex, loopSleep);
}
}

bool cTVScraperWorker::Scrap(const cEvent *event, const cRecording *recording) {
// return true, if request to rate limited internet db was required. Otherwise, false
   sMovieOrTv movieOrTv;
   bool internet_req_required = ScrapFindAndStore(movieOrTv, event, recording);
   Scrap_assign(movieOrTv, event, recording) ;
   return internet_req_required;
}

bool cTVScraperWorker::ScrapFindAndStore(sMovieOrTv &movieOrTv, const cEvent *event, const cRecording *recording) {
// if nothing is found => movieOrTv.scrapType = scrapNone;
// return true, if request to rate limited internet db was required. Otherwise, false

// set default values (nothing found)
movieOrTv.type = scrapNone;
movieOrTv.episodeSearchWithShorttext = false;
string movieName = (event->Title())?event->Title():"";
if (overrides->Ignore(movieName)) return false;
if (!overrides->Substitute(movieName) ) {
// some more, but only if no explicit substitution
  movieName = overrides->RemovePrefix(movieName);
  StringRemoveLastPartWithP(movieName); // always remove this: Year, number of season/episode, ... but not a part of the name
}
string episodeSearchString = "";
string movieNameCache = movieName;
bool cache_or_overrides_found = false;
movieOrTv.id = overrides->thetvdbID(movieName);
if (movieOrTv.id) {
  cache_or_overrides_found = true;
  movieOrTv.type = scrapSeries;
  movieOrTv.episodeSearchWithShorttext = true;
  tvdbScraper->StoreSeries(movieOrTv.id, false);
  movieOrTv.id *= -1;
} else {
// check cache
  cache_or_overrides_found = db->GetFromCache(movieName, event, recording, movieOrTv);
//  map<string,sMovieOrTv>::iterator cacheHit = cache.find(movieName);
//  if (cacheHit != cache.end()) movieOrTv = cacheHit->second;
}

if (cache_or_overrides_found) {
  if(movieOrTv.episodeSearchWithShorttext) {
    if(event->ShortText() && *event->ShortText() ) episodeSearchString = event->ShortText();
       else if(event->Description() && *event->Description() ) episodeSearchString = event->Description();
    movieOrTv.episodeSearchStringEmpty = episodeSearchString.empty();
    UpdateEpisodeListIfRequired(movieOrTv.id);
    db->SearchEpisode(movieOrTv, episodeSearchString);
  }
  if (!config.enableDebug) return false;
  switch (movieOrTv.type) {
    case scrapNone:
      esyslog("tvscraper: found cache %s => scrapNone", movieName.c_str());
      return false;
    case scrapMovie:
      esyslog("tvscraper: found movie cache %s => %i", movieName.c_str(), movieOrTv.id);
      return false;
    case scrapSeries:
      esyslog("tvscraper: found %stv cache tv \"%s\", episode \"%s\" , id %i season %i episode %i", movieOrTv.id < 0?"TV":"", movieName.c_str(), episodeSearchString.c_str(), movieOrTv.id, movieOrTv.season, movieOrTv.episode);
      return false;
    default:
      esyslog("tvscraper: ERROR found cache %s => unknown scrap type %i", movieName.c_str(), movieOrTv.type);
      return false;
  }
}
if (config.enableDebug) esyslog("tvscraper: scraping \"%s\"", movieName.c_str());
scrapType type_override = overrides->Type(movieName);
cTVDBSeries TVtv(db, tvdbScraper);
cMovieDbTv tv(db, moviedbScraper);
cMovieDbMovie movie(db, moviedbScraper, event, recording);
searchResultTvMovie searchResult;
scrapType sType = Search(&TVtv, &tv, &movie, movieName, event, recording, type_override, searchResult);
    if (sType == scrapNone) {
// check: series, format name: episode_name
      std::size_t found = movieName.find(":");
      if(found != std::string::npos && found > 4) {
        std::size_t ssnd;
        for(ssnd = found + 1; ssnd < movieName.length() && movieName[ssnd] == ' '; ssnd++);
        if(movieName.length() - ssnd > 4) {
          string orig_movieName = movieName;
          episodeSearchString = movieName.substr(ssnd);
          movieName.erase(found);
          StringRemoveTrailingWhitespace(movieName);
          sType = Search(&TVtv, &tv, NULL, movieName, event, recording, type_override, searchResult);
          if(sType == scrapNone) {
            movieName = orig_movieName;
            episodeSearchString.clear();
          }
        }
      }
    }
    if (sType == scrapNone) {
// check: series, format name - episode_name
      std::size_t found = movieName.find("-");
      if(found != std::string::npos && found > 4) {
        std::size_t ssnd;
        for(ssnd = found + 1; ssnd < movieName.length() && movieName[ssnd] == ' '; ssnd++);
        if(movieName.length() - ssnd > 4) {
          string orig_movieName = movieName;
          episodeSearchString = movieName.substr(ssnd);
          movieName.erase(found);
          StringRemoveTrailingWhitespace(movieName);
          sType = Search(&TVtv, &tv, NULL, movieName, event, recording, type_override, searchResult);
          if(sType == scrapNone) {
            movieName = orig_movieName;
            episodeSearchString.clear();
          }
        }
      }
    }

    movieOrTv.id = 0; // default, if nothing was found
    if(sType == scrapMovie) {
      if (config.enableDebug) esyslog("tvscraper: movie \"%s\" successfully scraped, id %i", movieName.c_str(), movie.ID());
      moviedbScraper->StoreMovie(movie);
      movieOrTv.type = scrapMovie;
      movieOrTv.id = movie.ID();
      movieOrTv.season = -100;
      movieOrTv.episode = 0;
      movieOrTv.episodeSearchStringEmpty = false;
      movieOrTv.year = searchResult.year;
    } else if(sType == scrapSeries) {
// search episode
      movieOrTv.type = scrapSeries;
      movieOrTv.episodeSearchWithShorttext = episodeSearchString.empty();
      movieOrTv.year = searchResult.year;
      if (movieOrTv.episodeSearchWithShorttext) {
        if(event->ShortText() && *event->ShortText() ) episodeSearchString = event->ShortText();
           else if(event->Description() && *event->Description() ) episodeSearchString = event->Description();
      }
      movieOrTv.episodeSearchStringEmpty = episodeSearchString.empty();
      if(tv.tvID()) {
// entry for tv series in MovieDB found
        movieOrTv.id = tv.tvID();
        tv.UpdateDb();
        UpdateEpisodeListIfRequired(movieOrTv.id);
        db->SearchEpisode(movieOrTv, episodeSearchString);
        if (config.enableDebug) esyslog("tvscraper: tv \"%s\", episode \"%s\" successfully scraped, id %i season %i episode %i", movieName.c_str(), episodeSearchString.c_str(), movieOrTv.id, movieOrTv.season, movieOrTv.episode);
      } else {
// entry in TVDB found
        int seriesID = tvdbScraper->StoreSeries(TVtv.ID(), false);
        if(seriesID != TVtv.ID()) esyslog("tvscraper: seriesID != TVtv.tvID(), seriesID %i TVtv.tvID() %i", seriesID, TVtv.ID() );
        if(seriesID > 0) {
          movieOrTv.id = seriesID * (-1);
          UpdateEpisodeListIfRequired(movieOrTv.id);
          db->SearchEpisode(movieOrTv, episodeSearchString);
          if (config.enableDebug) esyslog("tvscraper: TVtv \"%s\", episode \"%s\" successfully scraped, id %i season %i episode %i", movieName.c_str(), episodeSearchString.c_str(), movieOrTv.id, movieOrTv.season, movieOrTv.episode);
        }
      }
    } else if (config.enableDebug) esyslog("tvscraper: nothing found for \"%s\" ", movieName.c_str());
  db->InsertCache(movieNameCache, event, recording, movieOrTv);
//    cache.insert(pair<string, sMovieOrTv>(movieNameCache, movieOrTv));
  return true;
}
void cTVScraperWorker::Scrap_assign(const sMovieOrTv &movieOrTv, const cEvent *event, const cRecording *recording) {
// assig found movieOrTv to event/recording in db
  if(movieOrTv.type == scrapMovie || movieOrTv.type == scrapSeries) {
    if (!recording) db->InsertEvent(event, recording, movieOrTv.id, movieOrTv.season, movieOrTv.episode, movieOrTv.episodeSearchStringEmpty );
               else db->InsertRecording2(event, recording, movieOrTv.id, movieOrTv.season, movieOrTv.episode);
  }
}
scrapType cTVScraperWorker::Search(cTVDBSeries *TVtv, cMovieDbTv *tv, cMovieDbMovie *movie, const string &name, const cEvent *event, const cRecording *recording, scrapType type_override, searchResultTvMovie &searchResult) {
  if((tv || TVtv) && ! movie){
    map<string,searchResultTvMovie>::iterator cacheHit = cacheTv.find(name);
    if (cacheHit != cacheTv.end() && (TVtv || (cacheHit->second).id > 0) ) {
      if((int)(cacheHit->second).id > 0) {
        if (config.enableDebug) esyslog("tvscraper: found tv cache %s => %i", ((string)cacheHit->first).c_str(), (int)cacheHit->second.id);
        if (tv) tv->SetTvID((int)cacheHit->second.id);
            else esyslog("tvscraper: ERROR, cTVScraperWorker::Search, tv == NULL");
      } else {
        if (config.enableDebug) esyslog("tvscraper: found TVtv cache %s => %i", ((string)cacheHit->first).c_str(), (int)cacheHit->second.id);
        if (TVtv) TVtv->SetSeriesID((int)cacheHit->second.id * (-1));
            else esyslog("tvscraper: ERROR, cTVScraperWorker::Search, TVtv == NULL");
      }
      searchResult = cacheHit->second;
      return scrapSeries;
    }
  }
//convert searchstring to lower case
  string searchString = name;
  transform(searchString.begin(), searchString.end(), searchString.begin(), ::tolower);

  vector<searchResultTvMovie> resultSet;

  switch (type_override) {
    case scrapMovie:
      if(movie) movie->AddMovieResults(resultSet, searchString);
      break;
    case scrapSeries:
      if(tv) tv->AddTvResults(resultSet, searchString);
      if(TVtv) TVtv->AddResults(resultSet, searchString);
      break;
    default:
      if(tv) tv->AddTvResults(resultSet, searchString);
      if(movie) movie->AddMovieResults(resultSet, searchString);
      if(TVtv) TVtv->AddResults(resultSet, searchString);
  }
  if (!FindBestResult2(resultSet, searchResult, event, recording)) return scrapNone; // nothing found
  if(searchResult.movie) {
    int movieID = searchResult.id;
    movie->SetID( movieID);
    return scrapMovie;
  } else {
    int tvID = searchResult.id;
    if(!movie) cacheTv.insert(pair<string, searchResultTvMovie>(name, searchResult));
    if(tv) tv->SetTvID(0);
    if(TVtv) TVtv->SetSeriesID(0);
    if (tvID > 0) tv->SetTvID(tvID);
       else  TVtv->SetSeriesID(tvID * (-1));
    return scrapSeries;
  }
}

bool CompareSearchResult (searchResultTvMovie i, searchResultTvMovie j) {
  if (i.distance == j.distance) return i.popularity > j.popularity;
  return i.distance < j.distance;
}

bool cTVScraperWorker::FindBestResult2(vector<searchResultTvMovie> &resultSet, searchResultTvMovie &searchResult, const cEvent *event, const cRecording *recording){
    std::sort (resultSet.begin(), resultSet.end(), CompareSearchResult);

    for( const searchResultTvMovie &sR : resultSet ) {
      if (sR.distance < 250) { searchResult = sR; return true; }
      if (sR.year) { searchResult = sR; return true; }
    }

    int durationInMinLow;
    int durationInMinHigh;
    if (GetDurationRange(event, recording, durationInMinLow, durationInMinHigh) ) {
      for( const searchResultTvMovie &sR : resultSet ) if (sR.movie) {
        int runtime = moviedbScraper->GetMovieRuntime(sR.id);
        if (runtime >= durationInMinLow && runtime <= durationInMinHigh) {
          if (config.enableDebug) esyslog("tvscraper: runtime %i durationInMinLow %i durationInMinHigh %i id %i title \"%s\"", runtime, durationInMinLow, durationInMinHigh, sR.id, event?event->Title():"No event");
          searchResult = sR;
          return true;
        } else
          if (config.enableDebug) esyslog("tvscraper: not selected, runtime %i durationInMinLow %i durationInMinHigh %i id %i title \"%s\"", runtime, durationInMinLow, durationInMinHigh, sR.id, event?event->Title():"No event");
      }
    }

    return false;
}
bool cTVScraperWorker::GetDurationRange(const cEvent *event, const cRecording *recording, int &durationInMinLow, int &durationInMinHigh) {
// return true, if data is available
    int durationInSec = 0;
    if (recording) {
      durationInSec = recording->FileName() ? recording->LengthInSeconds() : 0;
      if (recording->IsEdited() ) {
        durationInMinLow  = durationInSec * 60 - 1;
        durationInMinHigh = durationInSec * 60 + 10;
      } else {
        durationInMinLow  = durationInSec * 60 - 13; // (3 Vorlauf, 10 Nachlauf)
        durationInMinLow  = durationInMinLow - durationInMinLow / 5 - 5;
        durationInMinHigh = durationInSec * 60 + 10; // might be VPS
      }
    } else if (event) {
      durationInSec = event->Duration();
      if (event->Vps() ) {
        durationInMinLow  = durationInSec * 60 - 1;
        durationInMinHigh = durationInSec * 60 + 10;
      } else {
        durationInMinLow  = durationInSec * 60;
        durationInMinLow  = durationInMinLow - durationInMinLow / 5 - 5;
        durationInMinHigh = durationInSec * 60 + 1;
      }
    }
    return durationInSec != 0;
}

void cTVScraperWorker::FindBestResult(const vector<searchResultTvMovie> &resultSet, searchResultTvMovie &searchResult){
    int bestMatch = -1;
    int numResults = resultSet.size();
    for (int i=0; i<numResults; i++) {
        if (i == 0) {
            bestMatch = resultSet[i].distance;
            searchResult = resultSet[i];
        } else if (resultSet[i].distance < bestMatch) {
            bestMatch = resultSet[i].distance;
            searchResult = resultSet[i];
        }
    }
    return;
}

void cTVScraperWorker::UpdateEpisodeListIfRequired(int tvID) {
// check: is update required?
  string status;
  time_t lastUpdated = 0;
  time_t now = time(0);

  db->GetTv(tvID, lastUpdated, status);
  if (lastUpdated) {
    if (difftime(now, lastUpdated) < 7*24*60*60 ) return; // min one week between updates
    if (status.compare("Ended") == 0) return; // see https://thetvdb-api.readthedocs.io/api/series.html
// not documented for themoviedb, see https://developers.themoviedb.org/3/tv/get-tv-details . But test indicates the same value ...
  }
// update is required
  if (tvID > 0) {
    cMovieDbTv tv(db, moviedbScraper);
    tv.SetTvID(tvID);
    tv.UpdateDb();
  } else {
    tvdbScraper->StoreSeries(tvID * (-1), true);
  }
}


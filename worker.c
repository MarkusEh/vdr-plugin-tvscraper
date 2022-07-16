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
    initSleep =     60 * 1000;
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
    if (config.GetBaseDirLen() == 0) {
      esyslog("tvscraper: ERROR: no base dir");
      startLoop = false;
      return;
    }
    plgBaseDir = config.GetBaseDir();
    stringstream strSeriesDir;
    strSeriesDir << plgBaseDir << "series";
    seriesDir = strSeriesDir.str();
    stringstream strMovieDir;
    strMovieDir << plgBaseDir << "movies";
    movieDir = strMovieDir.str();
    bool ok = false;
    ok = CreateDirectory(plgBaseDir.substr(0, config.GetBaseDirLen()-1 ));
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

bool cTVScraperWorker::ScrapEPG(void) {
// true if one or more new events were scraped
  bool newEvent = false;
  if (config.GetReadOnlyClient() ) return newEvent;
// check far changes in schedule (works only if APIVERSNUM >= 20301
#if APIVERSNUM >= 20301
  if (!cSchedules::GetSchedulesRead(schedulesStateKey)) {
    if (config.enableDebug) dsyslog("tvscraper: Schedule was not changed, skipping scan");
    return newEvent;
  }
  schedulesStateKey.Remove();
#endif
// loop over all channels configured for scraping, and create map to enable check for new events
  map<string, set<int>*> currentEvents;
  for (const string &channelID: config.GetChannels() ) {
    map<string, set<int>*>::iterator currentEventsCurrentChannelIT = currentEvents.find(channelID);
    if (currentEventsCurrentChannelIT == currentEvents.end() ) {
      currentEvents.insert(pair<string, set<int>*>(channelID, new set<int>));
      currentEventsCurrentChannelIT = currentEvents.find(channelID);
    }
    map<string, set<int>*>::iterator lastEventsCurrentChannelIT = lastEvents.find(channelID);
// start the block where we lock channels. After this block, all locks are released
    {
#if APIVERSNUM < 20301
      const cChannel *channel = Channels.GetByChannelID(tChannelID::FromString(channelID.c_str()));
#else
      LOCK_CHANNELS_READ;
      const cChannel *channel = Channels->GetByChannelID(tChannelID::FromString(channelID.c_str()));
#endif
      if (!channel) {
          dsyslog("tvscraper: Channel %s is not availible, skipping. Most likely this channel does not exist. To get rid of this message, goto tvscraper settings and edit the channel list.", channelID.c_str());
          continue;
      }

// now get and lock the schedule
#if APIVERSNUM < 20301
      cSchedulesLock schedulesLock;
      const cSchedules *Schedules = cSchedules::Schedules(schedulesLock);
#else
      LOCK_SCHEDULES_READ;
#endif
      bool newEventSchedule = false;
      const cSchedule *Schedule = Schedules->GetSchedule(channel);
      if (Schedule) {
        for (const cEvent *event = Schedule->Events()->First(); event; event =  Schedule->Events()->Next(event)) {
          if (event->EndTime() < time(0) ) continue; // do not scrape past events. Avoid to scrape them, and delete directly afterwards
          if (!Running()) {
            for (auto &event: currentEvents) delete event.second;
	    return newEvent;
          }
// insert this event in the list of screped events
          currentEventsCurrentChannelIT->second->insert(event->EventID() );
// check: was this event scraped already?
          if (lastEventsCurrentChannelIT == lastEvents.end() ||
              lastEventsCurrentChannelIT->second->find(event->EventID()) == lastEventsCurrentChannelIT->second->end() ) {
// this event was not yet scraped, scrape it now
            newEvent = true;
            if (!newEventSchedule) {
              dsyslog("tvscraper: scraping Channel %s %s", channel->Name(), channelID.c_str());
              newEventSchedule = true;
            }
  	    csEventOrRecording sEvent(event);
  	    cSearchEventOrRec SearchEventOrRec(&sEvent, overrides, moviedbScraper, tvdbScraper, db);
      	    SearchEventOrRec.Scrape();
          }
        }
      } else dsyslog("tvscraper: Schedule for channel %s %s is not availible, skipping", channel->Name(), channelID.c_str());
    } // end of block with locks
    waitCondition.TimedWait(mutex, 100);
  } // end loop over all channels
  currentEvents.swap(lastEvents);
  for (auto &event: currentEvents) delete event.second;
  return newEvent;
}

void cTVScraperWorker::ScrapRecordings(void) {
  if (config.GetReadOnlyClient() ) return;
  db->ClearRecordings2();

#if APIVERSNUM < 20301
  for (cRecording *rec = Recordings.First(); rec; rec = Recordings.Next(rec)) {
    if (overrides->IgnorePath(rec->FileName())) continue;
    {
#else
  vector<string> recordingFileNames;
  {
    LOCK_RECORDINGS_READ;
    for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec)) {
      if (overrides->IgnorePath(rec->FileName())) continue;
      if (rec->FileName()) recordingFileNames.push_back(rec->FileName());
    }
  }
  for (const string &filename: recordingFileNames) {
    {
      LOCK_RECORDINGS_READ;
      const cRecording *rec = Recordings->GetByName(filename.c_str() );
      if (!rec) continue;
#endif
      if (config.enableDebug) esyslog("tvscraper: Scrap recording \"%s\"", rec->FileName() );

      csRecording csRecording(rec);
      const cRecordingInfo *recInfo = rec->Info();
      const cEvent *recEvent = recInfo->GetEvent();
      if (recEvent) {
          cSearchEventOrRec SearchEventOrRec(&csRecording, overrides, moviedbScraper, tvdbScraper, db);  
          SearchEventOrRec.Scrape();
          if (!Running() ) break;
      }
    }
// here, the read lock is released, so wait a sort time, in case someone needs a write lock
    waitCondition.TimedWait(mutex, 100);
  }
}

bool cTVScraperWorker::TimersRunningPlanned(double nextMinutes) {
// return true is a timer is running, or a timer will start within the next nextMinutes minutes
// otherwise false
  if (config.GetReadOnlyClient() ) return true;
#if APIVERSNUM < 20301
  for (cTimer *timer = Timers.First(); timer; timer = Timers.Next(timer)) {
#else
  LOCK_TIMERS_READ;
  for (const cTimer *timer = Timers->First(); timer; timer = Timers->Next(timer)) if (timer->Local() ) {
#endif
    if (timer->Recording()) return true;
    if (difftime(timer->StartTime(), time(0) )*60 < nextMinutes) return true;
  }
  return false;
}

void cTVScraperWorker::CheckRunningTimers(void) {
  if (config.GetReadOnlyClient() ) return;
#if APIVERSNUM < 20301
  for (cTimer *timer = Timers.First(); timer; timer = Timers.Next(timer))
#else
  LOCK_TIMERS_READ;
  for (const cTimer *timer = Timers->First(); timer; timer = Timers->Next(timer)) if (timer->Local() )
#endif
  if (timer->Recording()) {
// figure out recording
    cRecordControl *rc = cRecordControls::GetRecordControl(timer);
    if (!rc) {
      esyslog("tvscraper: ERROR cTVScraperWorker::CheckRunningTimers: Timer is recording, but there is no cRecordControls::GetRecordControl(timer)");
      continue;
    }
#if APIVERSNUM < 20301
    const cRecording *recording = Recordings.GetByName(rc->FileName() );
#else
    LOCK_RECORDINGS_READ;
    const cRecording *recording = Recordings->GetByName(rc->FileName() );
#endif
    if (!recording) {
      esyslog("tvscraper: ERROR cTVScraperWorker::CheckRunningTimers: no recording for file \"%s\"", rc->FileName() );
      continue;
    }
    csRecording sRecording(recording);

    if (db->SetRecording(&sRecording)) {
      cMovieOrTv *movieOrTv = cMovieOrTv::getMovieOrTv(db, &sRecording);
      if (movieOrTv) {
        movieOrTv->copyImagesToRecordingFolder(recording);
        delete movieOrTv;
      }
    } else {
      tEventID eventID = sRecording.EventID();
      cString channelIDs = sRecording.ChannelIDs();
      esyslog("tvscraper: cTVScraperWorker::CheckRunningTimers: no entry in table event found for eventID %i, channelIDs %s, recording for file \"%s\"", eventID, (const char *)channelIDs, rc->FileName() );
      if (ConnectScrapers() ) { 
        cSearchEventOrRec SearchEventOrRec(&sRecording, overrides, moviedbScraper, tvdbScraper, db);  
        SearchEventOrRec.Scrape();
      }
    }
  }
}

bool cTVScraperWorker::StartScrapping(void) {
  if (!manualScan && TimersRunningPlanned(15.) ) return false;
  bool resetScrapeTime = false;
  if (manualScan) {
    manualScan = false;
    for (auto &event: lastEvents) event.second->clear();
    resetScrapeTime = true;
  }
  if (resetScrapeTime || lastEvents.empty() ) {
// a full scrape will be done, write this to db, so next full scrape will be in one day
    db->execSql("delete from scrap_checker", "");
    char sql[] = "INSERT INTO scrap_checker (last_scrapped) VALUES (?)";
    db->execSql(sql, "t", time(0));
    return true;
  }
// Delete the scraped event IDs once a day, to update data
  int minTime = 24 * 60 * 60;
  if (db->CheckStartScrapping(minTime)) for (auto &event: lastEvents) event.second->clear();
  return true;
}

void cTVScraperWorker::Action(void) {
  if (!startLoop) return;
  if (config.GetReadOnlyClient() ) return;

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
//    DisconnectScrapers();
      db->BackupToDisc();
      dsyslog("tvscraper: scanning video dir done");
      continue;
    }
    CheckRunningTimers();
    if (!Running() ) break;
    if (StartScrapping()) {
      dsyslog("tvscraper: start scraping epg");
      if (ConnectScrapers()) {
        bool newEvents = ScrapEPG();
        if (newEvents && Running() ) cMovieOrTv::DeleteAllIfUnused(db);
        if (newEvents) db->BackupToDisc();
      }
      dsyslog("tvscraper: epg scraping done");
    }
    waitCondition.TimedWait(mutex, loopSleep);
  }
}

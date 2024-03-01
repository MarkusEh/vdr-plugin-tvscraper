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
    m_movieDbMovieScraper = NULL;
    m_movieDbTvScraper = NULL;
    m_tvDbTvScraper = NULL;
    initSleep = 2 * 60 * 1000;  // todo: wait for video directory scanner thread ended
//    initSleep =     60 * 1000;
    loopSleep = 5 * 60 * 1000;
    lastTimerRecordingCheck = 0;
}

cTVScraperWorker::~cTVScraperWorker() {
    if (m_movieDbMovieScraper) delete m_movieDbMovieScraper;
    if (m_movieDbTvScraper) delete m_movieDbTvScraper;
    if (m_tvDbTvScraper) delete m_tvDbTvScraper;
    if (moviedbScraper) delete moviedbScraper;
    if (tvdbScraper) delete tvdbScraper;
}

void cTVScraperWorker::Stop(void) {
    waitCondition.Broadcast();    // wakeup the thread
    Cancel(210);                    // wait up to 210 seconds for thread was stopping
//    db->BackupToDisc();
}

void cTVScraperWorker::InitVideoDirScan(const char *recording) {
    m_recording = cSv(recording);
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
    bool ok =    CreateDirectory(config.GetBaseDir() );
    if (ok) ok = CreateDirectory(config.GetBaseDirEpg() );
    if (ok) ok = CreateDirectory(config.GetBaseDirRecordings() );
    if (ok) ok = CreateDirectory(config.GetBaseDirSeries() );
    if (ok) ok = CreateDirectory(config.GetBaseDirMovies() );
    if (ok) ok = CreateDirectory(config.GetBaseDirMovieActors() );
    if (ok) ok = CreateDirectory(config.GetBaseDirMovieCollections() );
    if (ok) ok = CreateDirectory(config.GetBaseDirMovieTv() );
    if (!ok) {
        esyslog("tvscraper: ERROR: check %s for write permissions", config.GetBaseDir().c_str());
        startLoop = false;
    } else {
        dsyslog("tvscraper: using base directory %s", config.GetBaseDir().c_str());
    }
}

bool cTVScraperWorker::ConnectScrapers(void) {
  if (!moviedbScraper) {
    moviedbScraper = new cMovieDBScraper(db, overrides);
    if (!moviedbScraper->Connect()) {
      esyslog("tvscraper: ERROR, connection to TheMovieDB failed");
      delete moviedbScraper;
      moviedbScraper = NULL;
      return false;
    }
  }
  if (!tvdbScraper) {
    tvdbScraper = new cTVDBScraper(db);
    if (!tvdbScraper->Connect()) {
      esyslog("tvscraper: ERROR, connection to TheTVDB failed");
      delete tvdbScraper;
      tvdbScraper = NULL;
      return false;
    }
  }
  if (!m_movieDbMovieScraper) m_movieDbMovieScraper = new cMovieDbMovieScraper(moviedbScraper);
  if (!m_movieDbTvScraper) m_movieDbTvScraper = new cMovieDbTvScraper(moviedbScraper);
  if (!m_tvDbTvScraper) m_tvDbTvScraper = new cTvDbTvScraper(tvdbScraper);
  return true;
}

bool GetChannelName(std::string &channelName, const tChannelID &channelid, int &msg_cnt) {
// return false if channel does not exist in Channels
// Get Channel Name
#if APIVERSNUM < 20301
  const cChannel *channel = Channels.GetByChannelID(channelid);
#else
  LOCK_CHANNELS_READ;
  const cChannel *channel = Channels->GetByChannelID(channelid);
#endif
  if (!channel) {
    msg_cnt++;
    if (msg_cnt < 5) dsyslog("tvscraper: Channel %s is not availible, skipping. Most likely this channel does not exist. To get rid of this message, goto tvscraper settings and edit the channel list.", (const char *)channelid.ToString());
    if (msg_cnt == 5) dsyslog("tvscraper: Skipping further messages: Channel %s is not availible, skipping. Most likely this channel does not exist. To get rid of this message, goto tvscraper settings and edit the channel list.", (const char *)channelid.ToString());
    return false;
  }
  channelName = channel->Name();
  return true;
}
vector<tEventID> GetEventIDs(const tChannelID &channelid, const std::string &channelName) {
// Return a list of event IDs (in vector<tEventID> eventIDs)
vector<tEventID> eventIDs;
  time_t now = time(0);
// get and lock the schedule
#if APIVERSNUM < 20301
  cSchedulesLock schedulesLock;
  const cSchedules *Schedules = cSchedules::Schedules(schedulesLock);
#else
  LOCK_SCHEDULES_READ;
#endif
  const cSchedule *Schedule = Schedules->GetSchedule(channelid);
  if (Schedule) {
    for (const cEvent *event = Schedule->Events()->First(); event; event = Schedule->Events()->Next(event))
      if (event->EndTime() >= now) eventIDs.push_back(event->EventID());
  } else dsyslog("tvscraper: Schedule for channel %s %s is not availible, skipping", channelName.c_str(), (const char *)channelid.ToString() );
  return eventIDs;
}

bool cTVScraperWorker::ScrapEPG(void) {
// true if one or more new events were scraped
  bool newEvent = false;
  if (config.GetReadOnlyClient() ) return newEvent;
// check for changes in schedule (works only if APIVERSNUM >= 20301
#if APIVERSNUM >= 20301
  if (!cSchedules::GetSchedulesRead(schedulesStateKey)) {
    if (config.enableDebug) dsyslog("tvscraper: Schedule was not changed, skipping scan");
    return newEvent;
  }
  schedulesStateKey.Remove();
#endif
// loop over all channels configured for scraping, and create map to enable check for new events
// note: as a result, elso information from external EPG providers is only collected for these channels

// to provide statistics:
  std::chrono::duration<double> timeNeededExt(0);
  std::chrono::duration<double> timeNeededDlMM(0);
  std::chrono::duration<double> timeNeededDlMS(0);
  std::chrono::duration<double> timeNeededDlTS(0);
  std::chrono::duration<double> timeNeededOthers(0);
  std::chrono::duration<double> timeNeededDl(0);
  int num0  = 0;
  int num1  = 0;
  int num11 = 0;
  int movieOrTvIdMaxTime = 0;
  std::chrono::duration<double> time0(0);
  std::chrono::duration<double> time1(0);
  std::chrono::duration<double> time11(0);
  std::chrono::duration<double> time0_max(0);
  std::chrono::duration<double> time1_max(0);
  std::chrono::duration<double> time11_max(0);
  moviedbScraper->apiCalls.reset();
  tvdbScraper->apiCalls.reset();
  config.timeDownloadMedia.reset();
  config.timeDownloadActorsMovie.reset();

  db->m_cache_time.reset();
  db->m_cache_episode_search_time.reset();
  db->m_cache_update_episode_time.reset();
  db->m_cache_update_similar_time.reset();

  map<tChannelID, set<tEventID>*> currentEvents;
  set<tChannelID> channels;
  vector<cTvMedia> extEpgImages;
  string extEpgImage;
  int msg_cnt = 0;
  int i_channel_num = 0;
  for (const tChannelID &channelID: config.GetScrapeAndEpgChannels() ) { // GetScrapeAndEpgChannels creates a copy, thread save
    if (++i_channel_num > 1000) {
      esyslog("tvscraper: ERROR: don't scrape more than 1000 channels");
      break;
    }
    std::string channelName;
    if (!GetChannelName(channelName, channelID, msg_cnt)) continue;  // channel not found in Channels

    std::shared_ptr<iExtEpgForChannel> extEpg = config.GetExtEpgIf(channelID);
    bool channelActive = config.ChannelActive(channelID);

    map<tChannelID, set<tEventID>*>::iterator currentEventsCurrentChannelIT = currentEvents.find(channelID);
    if (currentEventsCurrentChannelIT == currentEvents.end() ) {
      currentEvents.insert(pair<tChannelID, set<tEventID>*>(channelID, new set<tEventID>));
      currentEventsCurrentChannelIT = currentEvents.find(channelID);
    }
    map<tChannelID, set<tEventID>*>::iterator lastEventsCurrentChannelIT = lastEvents.find(channelID);
    bool newEventSchedule = false;
    vector<tEventID> eventIDs = GetEventIDs(channelID, channelName);
    int network_id = config.Get_TheTVDB_company_ID_from_channel_name(channelName);
    for (const tEventID &eventID: eventIDs) {
      auto begin = std::chrono::high_resolution_clock::now();
      if (!Running()) {
        for (auto &event: currentEvents) delete event.second;
        return newEvent;
      }
// insert this event in the list of scraped events
      currentEventsCurrentChannelIT->second->insert(eventID);
// check: was this event scraped already?
      if (lastEventsCurrentChannelIT == lastEvents.end() ||
          lastEventsCurrentChannelIT->second->find(eventID) == lastEventsCurrentChannelIT->second->end() ) {
// this event was not yet scraped, scrape it now
//  vector<cTvMedia> extEpgImages;
        extEpgImages.clear();
        extEpgImage.clear();
        cMovieOrTv *movieOrTv = NULL;
        int statistics;
        std::chrono::duration<double> timeNeeded;
        const cEvent *event = getEvent(eventID, channelID);
        if (!event || !event->StartTime() || !event->EventID() ) continue;
        cStaticEvent sEvent(event); // event will be valid for 5 seconds. We might need longer, so we copy relevant event information to sEvent (where we control the lifetime, and no VDR locks are required
        timeNeededOthers += std::chrono::high_resolution_clock::now() - begin;
        if (extEpg) {
          auto begin = std::chrono::high_resolution_clock::now();
          extEpg->enhanceEvent(&sEvent, extEpgImages);
          if (!extEpgImages.empty() ) extEpgImage = getEpgImagePath(&sEvent, true);
          timeNeededExt += std::chrono::high_resolution_clock::now() - begin;
        }
        if (!channelActive) continue;
        newEvent = true;
        if (!newEventSchedule) {
          isyslog("tvscraper: scraping Channel %s %s TheTVDB company ID %d", channelName.c_str(), cToSvConcat(channelID).c_str(), network_id);
          newEventSchedule = true;
        }
        csStaticEvent sEoR(&sEvent);
        cSearchEventOrRec SearchEventOrRec(&sEoR, overrides, m_movieDbMovieScraper, m_movieDbTvScraper, m_tvDbTvScraper, db, channelName);
        auto begin_scrape = std::chrono::high_resolution_clock::now();
        movieOrTv = SearchEventOrRec.Scrape(statistics);
        timeNeeded = std::chrono::high_resolution_clock::now() - begin_scrape;
        if (timeNeeded > std::max(std::max(time0_max, time1_max), time11_max)) movieOrTvIdMaxTime = movieOrTv?movieOrTv->dbID():0;
        switch (statistics) {
          case 0:
            ++num0;
            time0 += timeNeeded;
            time0_max = std::max(timeNeeded, time0_max);
            break;
          case 1:
            ++num1;
            time1 += timeNeeded;
            time1_max = std::max(timeNeeded, time1_max);
            break;
          case 11:
            ++num11;
            time11 += timeNeeded;
            time11_max = std::max(timeNeeded, time11_max);
            break;
        }
        waitCondition.TimedWait(mutex, 10); // short wait time after scraping an event
        auto begin = std::chrono::high_resolution_clock::now();
        if (!extEpgImages.empty() ) {
          DownloadImg(extEpgImages[0].path, extEpgImage);
        }
        timeNeededDl += std::chrono::high_resolution_clock::now() - begin;
        if (movieOrTv) {
          begin = std::chrono::high_resolution_clock::now();
          movieOrTv->DownloadImages(m_movieDbMovieScraper, m_movieDbTvScraper, m_tvDbTvScraper, "");
          if (movieOrTv->getType() == tMovie)
            timeNeededDlMM += std::chrono::high_resolution_clock::now() - begin;
          else if (movieOrTv->dbID() > 0)
            timeNeededDlMS += std::chrono::high_resolution_clock::now() - begin;
          else
            timeNeededDlTS += std::chrono::high_resolution_clock::now() - begin;
          delete movieOrTv;
          movieOrTv = NULL;
        }
        if (!Running() ) {
          for (auto &event: currentEvents) delete event.second;
          return newEvent;
        }
        bool newRec = CheckRunningTimers();
        if (newRec) backup_requested = true;
        if (backup_requested) {
          int rc = db->BackupToDisc();
          if (rc == SQLITE_OK) backup_requested = false;
          if (!Running() ) {
            for (auto &event: currentEvents) delete event.second;
            return newEvent;
          }
        }
      }
    }  // end loop over all events
  } // end loop over all channels
  currentEvents.swap(lastEvents);
  for (auto &event: currentEvents) delete event.second;
  if (num0+num1+num11 > 0) {
// write statistics
    isyslog("tvscraper: statistics, over all, num = %5i, time = %9.5f, average %f, max = %f, movIdMax %i", num0+num1+num11, (time0+time1+time11).count(), (time0+time1+time11).count()/(num0+num1+num11), std::max(std::max(time0_max, time1_max), time11_max).count(), movieOrTvIdMaxTime);
    if (config.enableDebug) {
      dsyslog("tvscraper: skip due to override, num = %5i, time = %9.5f, average %f, max = %f", num0, time0.count(), time0.count()/num0, time0_max.count());
      dsyslog("tvscraper: cache hit           , num = %5i, time = %9.5f, average %f, max = %f", num1, time1.count(), time1.count()/num1, time1_max.count());
      dsyslog("tvscraper: external database   , num = %5i, time = %9.5f, average %f, max = %f", num11, time11.count(), time11.count()/num11, time11_max.count());
      moviedbScraper->apiCalls.print("moviedbScraper, api calls");
         tvdbScraper->apiCalls.print("tvdbScraper   , api calls");
      db->m_cache_time.print("select from cache        ");
      db->m_cache_episode_search_time.print("cache_episode_search_time");
      db->m_cache_update_episode_time.print("cache_update_episode_time");
      db->m_cache_update_similar_time.print("cache_update_similar_time");
      dsyslog("tvscraper: time for ext epg, time = %9.5f", timeNeededExt.count() );
      dsyslog("tvscraper: timeNeededOthers, time = %9.5f", timeNeededOthers.count() );
      dsyslog("tvscraper: timeNeededDl,     time = %9.5f", timeNeededDl.count() );
      dsyslog("tvscraper: dl TMDb M       , time = %9.5f", timeNeededDlMM.count() );
      dsyslog("tvscraper: dl TMDb S       , time = %9.5f", timeNeededDlMS.count() );
      dsyslog("tvscraper: dl TheTVDB S    , time = %9.5f", timeNeededDlTS.count() );
      config.timeDownloadActorsMovie.print("timeDownloadActorsMovie");
      config.timeDownloadMedia.print      ("timeDownloadMediaMovie ");
    }
  }
  return newEvent;
}

void cTVScraperWorker::ScrapRecordings(void) {
  if (config.GetReadOnlyClient() ) return;
  if (m_recording.empty() ) db->ClearRecordings2();
  else  {
    LOCK_RECORDINGS_READ;
    const cRecording *rec = Recordings->GetByName(m_recording.c_str() );
    if (!rec) {
      esyslog("tvscraper: recording %s does not exist, skip scan", m_recording.c_str());
      return;
    }
    const cRecordingInfo *recInfo = rec->Info();
    if (!recInfo || !recInfo->GetEvent() ) {
      esyslog("tvscraper: recording %s does exist, but no recInfo or no GetEvent. skip scan", m_recording.c_str());
      return;
    }
    csRecording sRecording(rec);
    db->DeleteEventOrRec(&sRecording);
  }

  cMovieOrTv *movieOrTv = NULL;
  vector<string> recordingFileNames;
  if (m_recording.empty() ) {
    {
      LOCK_RECORDINGS_READ;
      for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec)) {
        if (overrides->IgnorePath(rec->FileName())) continue;
        if (rec->FileName()) recordingFileNames.push_back(rec->FileName());
      }
    }
  }
  else recordingFileNames.push_back(m_recording);
  for (const string &filename: recordingFileNames) {
    {
      LOCK_RECORDINGS_READ;
      const cRecording *rec = Recordings->GetByName(filename.c_str() );
      if (!rec) continue;
      if (config.enableDebug) esyslog("tvscraper: Scrap recording \"%s\"", rec->FileName() );

      const cRecordingInfo *recInfo = rec->Info();
      if (recInfo && recInfo->GetEvent() ) {
        csRecording sRecording(rec);
        cSearchEventOrRec SearchEventOrRec(&sRecording, overrides, m_movieDbMovieScraper, m_movieDbTvScraper, m_tvDbTvScraper, db, recInfo->ChannelName() );
        int statistics;
        movieOrTv = SearchEventOrRec.Scrape(statistics);
      }
    }
// here, the read lock is released, so wait a short time, in case someone needs a write lock
    waitCondition.TimedWait(mutex, 100);
    if (movieOrTv) {
      movieOrTv->DownloadImages(m_movieDbMovieScraper, m_movieDbTvScraper, m_tvDbTvScraper, filename);
      delete movieOrTv;
      movieOrTv = NULL;
    }
    if (!Running() ) break;
    bool newRec = CheckRunningTimers();
    if (newRec) backup_requested = true;
    if (backup_requested) {
      int rc = db->BackupToDisc();
      if (rc == SQLITE_OK) backup_requested = false;
    }
    if (!Running() ) break;
  }
  deleteOutdatedRecordingImages(db);
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
    if (timer->HasFlags(tfActive) && (difftime(timer->StartTime(), time(0) )*60 < nextMinutes)) return true;
  }
  return false;
}

void writeTimerInfo(const cTimer *timer, const char *pathName) {
  cToSvConcat filename(pathName, "/tvscraper.json");

  cJsonDocumentFromFile document(filename);
  if (document.HasParseError() ) return;
  rapidjson::Value::MemberIterator ji_timer = document.FindMember("timer");
  if (ji_timer != document.MemberEnd() ) {
    rapidjson::Value::MemberIterator ji_stop_time = ji_timer->value.FindMember("stop_time");
    if (ji_stop_time == ji_timer->value.MemberEnd() ) {
      esyslog("tvscraper, ERROR in worker.c, writeTimerInfo: \"stop_time\" missing in tvscraper.json");
      ji_timer->value.AddMember("stop_time", rapidjson::Value().SetInt64(timer->StopTime()), document.GetAllocator());
    } else {
      if (ji_stop_time->value.GetInt64() == timer->StopTime() ) return;
      ji_stop_time->value.SetInt64(timer->StopTime());
//      if (ji_stop_time != ji_timer->value.MemberEnd() ) ji_timer->value.RemoveMember(ji_stop_time);
    }
  } else {
    rapidjson::Value timer_j;
    timer_j.SetObject();

    timer_j.AddMember("vps", rapidjson::Value().SetBool(timer->HasFlags(tfVps)), document.GetAllocator());
    timer_j.AddMember("start_time", rapidjson::Value().SetInt64(timer->StartTime()), document.GetAllocator());
    timer_j.AddMember("stop_time", rapidjson::Value().SetInt64(timer->StopTime()), document.GetAllocator());

    document.AddMember("timer", timer_j, document.GetAllocator());
  }
  jsonWriteFile(document, filename);
}

bool cTVScraperWorker::CheckRunningTimers(void) {
// assign scrape result from EPG to recording
// return true if new data are assigned to one or more recordings
  if (config.GetReadOnlyClient() ) return false;
  if (lastTimerRecordingCheck + 2 * 60 > time(0) ) return false;  // no need to check more often
  lastTimerRecordingCheck = time(0);
  bool newRecData = false;
  std::vector<std::string> recordingFileNames; // filenames of recordings with running timers
  struct stat buffer;
  { // in this block, we lock the timers
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
      if (stat(cToSvConcat(rc->FileName(), "/tvscraper.json").c_str(), &buffer) != 0) {
        recordingFileNames.push_back(rc->FileName() );
      }
      writeTimerInfo(timer, rc->FileName() );
    }
  } // timer lock is released
  for (const string &filename: recordingFileNames) {
// loop over all recordings with running timers
    cMovieOrTv *movieOrTv = NULL;
    std::string epgImagePath;
    std::string recordingImagePath;
    { // in this block we have recording locks
#if APIVERSNUM < 20301
      const cRecording *recording = Recordings.GetByName(filename.c_str() );
#else
      LOCK_RECORDINGS_READ;
      const cRecording *recording = Recordings->GetByName(filename.c_str() );
#endif
      if (!recording) {
        esyslog("tvscraper: ERROR cTVScraperWorker::CheckRunningTimers: no recording for file \"%s\"", filename.c_str() );
        continue;
      }
      if (!recording->Info() ) {
        esyslog("tvscraper: ERROR cTVScraperWorker::CheckRunningTimers: no recording->Info for file \"%s\"", filename.c_str() );
        continue;
      }
      const cEvent *event = recording->Info()->GetEvent();
      epgImagePath = event?getExistingEpgImagePath(event->EventID(), event->StartTime(), recording->Info()->ChannelID()):"";
      recordingImagePath = getRecordingImagePath(recording);

      csRecording sRecording(recording);
      int r = db->SetRecording(&sRecording);
      if (r == 2) {
        newRecData = true;
        movieOrTv = cMovieOrTv::getMovieOrTv(db, recording);
        if (movieOrTv) {
          movieOrTv->copyImagesToRecordingFolder(recording->FileName() );
          delete movieOrTv;
          movieOrTv = NULL;
        }
      }
      if (r == 0) {
        tEventID eventID = sRecording.EventID();
        std::string channelIDs = sRecording.ChannelIDs();
        esyslog("tvscraper: cTVScraperWorker::CheckRunningTimers: no entry in table event found for eventID %i, channelIDs %s, recording for file \"%s\"", (int)eventID, channelIDs.c_str(), filename.c_str() );
        if (ConnectScrapers() ) { 
          cSearchEventOrRec SearchEventOrRec(&sRecording, overrides, m_movieDbMovieScraper, m_movieDbTvScraper, m_tvDbTvScraper, db, recording->Info()->ChannelName() );
          int statistics;
          movieOrTv = SearchEventOrRec.Scrape(statistics);
          if (movieOrTv) newRecData = true;
        }
      }
    } // the locks are released
    waitCondition.TimedWait(mutex, 1);  // allow others to get the locks
    if (movieOrTv) {
      movieOrTv->DownloadImages(m_movieDbMovieScraper, m_movieDbTvScraper, m_tvDbTvScraper, filename);
      delete movieOrTv;
      movieOrTv = NULL;
    }
    std::string fanartImg = concat(filename, "/fanart.jpg");
    if (!FileExistsImg(fanartImg) && !epgImagePath.empty() ) {
//    esyslog("tvscraper, CopyFile %s, %s", epgImagePath.c_str(), fanartImg.c_str() );
      CopyFileImg(epgImagePath, fanartImg);
//    esyslog("tvscraper, CopyFile %s, %s", epgImagePath.c_str(), recordingImagePath.c_str() );
      CopyFileImg(epgImagePath, recordingImagePath);
      if (!newRecData) TouchFile(config.GetRecordingsUpdateFileName().c_str());
    }
  }
  if (newRecData && !recordingFileNames.empty() ) TouchFile(config.GetRecordingsUpdateFileName().c_str());
  return newRecData;
}

bool cTVScraperWorker::StartScrapping(bool &fullScan) {
  fullScan = false;
//   if (!manualScan && TimersRunningPlanned(15.) ) return false;  // we avoided scans during recordings. Seems not to be required, and such scans have advantages (create timers for collectios ..., )
  bool resetScrapeTime = false;
  if (manualScan) {
    manualScan = false;
    for (auto &event: lastEvents) event.second->clear();
    resetScrapeTime = true;
  }
  if (resetScrapeTime || lastEvents.empty() ) {
// a full scrape will be done, write this to db, so next full scrape will be in one day
    db->exec("delete from scrap_checker");
    const char *sql = "INSERT INTO scrap_checker (last_scrapped) VALUES (?)";
    db->exec(sql, time(0));
    fullScan = true;
    return true;
  }
// Delete the scraped event IDs once a day, to update data
  int minTime = 24 * 60 * 60;
  if (db->CheckStartScrapping(minTime)) {
    for (auto &event: lastEvents) event.second->clear();
    fullScan = true;
  }
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
      dsyslog("tvscraper: scanning video dir");
      if (ConnectScrapers()) {
        ScrapRecordings();
        scanVideoDir = false;
        m_recording.clear();
        dsyslog("tvscraper: touch \"%s\"", config.GetRecordingsUpdateFileName().c_str());
        TouchFile(config.GetRecordingsUpdateFileName().c_str());
      }
      dsyslog("tvscraper: scanning video dir done");
      continue;
    }
    bool newRec = CheckRunningTimers();
    if (newRec) backup_requested = true;
    if (!Running() ) break;
    bool fullScan = false;
    if (StartScrapping(fullScan)) {
      dsyslog("tvscraper: start scraping epg");
      if (ConnectScrapers()) {
        bool newEvents = ScrapEPG();
        if (newEvents) TouchFile(config.GetEPG_UpdateFileName().c_str());
        if (newEvents && Running() ) cMovieOrTv::DeleteAllIfUnused(db);
        if (fullScan && Running()) backup_requested = true;
      }
      dsyslog("tvscraper: epg scraping done");
      if (!Running() ) break;
      if (config.getEnableAutoTimers() ) timersForEvents(*db);
    }
    if (backup_requested) {
      int rc = db->BackupToDisc();
      if (rc == SQLITE_OK) backup_requested = false;
    }
    waitCondition.TimedWait(mutex, loopSleep);
  }
}

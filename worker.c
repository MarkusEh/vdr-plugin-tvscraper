#include <locale.h>
#include <vdr/menu.h>
#include "worker.h"

using namespace std;

cTVScraperWorker::cTVScraperWorker(cTVScraperDB *db, cOverRides *overrides):
  cThread("tvscraper", true),
  m_epgImageDownloadSleep(5),  // 5 seconds
  m_epgImageDownloadIterations(5*12),  // result in main loop sleep of 5 mins (5*12*m_epgImageDownloadSleep)
  m_epg_images(&m_curl, m_epgImageDownloadSleep) {
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
//  loopSleep = 5 * 60 * 1000;
}

cTVScraperWorker::~cTVScraperWorker() {
    if (m_movieDbMovieScraper) delete m_movieDbMovieScraper;
    if (m_movieDbTvScraper) delete m_movieDbTvScraper;
    if (m_tvDbTvScraper) delete m_tvDbTvScraper;
    if (moviedbScraper) delete moviedbScraper;
    if (tvdbScraper) delete tvdbScraper;
}

void cTVScraperWorker::Stop(void) {
    Cancel(210);                  // wait up to 210 seconds for thread was stopping
}
bool cTVScraperWorker::waitMs(int ms)	{
// return Running()
  if (!Running() ) return false;
  cTimeMs SleepTime(ms);
  while (!SleepTime.TimedOut()) {
    cCondWait::SleepMs(100);
    if (!Running() ) return false;
  }
  return true;
}

void cTVScraperWorker::InitVideoDirScan(const char *filename) {
  if (config.GetReadOnlyClient() ) return;
  if (filename && *filename) {
    LOCK_RECORDINGS_READ;
    const cRecording *recording = Recordings->GetByName(filename);
    if (!recording) {
      esyslog("tvscraper: ERROR cannot start scraping \"%s\", recording not found", filename);
      return;
    }
    isyslog("tvscraper: start scraping \"%s\"", filename);
    const cEvent *event = recording->Info()->GetEvent();
    cToSvConcat channelIDs;
    if ((event->EventID() != 0) & recording->Info()->ChannelID().Valid() ) channelIDs.concat(recording->Info()->ChannelID());
    else channelIDs.concat(recording->Name());
    db->exec("DELETE FROM recordings2 WHERE event_id = ? and event_start_time = ? AND (recording_start_time is NULL OR recording_start_time = ?) and channel_id = ?",
      event->EventID(), event->StartTime()?event->StartTime():recording->Start(), recording->Start(), channelIDs);
  } else {
    isyslog("tvscraper: start scraping all recordings");
    db->exec("DELETE FROM recordings2");
  }
}

void cTVScraperWorker::InitManualScan(void) {
  manualScan = true;
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
    moviedbScraper = new cMovieDBScraper(&m_curl, db, overrides);
    if (!moviedbScraper->Connect()) {
      esyslog("tvscraper: ERROR, connection to TheMovieDB failed");
      delete moviedbScraper;
      moviedbScraper = NULL;
      return false;
    }
  }
  if (!tvdbScraper) {
    tvdbScraper = new cTVDBScraper(&m_curl, db);
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
  LOCK_CHANNELS_READ;
  const cChannel *channel = Channels->GetByChannelID(channelid);
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
  LOCK_SCHEDULES_READ;
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
  cEpgImagePath extEpgImage;
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
        cStaticEvent sEvent;
        {
//        event will be valid for 5 seconds. We might need longer, so we copy relevant event information to sEvent (where we control the lifetime, and no VDR locks are required
          LOCK_SCHEDULES_READ;
          const cEvent *event = getEvent(eventID, channelID, Schedules);
          if (!event || !event->StartTime() || !event->EventID() ) continue;
          sEvent.set_from_event(event);
        }
        timeNeededOthers += std::chrono::high_resolution_clock::now() - begin;
        if (extEpg) {
          auto begin = std::chrono::high_resolution_clock::now();
          extEpg->enhanceEvent(&sEvent, extEpgImages);
          if (!extEpgImages.empty() ) {
            extEpgImage.set(&sEvent, true);
//          dsyslog3("extEpgImages[0].path = ", extEpgImages[0].path, " extEpgImage = ", extEpgImage, " .");
            if (extEpgImages[0].path[0] != '/') m_epg_images.add(sEvent.StartTime(), extEpgImages[0].path, extEpgImage);
          }
          timeNeededExt += std::chrono::high_resolution_clock::now() - begin;
          m_epg_images.downloadOne();
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
        if (config.m_writeEpisodeToEpg) {
          if (movieOrTv && movieOrTv->getType() == tSeries && movieOrTv->getEpisode() != 0) {
            std::string title, episodeName;
            if (movieOrTv->getOverview(&title, &episodeName, nullptr, nullptr, nullptr)) {
              cToSvConcat description(sEoR.Description() );
              description.concat("\n", config.m_description_delimiter, " ", remove_trailing_whitespace(title));
              description.concat("\n", tr("Episode Name:"), " ", remove_trailing_whitespace(episodeName));
              description.concat("\n", tr("Season Number:"), " ", movieOrTv->getSeason() );
              description.concat("\n", tr("Episode Number:"), " ", movieOrTv->getEpisode() );
              sEvent.SetDescription(description.c_str() );
            }
          } else {
            std::cmatch capture_groups;
            for (const std::regex &r: overrides->m_regexDescription_titleEpisodeSeasonNumberEpisodeNumber) {
              if (std::regex_match(sEoR.Description().data(), sEoR.Description().data()+sEoR.Description().length(), capture_groups, r) && capture_groups.size() == 5) {
                cToSvConcat description(sEoR.Description() );
                description.concat("\n", config.m_description_delimiter, " ", remove_trailing_whitespace(capture_groups[1].str()));
                description.concat("\n", tr("Episode Name:"), " ", remove_trailing_whitespace(capture_groups[2].str()));
                description.concat("\n", tr("Season Number:"), " ", remove_trailing_whitespace(capture_groups[3].str()));
                description.concat("\n", tr("Episode Number:"), " ", remove_trailing_whitespace(capture_groups[4].str()));
                sEvent.SetDescription(description.c_str() );
                break;
              }
            }
          }
        }
        waitMs(10); // short wait time after scraping an event
        auto begin = std::chrono::high_resolution_clock::now();
        if (!extEpgImages.empty() && !extEpgImages[0].path.empty() ) {
          if (extEpgImages[0].path[0] == '/') CopyFileImg(extEpgImages[0].path, extEpgImage);
//        else DownloadImg(&m_curl, extEpgImages[0].path, extEpgImage);
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
        ScrapChangedRecordings();
        if (!Running() ) {
          for (auto &event: currentEvents) delete event.second;
          return newEvent;
        }
      }
    }  // end loop over all events
    m_epg_images.downloadOne();
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

void cTVScraperWorker::ScrapChangedRecordings() {
// scrape new recordings (not yet scraped)
// and clear runtime & duration deviation for changed recordings (length_in_seconds missmatch)
  if (lastScrapeRecordings + 60 > time(NULL)) return;  // not more often than every 1 mins
  lastScrapeRecordings = time(NULL);
  std::vector<std::string> recordingFileNames; // recordings we have to scrape
  {
    LOCK_RECORDINGS_READ;
    for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec)) {
      if (!rec->FileName() || overrides->IgnorePath(rec->FileName())) continue;
// get currently found movie/series
      int season_number;
      int lengthInSeconds = db->GetLengthInSeconds(rec, season_number);
// -3 -> no entry in db   -> scrape
// -2 -> never checked / assigned
// -1 -> not known by VDR
      if (lengthInSeconds == -3 || season_number == -102) recordingFileNames.push_back(rec->FileName());
      else if (lengthInSeconds != rec->LengthInSeconds()) {
        db->ClearRuntimeDurationDeviation(rec);
// we update lengthInSeconds in db directly in ClearRuntimeDurationDeviation
// Note: Might not be required. We could check this before DurationDeviation is used (?)
      }
    }
  }
  ScrapRecordings(recordingFileNames);
}

void cTVScraperWorker::ScrapRecordings(const std::vector<std::string> &recordingFileNames) {
  CheckRunningTimers();
  if (!Running() ) return;
  bool new_assignment = false;
  for (const std::string &filename: recordingFileNames) {
    cMovieOrTv *movieOrTv = nullptr;
    bool movieOrTv_fromEpg = false;
    cEpgImagePath epgImagePath;
    std::string recordingImagePath;
    {
      LOCK_RECORDINGS_READ;
      const cRecording *recording = Recordings->GetByName(filename.c_str() );
      if (!recording) continue;  // there was a change since we got the filename -> ignore
      recordingImagePath = getRecordingImagePath(recording);

      if ((recording->IsInUse() & ruTimer) != 0) {
// this recording is currently used by a timer
// -> copy EPG information
        const cEvent *event = recording->Info()->GetEvent();  // this event is locked with the recording
        epgImagePath.set(event->EventID(), event->StartTime(), recording->Info()->ChannelID(), false);
// note: getExistingEpgImagePath allways return an non-empty path
        if (db->SetRecording(recording)  != 0) {
// return 2 if the movieTv assigned to the event was not yet assigned to the recording, but it is done now
          movieOrTv = cMovieOrTv::getMovieOrTv(db, recording);
          if (movieOrTv) movieOrTv_fromEpg = true;
        }
      }

      if (!movieOrTv) movieOrTv = ScrapRecording(recording);

      if (movieOrTv) {
        db->InsertRecording(recording, movieOrTv->dbID(), movieOrTv->getSeason(), movieOrTv->getEpisode() );
        new_assignment = true;
      } else {
        db->DeleteRecording(recording);
      }
    }  // end lock on recording
    if (movieOrTv) {
      if (movieOrTv_fromEpg) movieOrTv->copyImagesToRecordingFolder(filename);
      else                   movieOrTv->DownloadImages(m_movieDbMovieScraper, m_movieDbTvScraper, m_tvDbTvScraper, filename);

      delete movieOrTv;
      movieOrTv = nullptr;
    }
    if (!epgImagePath.empty() && !FileExistsImg(recordingImagePath) ) {
// copy img from EPG to img for this recording
      CopyFileImg(epgImagePath, recordingImagePath);
      if (!new_assignment) TouchFile(config.GetRecordingsUpdateFileName().c_str());  // tvscraper will now find this image
    }
    cToSvConcat fanartImg(filename, "/fanart.jpg");
    if (!FileExistsImg(fanartImg) && FileExistsImg(recordingImagePath)) {
// for new timer (from EPG), or for cut / copy / moved recording
// note: recordingImagePath depends only on event data (StartTime, ChannelID and EventID)
//       so, this will not change for cut / copy or move operations
      CopyFileImg(recordingImagePath, fanartImg);
    }
/* we create tvscraper.json in CheckRunningTimers */

// here, the read lock is released, so wait a short time, in case someone needs a write lock
    if (!waitMs(100)) break;
  }
  if (new_assignment) {
    db->BackupToDisc();
    TouchFile(config.GetRecordingsUpdateFileName().c_str());
  }
  if (!Running() ) return;
  deleteOutdatedRecordingImages(db);
}

cMovieOrTv *cTVScraperWorker::ScrapRecording(const cRecording *recording) {
// LOCK_RECORDINGS_READ required before calling
// do not download pictures -> must be done after this method
// use event duration for the scraping
  const cRecordingInfo *recInfo = recording->Info();
  if (!recInfo || !recInfo->GetEvent() || !recording->FileName() ) return nullptr;  // should not happen
  int statistics;
  timespec last_change = modification_time(recording->FileName());
  if (last_change.tv_sec != 0 && last_change.tv_sec + 20*60 < time(NULL) ) {
// last change of recording was 20 mins ago -> we assume any recording, copy or move operation is finished
    if (config.enableDebug) esyslog("tvscraper: Scrap recording \"%s\" (use rec length)", recording->FileName() );
    csRecording sRecording(recording);
    cSearchEventOrRec SearchEventOrRec(&sRecording, overrides, m_movieDbMovieScraper, m_movieDbTvScraper, m_tvDbTvScraper, db, recInfo->ChannelName() );
    return SearchEventOrRec.Scrape(statistics);
  } else {
// recording is still changeing -> use event information (especially duration) for scraping
    if (config.enableDebug) esyslog("tvscraper: Scrap recording \"%s\" (use event length)", recording->FileName() );
    cStaticEvent staticEvent;
    staticEvent.set_from_recording(recording);
    csStaticEvent eventOrRecording(&staticEvent);
    cSearchEventOrRec SearchEventOrRec(&eventOrRecording, overrides, m_movieDbMovieScraper, m_movieDbTvScraper, m_tvDbTvScraper, db, recInfo->ChannelName() );
    return SearchEventOrRec.Scrape(statistics);
  }
}


void cTVScraperWorker::CheckRunningTimers() {
// we update the stop time in tvscraper.json in case of changes in the timer stop time
  if (config.GetReadOnlyClient() ) return;

  LOCK_TIMERS_READ;
  for (const cTimer *timer = Timers->First(); timer; timer = Timers->Next(timer)) if (timer->Local() )
  if (timer->Recording()) {
// figure out recording
    cRecordControl *rc = cRecordControls::GetRecordControl(timer);
    if (!rc) {
      esyslog("tvscraper: ERROR cTVScraperWorker::CheckRunningTimers: Timer is recording, but there is no cRecordControls::GetRecordControl(timer)");
      continue;
    }
    writeTimerInfo(timer, rc->FileName() );
  }
}

bool cTVScraperWorker::StartScrapping(bool &fullScan) {
  fullScan = false;
  bool resetScrapeTime = false;
  if (manualScan) {
    manualScan = false;
    for (auto &event: lastEvents) event.second->clear();
    resetScrapeTime = true;
  }
  if (resetScrapeTime || lastEvents.empty() ) {
// a full scrape will be done, write this to db, so next full scrape will be in one day
    db->exec("DELETE FROM scrap_checker");
    db->exec("INSERT INTO scrap_checker (last_scrapped) VALUES (?)", time(0));
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

  dsyslog("tvscraper: waiting %d minutes to start main loop", initSleep / 1000 / 60);
  waitMs(initSleep);

  while (Running()) {
    if (ConnectScrapers()) ScrapChangedRecordings();
    if (!Running() ) break;
//  dsyslog("tvscraper: scanning video dir done");

    bool fullScan = false;
    if (StartScrapping(fullScan)) {
//    dsyslog("tvscraper: start scraping epg");
      if (ConnectScrapers()) {
        bool newEvents = ScrapEPG();
        if (newEvents) TouchFile(config.GetEPG_UpdateFileName().c_str());
        if (newEvents && Running() ) cMovieOrTv::DeleteAllIfUnused(db);
      }
      if (!Running() ) break;
//    dsyslog("tvscraper: epg scraping done");
      if (config.getEnableAutoTimers() ) {
        if (config.enableDebug) dsyslog2("start auto timers");
        timersForEvents(*db, this);
        if (config.enableDebug) dsyslog2("auto timers finished");
      }
    }
    if (!Running() ) break;
    for (int i = 0; i < m_epgImageDownloadIterations; ++i) {
      m_epg_images.downloadOne();
      if (!waitMs(m_epgImageDownloadSleep*1000)) break;
    }
  }
}
int cEpgImage::download(cCurl *curl) {
  if (DownloadImg(curl, m_url, m_local_path)) return true;
  return ++m_failed_downloads;
}

void cEpgImages::downloadOne() {
  if (time(NULL) < m_last_download + m_epgImageDownloadSleep) return; // only download one in 15s

  while (!m_epg_images.empty()) {
    if (m_epg_images.back().m_event_start <= time(NULL) ||
        FileExistsImg(m_epg_images.back().m_local_path) ) {
      m_epg_images.pop_back();
    } else {
      int res = m_epg_images.back().download(m_curl);
      if (m_last_report + 5*60 < time(NULL) ) {
// display progress in syslog every 2 mins
        dsyslog2("after download EPG image, res = ", res, " still planned for download: ", m_epg_images.size(), " EPG images");
//    dsyslog2("download \"", m_epg_images.back().m_url, "\" to \"", m_epg_images.back().m_local_path, "\" res = ", res);
          m_last_report = time(NULL);
      }
      if (res == 0) m_epg_images.pop_back();
      if (res > 3) {
        isyslog2("download of ", m_epg_images.back().m_url, " failed");
        m_epg_images.pop_back();
      }
      m_last_download = time(NULL);
      return; // download only one image
    }
  }
}

#include "autoTimers.h"
#include "tvscraperdb.h"

bool operator< (const cScraperRec &rec1, const cScraperRec &rec2) {
  if (rec1.m_movie_tv_id != rec2.m_movie_tv_id) return rec1.m_movie_tv_id < rec2.m_movie_tv_id;
  if (rec1.m_season_number != rec2.m_season_number) return rec1.m_season_number < rec2.m_season_number;
  return rec1.m_episode_number < rec2.m_episode_number;
}

bool operator< (const cEventMovieOrTv &rec1, const cEventMovieOrTv &rec2) {
  if (rec1.m_movie_tv_id != rec2.m_movie_tv_id) return rec1.m_movie_tv_id < rec2.m_movie_tv_id;
  if (rec1.m_season_number != rec2.m_season_number) return rec1.m_season_number < rec2.m_season_number;
  return rec1.m_episode_number < rec2.m_episode_number;
}

bool operator< (const cScraperRec &first, const cEventMovieOrTv &sec) {
  if (first.m_movie_tv_id != sec.m_movie_tv_id) return first.m_movie_tv_id < sec.m_movie_tv_id;
  if (first.m_season_number != sec.m_season_number) return first.m_season_number < sec.m_season_number;
  return first.m_episode_number < sec.m_episode_number;
}
bool operator< (const cEventMovieOrTv &first, const cScraperRec &sec) {
  if (first.m_movie_tv_id != sec.m_movie_tv_id) return first.m_movie_tv_id < sec.m_movie_tv_id;
  if (first.m_season_number != sec.m_season_number) return first.m_season_number < sec.m_season_number;
  return first.m_episode_number < sec.m_episode_number;
}
bool operator< (const cScraperRec &first, int sec) {
  return first.m_movie_tv_id < sec;
}
bool operator< (int first, const cScraperRec &sec) {
  return first < sec.m_movie_tv_id;
}
/*
template <class T, class U>
bool operator< (const T &first, const U &sec) {
  if (first.m_movie_tv_id != sec.m_movie_tv_id) return first.m_movie_tv_id < sec.m_movie_tv_id;
  if (first.m_season_number != sec.m_season_number) return first.m_season_number < sec.m_season_number;
  return first.m_episode_number < sec.m_episode_number;
}
*/



// getEvent ********************************  
const cEvent* getEvent(tEventID eventid, const tChannelID &channelid) {
// note: NULL is returned, if this event is not available
  if ( !channelid.Valid() || eventid == 0 ) return NULL;
  cSchedule const* schedule;
  #if VDRVERSNUM >= 20301
  LOCK_SCHEDULES_READ;
  schedule = Schedules->GetSchedule( channelid );
  #else
  cSchedulesLock schedLock;
  cSchedules const* schedules = cSchedules::Schedules( schedLock );
  schedule = schedules->GetSchedule( channelid );
  #endif
  return schedule->GetEvent( eventid );
}

// getTimer ********************************  
const cTimer* getTimer(tEventID eventid, const tChannelID &channelid) {
  LOCK_TIMERS_READ;
  for (cTimer* timer = (cTimer *)Timers->First(); timer; timer = (cTimer *)Timers->Next(timer)) {
    if (timer->Channel() && timer->Channel()->GetChannelID() == channelid &&
        timer->Event() && timer->Event()->EventID() == eventid) return timer;
  }
  return NULL;
}

// createTimer ********************************  
const cTimer* createTimer(const cEvent *event, const char *fileName = NULL) {
  if (!event) return NULL;
  cTimer *timer = new cTimer(event, fileName);
  timer->ClrFlags(tfRecording);
  timer->SetPriority(10);
  LOCK_TIMERS_WRITE;
  Timers->Add(timer);
  return timer;
}

// getCollections ********************************
bool getCollections(const cTVScraperDB &db, const std::set<cScraperRec, std::less<>> &recordings, std::set<int> &collections) {
  for (const cScraperRec &recording : recordings) if (recording.seasonNumber() == -100) {
    int collection_id = db.GetMovieCollectionID(recording.movieTvId() );
    if (collection_id) collections.insert(collection_id);
  }
  return true;
}

// getRecordings ********************************
bool getRecordings(const cTVScraperDB &db, std::set<cScraperRec, std::less<>> &recordings) {
// return true on sucess, false in case of (temporary) not availability of data
// all recordings where a movie or TV episode was identified are returned.
// In case of duplicates, the better one is returned

  const std::set<std::string> channelsHD = config.GetHD_Channels();
  const std::set<std::string> excludedRecordingFolders = config.GetExcludedRecordingFolders();
  if (channelsHD.size() == 0) return false;
#if APIVERSNUM < 20301
  for (cRecording *rec = Recordings.First(); rec; rec = Recordings.Next(rec))
#else
  LOCK_RECORDINGS_READ;
  for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec))
#endif
  {
    const char *pos_delim = strrchr(rec->Name(), '~');                                                                                                  
    if (pos_delim != 0)
      if (excludedRecordingFolders.find(std::string(rec->Name(), pos_delim - rec->Name() )) != excludedRecordingFolders.end() ) continue;
    csRecording sRecording(rec);
    tEventID eventID = sRecording.EventID();
    time_t eventStartTime = sRecording.StartTime();
    cString channelIDs = sRecording.ChannelIDs();
    int  movie_tv_id, season_number, episode_number;
    const char sql[] = "select movie_tv_id, season_number, episode_number from recordings2 where event_id = ? and event_start_time = ? and channel_id = ?";
    if (!db.QueryLine(sql, "ils", "iii", (int)eventID, (sqlite3_int64)eventStartTime, (const char *)channelIDs, &movie_tv_id, &season_number, &episode_number)) continue;
    if (movie_tv_id == 0) continue;
    if (season_number == 0 && episode_number == 0) continue; // we look only for recordings we can assign to a specific episode/movie
    bool hd = channelsHD.find((const char *)channelIDs) != channelsHD.end();
    int numberOfErrors = rec->Info()->Errors();
    cScraperRec scraperRec(eventID, eventStartTime, (const char *)channelIDs, rec->Name(), movie_tv_id, season_number, episode_number, hd, numberOfErrors);
    auto found = recordings.find(scraperRec);
    if (found == recordings.end() ) recordings.insert(std::move(scraperRec)); // not in list -> insert
    else if (scraperRec.isBetter(*found)) {
      recordings.erase(found);
      recordings.insert(std::move(scraperRec));
    }
  }
// don't remove entries without action item, return all (unique) recordings
// during loop over recordings: Check for action items
  return true;
}

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

// getBestEvents ********************************
std::vector<cEventScraper> getBestEvents(const cTVScraperDB &db, const cScraperRec &recording, const std::set<std::string> &HD_Channels) {
  std::vector<cEventScraper> result_sd;
  std::vector<cEventScraper> result_hd;
  const char sql_event[] = "select event_id, channel_id, valid_till from event where movie_tv_id = ? and season_number = ? and episode_number = ?";
  int l_event_id;
  char *l_channel_id;
  sqlite3_int64 l_validTill;
  for (sqlite3_stmt *statement =
    db.QueryPrepare(sql_event, "iii", recording.movieTvId(), recording.seasonNumber(), recording.episodeNumber() );
    db.QueryStep(statement, "isl", &l_event_id, &l_channel_id, &l_validTill);)
  {
    if (HD_Channels.find(l_channel_id) != HD_Channels.end() ) result_hd.push_back(cEventScraper(l_event_id, l_channel_id, l_validTill));
    else if (recording.hd() == 0 && result_hd.size() == 0) result_sd.push_back(cEventScraper(l_event_id, l_channel_id, l_validTill));
  }
  if (result_hd.size() != 0 ) return result_hd;
  return result_sd;
}
bool operator< (const cEventScraper &event1, const cEventScraper &event2) {
  return event1.m_valid_till < event2.m_valid_till;
}
// timerForRecording ********************************
bool timerForRecording(const cTVScraperDB &db, const cScraperRec &recording, const std::set<std::string> &HD_Channels) {
// check:
//   is improvement possible (sd->hd, errors)?
//   if yes: is there an event which can be used?
// return false if no timer is available or created. true, otherwise
  if (!recording.improvemntPossible() ) return false;
  std::vector<cEventScraper> events = getBestEvents(db, recording, HD_Channels);
  if (events.size() == 0) return false;
// check: is there a timer for any of the events?
  for (cEventScraper &event_l: events)
    if (getTimer(event_l.m_event_id, event_l.m_channel_id) ) return true;
  std::sort(events.begin(), events.end() );
  for (cEventScraper &event_l: events) {
    const cEvent *event = getEvent(event_l.m_event_id, event_l.m_channel_id);
    if (!event) continue; // no event, ignore
    if (event->IsRunning(true) ) continue;  // event already started, ignore
    if (event->StartTime() <= time(0) ) continue;  // IsRunning does sometimes not work
// find folder for timer
    size_t pos = recording.name().find_last_of('~');
    if (pos == std::string::npos || pos == 0) {
      createTimer(event);
      return true;
    }
    if (recording.seasonNumber() == -100) {
      createTimer(event, (recording.name().substr(0, pos + 1) + event->Title()).c_str() );
      return true;
    }
// TV show. Test: Title~ShortText ?
    size_t pos2 = recording.name().find_last_of('~', pos - 1);
    if (pos2 == std::string::npos) pos2 = 0;
    else pos2++;
    if (recording.name().substr(pos2, pos-pos2).compare(event->Title() ) == 0) {
      if (event->ShortText() )
        createTimer(event, (recording.name().substr(0, pos + 1) + event->ShortText()).c_str() );
      else if (event->Description() ) {
        if (strlen(event->Description()) < 50)
          createTimer(event, (recording.name().substr(0, pos + 1) + event->Description()).c_str() );
        else
          createTimer(event, (recording.name().substr(0, pos + 1) + std::string(event->Description(), 50)).c_str() );
      }
      else createTimer(event, (recording.name().substr(0, pos + 1) + event->Title()).c_str() );
      return true;
    }
    createTimer(event, (recording.name().substr(0, pos + 1) + event->Title()).c_str() );
    return true;
  }
  return false;
}
// timersForRecordings ********************************
bool timersForRecordings(const cTVScraperDB &db) {
// for each recording, check:
//   is improvement possible (sd->hd, errors)?
//   if yes: is there an event which can be used?
  std::set<cScraperRec, std::less<>> recordings;
  if (!getRecordings(db, recordings) ) return false;
  std::set<std::string> HD_Channels = getDefaultHD_Channels();
  if (HD_Channels.size() == 0) return false;

  for (const cScraperRec &recording: recordings)
    timerForRecording(db, recording, HD_Channels);
  return true;
}

// getAllEvents ********************************
std::set<cEventMovieOrTv> getAllEvents(const cTVScraperDB &db) {
  std::set<cEventMovieOrTv> result;
  const time_t now_m10 = time(0) - 10*60;
  const char sql_event[] = "select event_id, channel_id, valid_till, movie_tv_id, season_number, episode_number from event";
  cEventMovieOrTv scraperEvent;
  int l_event_id;
  char *l_channel_id;
  sqlite3_int64 l_validTill;
  for (sqlite3_stmt *statement = db.QueryPrepare(sql_event, "");
    db.QueryStep(statement, "isliii", &l_event_id, &l_channel_id, &l_validTill, &scraperEvent.m_movie_tv_id, &scraperEvent.m_season_number, &scraperEvent.m_episode_number);)
  {
    scraperEvent.m_event = getEvent(l_event_id, tChannelID::FromString(l_channel_id));
    if (!scraperEvent.m_event) continue;
    if (scraperEvent.m_event->IsRunning(true) ) continue;  // event already started, ignore
    if (scraperEvent.m_event->StartTime() <= now_m10 ) continue;  // IsRunning does sometimes not work
    scraperEvent.m_hd = config.GetHD_Channels().find(l_channel_id) == config.GetHD_Channels().end()?0:1;
    auto found = result.find(scraperEvent);
    if (found == result.end() )
      result.insert(scraperEvent);
    else if (scraperEvent.isBetter(*found)) {
      result.erase(found);
      result.insert(std::move(scraperEvent));
    }
  }
  return result;
}

void createTimer(const cEventMovieOrTv &scraperEvent, const cScraperRec &recording) {
// create timer for event, in same folder as existing recording
  size_t pos = recording.name().find_last_of('~');
  if (pos == std::string::npos || pos == 0) {
    createTimer(scraperEvent.m_event);
    return;
  }
  if (recording.seasonNumber() == -100) {
    createTimer(scraperEvent.m_event, (recording.name().substr(0, pos + 1) + scraperEvent.m_event->Title()).c_str() );
    return;
  }
// TV show. Test: Title~ShortText ?
  size_t pos2 = recording.name().find_last_of('~', pos - 1);
  if (pos2 == std::string::npos) pos2 = 0;
  else pos2++;
  if (recording.name().substr(pos2, pos-pos2).compare(scraperEvent.m_event->Title() ) == 0) {
    if (scraperEvent.m_event->ShortText() )
      createTimer(scraperEvent.m_event, (recording.name().substr(0, pos + 1) + scraperEvent.m_event->ShortText()).c_str() );
    else if (scraperEvent.m_event->Description() ) {
      if (strlen(scraperEvent.m_event->Description()) < 50)
        createTimer(scraperEvent.m_event, (recording.name().substr(0, pos + 1) + scraperEvent.m_event->Description()).c_str() );
      else
        createTimer(scraperEvent.m_event, (recording.name().substr(0, pos + 1) + std::string(scraperEvent.m_event->Description(), 50)).c_str() );
    }
    else createTimer(scraperEvent.m_event, (recording.name().substr(0, pos + 1) + scraperEvent.m_event->Title()).c_str() );
    return;
  }
  createTimer(scraperEvent.m_event, (recording.name().substr(0, pos + 1) + scraperEvent.m_event->Title()).c_str() );
  return;
}

// timerForEvent ********************************
bool timerForEvent(const cTVScraperDB &db, const cEventMovieOrTv &scraperEvent, const std::set<cScraperRec, std::less<>> &recordings, const std::set<int> &collections) {
// check:
//   is there a recording, which can be improved -> timer
//   else:
//      is event part of a collection, that shall be recorded -> timer
//      is event part of a TV show, that shall be recorded -> timer
// return false if no timer is available or created. true, otherwise

  if (scraperEvent.m_event->HasTimer()) return true; // there is already a timer for this event
// is there a recording?
  auto found = recordings.find(scraperEvent);
  if (found != recordings.end()) {
    if (scraperEvent.m_hd <  found->hd() ) return false;
    if (scraperEvent.m_hd == found->hd() && found->numberOfErrors() == 0) return false;
// we need a timer ...
    createTimer(scraperEvent, *found);
    return true;
  }
// there is no recording with this movie / TV show
  if (scraperEvent.m_season_number == -100) {
// movie. Check: record because of collection?
    int collection_id = db.GetMovieCollectionID(scraperEvent.m_movie_tv_id);
    if (collection_id <= 0) return false;
    auto col_found = collections.find(collection_id);
    if (col_found == collections.end() ) return false;
// find recording with this collection (so new rcording will be in same folder)
    char sql[] = "select movie_id from movies3 where movie_collection_id = ?";
    int movie_id = db.QueryInt(sql, "");
    if (movie_id == 0) {
      createTimer(scraperEvent.m_event);
      return true;
    }
    cEventMovieOrTv eventMovieOrTv;
    eventMovieOrTv.m_movie_tv_id = movie_id;
    eventMovieOrTv.m_season_number = -100;
    eventMovieOrTv.m_episode_number = 0;
    auto found_c = recordings.find(eventMovieOrTv);
    if (found_c == recordings.end() ) {
      createTimer(scraperEvent.m_event);
      return true;
    }
    createTimer(scraperEvent, *found_c);
    return true;
  }
// TV show. Check: record because of all episodes shall be recorded?
  auto show_found = config.GetTV_Shows().find(scraperEvent.m_movie_tv_id);
  if (show_found == config.GetTV_Shows().end() ) return false;
// find recording with this TV show, so same folder can be used
  auto found_s = recordings.find(scraperEvent.m_movie_tv_id);
  if (found_s == recordings.end() ) {
    createTimer(scraperEvent.m_event);
    return true;
  }
  createTimer(scraperEvent, *found_s);
  return true;
}


bool timersForEvents(const cTVScraperDB &db) {
  std::set<cScraperRec, std::less<>> recordings;
  if (!getRecordings(db, recordings) ) return false;
  std::set<int> collections;
  if (!getCollections(db, recordings, collections) ) return false;

  for (const cEventMovieOrTv &scraperEvent: getAllEvents(db) )
    timerForEvent(db, scraperEvent, recordings, collections);
  return true;
}

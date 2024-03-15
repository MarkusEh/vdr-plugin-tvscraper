#include "autoTimers.h"
#include "tvscraperdb.h"

bool operator< (const cScraperRec &first, int sec) {
// to find TV show, with the given movie_tv_id
// ignore season / episode of TV show, select just one
  return (first.m_season_number != -100) && (first.m_movie_tv_id < sec);
}
bool operator< (int first, const cScraperRec &sec) {
// to find TV show, with the given movie_tv_id
// ignore season / episode of TV show, select just one
  return (sec.m_season_number != -100) && (first < sec.m_movie_tv_id);
}

bool operator< (const cScraperRec &rec1, const cScraperRec &rec2) {
  return compareMovieOrTvAT(&rec1, &rec2);
}

bool operator< (const cEventMovieOrTv &rec1, const cEventMovieOrTv &rec2) {
  return compareMovieOrTvAT(&rec1, &rec2);
}

bool operator< (const cScraperRec &first, const cEventMovieOrTv &sec) {
  return compareMovieOrTvAT(&first, &sec);
}
bool operator< (const cEventMovieOrTv &first, const cScraperRec &sec) {
  return compareMovieOrTvAT(&first, &sec);
}

bool operator< (const cTimerMovieOrTv &timer1, const cTimerMovieOrTv &timer2) {
  return compareMovieOrTvAT(&timer1, &timer2);
}

bool operator< (const cTimerMovieOrTv &timer1, const cEventMovieOrTv &sec) {
  return compareMovieOrTvAT(&timer1, &sec);
}

bool operator< (const cEventMovieOrTv &first, const cTimerMovieOrTv &timer2) {
  return compareMovieOrTvAT(&first, &timer2);
}

cMovieOrTvAT::cMovieOrTvAT(const tChannelID &channel_id, const cMovieOrTv *movieOrTv) {
  m_movie_tv_id = movieOrTv->dbID();
  if (movieOrTv->getType() == tMovie) {
    m_season_number = -100;
    m_episode_number = 0;
  } else {
    m_season_number = movieOrTv->getSeason();
    m_episode_number = movieOrTv->getEpisode();
  }
  m_hd = config.ChannelHD(channel_id);
  m_language = config.GetLanguage_n(channel_id);
}

// getEvent ********************************
const cEvent* getEvent(tEventID eventid, const tChannelID &channelid) {
// note: NULL is returned, if this event is not available
// VDR uses GarbageCollector for events -> the returned event will be an object for 5 seconds
  if ( !channelid.Valid() || eventid == 0 ) return NULL;
  cSchedule const* schedule;
#if VDRVERSNUM >= 20301
  LOCK_SCHEDULES_READ;
#else
  cSchedulesLock schedLock;
  cSchedules const* Schedules = cSchedules::Schedules( schedLock );
  if (!Schedules) return NULL;
#endif
  schedule = Schedules->GetSchedule( channelid );
  if (!schedule) return NULL;
  return schedule->GetEvent(eventid);
// note: GetEvent is deprecated, but GetEventById not available in VDR 2.4.8. return schedule->GetEventById(eventid);
}

bool AdjustSpawnedTimer(cTimer *ti, time_t tstart, time_t tstop) {
  // Event start/end times are given in "seconds since the epoch". Some broadcasters use values
  // that result in full minutes (with zero seconds), while others use any values. VDR's timers
  // use times given in full minutes, truncating any seconds. Thus we only react if the start/stop
  // times of the timer are off by at least one minute:
  if (abs(ti->StartTime() - tstart) >= 60 || abs(ti->StopTime() - tstop) >= 60) {
    cString OldDescr = ti->ToDescr();
    struct tm tm_r;
    struct tm *time = localtime_r(&tstart, &tm_r);
    ti->SetDay(cTimer::SetTime(tstart, 0));
    ti->SetStart(time->tm_hour * 100 + time->tm_min);
    time = localtime_r(&tstop, &tm_r);
    ti->SetStop(time->tm_hour * 100 + time->tm_min);
    ti->Matches();
    isyslog("tvscraper: timer %s times changed to %s-%s", *OldDescr, *TimeString(tstart), *TimeString(tstop));
    return true;
  }
  return false;
}

bool AdjustSpawnedTimer(cTimer *ti, const cEvent *event1, const cEvent *event2)
// Adjust the timer to shifted start/stop times of the events if necessary
// Note: the timer records both events!
{
  if (!ti || !event1 || !event2) return false;
  time_t tstart = std::min(event1->StartTime(), event2->StartTime() );
  time_t tstop = std::max(event1->EndTime(), event2->EndTime() );
  tstart -= Setup.MarginStart * 60;
  tstop  += Setup.MarginStop * 60;
  return AdjustSpawnedTimer(ti, tstart, tstop);
}

// createTimer ********************************
const cTimer* createTimer(const cEventMovieOrTv &scraperEvent, const cEvent *event, const cEvent *event2, const std::string &aux = "", const char *fileName = nullptr) {
// create a timer for event. If event2 is also given, create a timer for event+event2 (over 2 events)
  if (!event) return nullptr;
  cTimer *timer = new cTimer(event, fileName);
  timer->ClrFlags(tfRecording);
  if (scraperEvent.m_hd > 0) timer->SetPriority(15);
  else timer->SetPriority(10);
  if (aux.empty() ) timer->SetAux("<tvscraper></tvscraper>");
  else timer->SetAux(aux.c_str() );
  if (event2) AdjustSpawnedTimer(timer, event, event2);
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
// return true on success, false in case of (temporary) not availability of data
// all recordings where a movie or TV episode was identified are returned.
// In case of duplicates, the better one is returned

  cSql runtime_movie(&db, "select movie_runtime from movies3 where movie_id = ?");
  cSql runtime_tv(&db, "select episode_run_time from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?");
  LOCK_RECORDINGS_READ;
  for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec))
  {
    const char *pos_delim = strrchr(rec->Name(), '~');
    if (pos_delim != 0)
      if (!config.recordingFolderSelected(std::string(rec->Name(), pos_delim - rec->Name() ))) continue;
    int  movie_tv_id, season_number, episode_number, runtime_guess, duration_deviation;
    if (!db.GetMovieTvID(rec, movie_tv_id, season_number, episode_number, &runtime_guess, &duration_deviation)) continue;
    if (movie_tv_id == 0) continue;
    if (season_number == 0 && episode_number == 0) continue; // we look only for recordings we can assign to a specific episode/movie
    if (season_number == -100) episode_number = 0;
    csRecording sRecording(rec);
    tEventID eventID = sRecording.EventID();
    time_t eventStartTime = sRecording.StartTime();
#if VDRVERSNUM >= 20505
    int numberOfErrors = rec->Info()->Errors();
#else
    int numberOfErrors = 0;
#endif
    if (numberOfErrors < 0) numberOfErrors = 1; // -1: Not checked, also create timer in this case
    if (numberOfErrors == 0) {
// check duration deviation
      if (duration_deviation < 0) {
// no data in cache, find out and update cache in recordings2
        int runtime;
        cSql *runtime_sql;
        if (season_number == -100) { runtime_movie.resetBindStep(movie_tv_id); runtime_sql = &runtime_movie; }
        else { runtime_tv.resetBindStep(movie_tv_id, season_number, episode_number); runtime_sql = &runtime_tv; }
        runtime = runtime_sql->getInt(0, runtime_guess, runtime_guess);
        if (runtime <= 0) runtime = runtime_guess;
        duration_deviation = sRecording.durationDeviation(runtime);
        if (duration_deviation >= 0) db.SetDurationDeviation(rec, duration_deviation);
        else duration_deviation = 0;
      }
      if (duration_deviation > 60) numberOfErrors = 1;  // in case of deviations > 1min, create timer ...
    }
    if (numberOfErrors == 0 && config.GetTimersOnNumberOfTsFiles() && GetNumberOfTsFiles(rec) != 1) numberOfErrors = 1;  //  in case of more ts files, create timer ...
    cScraperRec scraperRec(eventID, eventStartTime, sRecording.ChannelID(), rec->Name(), movie_tv_id, season_number, episode_number, numberOfErrors, rec->Id() );
    auto found = recordings.find(scraperRec);
    if (found == recordings.end() ) recordings.insert(std::move(scraperRec)); // not in list -> insert
    else if (scraperRec.isBetter(*found)) {
      recordings.erase(found);
      recordings.insert(std::move(scraperRec));
    }
  }
// don't remove entries without action item, return all (unique) recordings
  return true;
}

bool InsertIfBetter (std::set<cTimerMovieOrTv, std::less<>> &timers, const cTimerMovieOrTv &timer, int *obsoletTimer = NULL) {
// in case timer is already in list, insert "better" timer in list, and return "worse" timer in obsoletTimer if requested
// return true if timer is already in list
  auto found = timers.find(timer);
  if (found == timers.end() ) {
    timers.insert(timer);
    return false;
  }
  if (timer.isBetter(*found) ) {
    if (obsoletTimer) *obsoletTimer = found->m_timerId;
    timers.erase(found);
    timers.insert(timer);
    return true;
  }
  if (obsoletTimer) *obsoletTimer = timer.m_timerId;
  return true;
}
// getAllTimers  ********************************
void getAllTimers (const cTVScraperDB &db, std::set<cTimerMovieOrTv, std::less<>> &otherTimers, std::set<cTimerMovieOrTv, std::less<>> &myTimers) {
// only timers where a movie or TV episode was found (i.a. only events in db, and only if episode was found)
// for each movie or tv, only one (the best) timer. HD better than SD. Otherwise, the earlier the better
  std::vector<int> timersToDelete;
  { // start LOCK_TIMERS_READ block
#if VDRVERSNUM >= 20301
  LOCK_TIMERS_READ;
  for (const cTimer *ti = Timers->First(); ti; ti = Timers->Next(ti)) if (ti->Local())
#else
  for (const cTimer *ti = Timers.First(); ti; ti = Timers.Next(ti))
#endif
  {
    if (!ti->Event() ) continue; // own timers without event are deleted in AdjustSpawnedScraperTimers
// get movie/tv for event
    cMovieOrTv *movieOrTv = cMovieOrTv::getMovieOrTv(&db, ti->Event() );
    bool mIdentified = movieOrTv && (movieOrTv->getType() == tMovie || (movieOrTv->getSeason() != 0 || movieOrTv->getEpisode() != 0));
    bool ownTimer = ti->Aux() && strncmp(ti->Aux(), "<tvscraper>", 11) == 0;
    if (!mIdentified) {
      if (ownTimer && !ti->Recording() ) timersToDelete.push_back(ti->Id());
      if (movieOrTv) delete movieOrTv;
      continue;
    }
    cTimerMovieOrTv timer(ti, movieOrTv);
    timer.m_needed = false;
    delete movieOrTv;
    timer.m_hd = config.ChannelHD(ti->Channel()->GetChannelID());
    if (ownTimer) {
// my timer
      int obsoletTimer;
      if (InsertIfBetter(myTimers, timer, &obsoletTimer) ) timersToDelete.push_back(obsoletTimer);
    } else {
// not my timer
      InsertIfBetter(otherTimers, timer);
    }
  }
  } // end LOCK_TIMERS_READ block
// delete obsolete timers
  for (const int &timerToDelete: timersToDelete) {
    cTimer *ti;
#if VDRVERSNUM >= 20301
    LOCK_TIMERS_WRITE;
    ti = Timers->GetById(timerToDelete);
    if (ti && !ti->Recording() ) {
      if (config.enableDebug) esyslog("tvscraper: getAllTimers, timer %s deleted", *(ti->ToDescr() ));
      Timers->Del(ti);
    }
#else
    ti = Timers.GetById(timerToDelete);
    if (ti && !ti->Recording() ) Timers.Del(ti);
#endif
  }
  return;
}

// getAllEvents ********************************
std::set<cEventMovieOrTv> getAllEvents(const cTVScraperDB &db) {
// ignore events which are running or about to start (in 10 mins or less)
// only events where a movie or TV episode was found (i.a. only events in db, and only if episode was found)
// for each movie or tv, only one (the best) event. HD better than SD. Otherwise, the earlier the better
  std::set<cEventMovieOrTv> result;
  const time_t now_m10 = time(0) - 10*60;
  cEventMovieOrTv scraperEvent(0);
  tEventID l_event_id = 0;
  const char *l_channel_id = NULL;
  for (cSql &statement: cSql(&db, "SELECT event_id, channel_id, movie_tv_id, season_number, episode_number FROM event"))
  {
    statement.readRow(l_event_id, l_channel_id, scraperEvent.m_movie_tv_id, scraperEvent.m_season_number, scraperEvent.m_episode_number);
    if (scraperEvent.m_season_number == 0 && scraperEvent.m_episode_number == 0) continue;
    if (!l_channel_id || !*l_channel_id) {
      esyslog("tvscraper: ERROR getAllEvents, l_channel_id == NULL");
      continue;
    }
    scraperEvent.m_channelid = tChannelID::FromString(l_channel_id);
    const cEvent *event = getEvent(l_event_id, scraperEvent.m_channelid);
    if (!event) continue;
    if (event->IsRunning(true) ) continue;  // event already started, ignore
    if (event->StartTime() <= now_m10 ) continue;  // IsRunning does sometimes not work
    scraperEvent.m_event_start_time = event->StartTime();
    scraperEvent.m_event_id = l_event_id;
    if (scraperEvent.m_season_number == -100) scraperEvent.m_episode_number = 0;
    scraperEvent.m_hd = config.ChannelHD(scraperEvent.m_channelid);
    scraperEvent.m_language = config.GetLanguage_n(scraperEvent.m_channelid);
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

const cRecording *recordingFromAux(const char *aux) {
  if (!aux || !*aux) return nullptr;
// parse aux for IDs of recording
  cSv xml_tvscraper = partInXmlTag(aux, "tvscraper");
  if (xml_tvscraper.empty() ) return nullptr;
  cSv xml_causedByIDs = partInXmlTag(xml_tvscraper, "causedByIDs");
  if (xml_causedByIDs.empty() ) return nullptr;
  cSv xml_eventID = partInXmlTag(xml_causedByIDs, "eventID");
  if (xml_eventID.empty() ) return nullptr;
  cSv xml_eventStartTime = partInXmlTag(xml_causedByIDs, "eventStartTime");
  if (xml_eventStartTime.empty() ) return nullptr;
  cSv xml_channelID = partInXmlTag(xml_causedByIDs, "channelID");
  bool channelID_valid = !xml_channelID.empty();
  tChannelID channelID;
  cSv name;
  if (channelID_valid) channelID = tChannelID::FromString(std::string(xml_channelID).c_str() );
  else {
    name = partInXmlTag(xml_tvscraper, "causedBy");
    if (name.empty() ) return nullptr;
  }
  tEventID event_id = parse_unsigned<tEventID>(xml_eventID);
  time_t event_start_time = parse_unsigned<time_t>(xml_eventStartTime);

  LOCK_RECORDINGS_READ;
  for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec))
  {
    if (!rec->Info() || !rec->Info()->GetEvent() ) continue;
    if (rec->Info()->GetEvent()->EventID() != event_id) continue;
    csRecording sRecording(rec);
    if (sRecording.StartTime() != event_start_time) continue;
    if (sRecording.ChannelID().Valid() != channelID_valid) continue;
    if (channelID_valid) {
      if (!(sRecording.ChannelID() == channelID) ) continue;
    } else {
      if (name != rec->Name() ) continue;
    }
    return rec;
  }
  return nullptr;
}

std::string getAux(const cTVScraperDB &db, const cScraperRec *recording, const char *reason, const cEvent *event2) {
  std::string result("<tvscraper>");
  if (recording) {
    result.append("<causedBy>");
    result.append(recording->name() );
    result.append("</causedBy>");
    result.append("<causedByIDs>");
    result.append("<eventID>");
    stringAppend(result, recording->EventID() );
    result.append("</eventID>");
    result.append("<eventStartTime>");
    stringAppend(result, recording->StartTime() );
    result.append("</eventStartTime>");
    tChannelID channelID = recording->ChannelID();
    if (channelID.Valid() ) {
      result.append("<channelID>");
      stringAppend(result, channelID);
      result.append("</channelID>");
    }
    result.append("</causedByIDs>");
  }
  if (reason) {
    if (recording && strcmp(reason, "collection") == 0) {
      cSql stmt(&db, "select movie_collection_name from movies3 where movie_id = ?", recording->movieTvId() );
      if (stmt.readRow() ) {
        const char *nameCollection = stmt.getCharS(0);
        if (nameCollection) { 
          result.append("<collectionName>");
          result.append(nameCollection );
          result.append("</collectionName>");
        }
      }
    }
    if (recording && strcmp(reason, "TV show, missing episode") == 0) {
      cSql stmt(&db, "select tv_name from tv2 where tv_id = ?", recording->movieTvId() );
      if (stmt.readRow() ) {
        const char *nameSeries = stmt.getCharS(0);
        if (nameSeries) { 
          result.append("<seriesName>");
          result.append(nameSeries );
          result.append("</seriesName>");
        }
      }
    }
    result.append("<reason>");
    result.append(reason);
    result.append("</reason>");
  }
  if (event2) {
    result.append("<numEvents>");
    result.append("2");
    result.append("</numEvents>");
  }
  result.append("</tvscraper>");
  return result;
}

std::string getEpisodeName(const cTVScraperDB &db, const cEventMovieOrTv &scraperEvent, const cEvent *event) {
// default: episode name = short text
  if (event->ShortText() && *event->ShortText() )
    return event->ShortText();
// no short text. Check episode name from data base
  const char sql[] = "select episode_name from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?";
  std::string episode_name = db.queryString(sql, scraperEvent.m_movie_tv_id, scraperEvent.m_season_number, scraperEvent.m_episode_number);
  if (!episode_name.empty() ) return episode_name;
// no short text. No episode name in data base. Check description
  if (event->Description() && *event->Description() )
    return strlen(event->Description()) < 50?event->Description():std::string(event->Description(), 50);
  return "no episode name found";
}

bool EventsMovieOrTvEqual(const cTVScraperDB &db, const cEvent *event1, const cEvent *event2, const cEventMovieOrTv *scraperEvent1 = NULL) {
// return true if the MovieOrTv assigned to event1 is equal to the MovieOrTv assigned to event2
// if scraperEvent1 is provided, it must contain the movie_tv_id, season_number and episode_number of event1
  if (cSv(event1->Title()) == cSv(event2->Title()) && cSv(event1->ShortText()) == cSv(event2->ShortText()) && cSv(event1->Description()) == cSv(event2->Description()) ) return true;

  int movie_tv_id1, season_number1, episode_number1;
  int movie_tv_id2, season_number2, episode_number2;
  if (!db.GetMovieTvID(event2, movie_tv_id2, season_number2, episode_number2)) return false;
  if (scraperEvent1) {
     movie_tv_id1 = scraperEvent1->m_movie_tv_id;
     season_number1 = scraperEvent1->m_season_number;
     episode_number1 = scraperEvent1->m_episode_number;
  } else {
    if (!db.GetMovieTvID(event1, movie_tv_id1, season_number1, episode_number1)) return false;
  }
// if (config.enableDebug) esyslog ("tvscraper: AdjustSpawnedScraperTimers, ti %s movie1 %i movie2 %i %s", ti->File(), movieOrTv1->dbID(), movieOrTv2->dbID(), *movieOrTv1 == *movieOrTv2?"equal":"not equal" );
  if (movie_tv_id1 != movie_tv_id2 || season_number1 != season_number2) return false;
  if (season_number1 != -100 && (episode_number1 !=  episode_number2) ) return false;
  return true;
}

const cEvent *getEvent2(const cTVScraperDB &db, const cEventMovieOrTv &scraperEvent, const cEvent *event) {
// check if the complete movie required a second event
  bool debug = scraperEvent.m_movie_tv_id == 297762; // wonder woman
  debug = false;
  if (debug) esyslog("tvscraper const cEvent *getEvent2 (0)");
  if (!event) return NULL;
  if (debug) esyslog("tvscraper const cEvent *getEvent2 (1)");
  if (event->Vps() ) return NULL;  // not for vps events
  if (debug) esyslog("tvscraper const cEvent *getEvent2 (2)");
  if (scraperEvent.m_season_number != -100) return NULL; // only for movies
  if (debug) esyslog("tvscraper const cEvent *getEvent2 (3)");
  int movieRuntime = db.GetMovieRuntime(scraperEvent.m_movie_tv_id);
  int eventDuration = event->Duration() / 60;
  if (eventDuration >= movieRuntime) return NULL;
  if (debug) esyslog("tvscraper const cEvent *getEvent2 (4)");
  const cSchedule *Schedule = event->Schedule();
  if (!Schedule ) return NULL;
  if (debug) esyslog("tvscraper const cEvent *getEvent2 (5)");
  const cEvent *eN = Schedule->Events()->Next(event);
  if (!eN) return NULL;
  if (debug) esyslog("tvscraper const cEvent *getEvent2 (6)");
  if ( eN->Duration() / 60 > 15) return NULL;
  if (debug) esyslog("tvscraper const cEvent *getEvent2 (7)");
  const cEvent *e2N = Schedule->Events()->Next(eN);
  if (!e2N) return NULL;
  if (debug) esyslog("tvscraper const cEvent *getEvent2 (8)");

  if (EventsMovieOrTvEqual(db, event, e2N, &scraperEvent) ) return e2N;
  return NULL;
}

void createTimer(const cTVScraperDB &db, const cEventMovieOrTv &scraperEvent, const char *reason, const cScraperRec *recording) {
// create timer for event, in same folder as existing recording
  const cEvent *event = getEvent(scraperEvent.m_event_id, scraperEvent.m_channelid);
  if (!event) return;
  const cEvent *event2 = getEvent2(db, scraperEvent, event);
  if (config.GetAutoTimersPathSet() || !recording) {
    std::string base = config.GetAutoTimersPathSet()?config.GetAutoTimersPath():"";
    if (scraperEvent.m_season_number == -100) {
      createTimer(scraperEvent, event, event2, getAux(db, recording, reason, event2), (base + event->Title()).c_str() );
    } else {
// structure of recording: title/episode_name
      createTimer(scraperEvent, event, event2, getAux(db, recording, reason, event2), (base + event->Title() + '~' + getEpisodeName(db, scraperEvent, event) ).c_str() );
    }
    return;
  }
  size_t pos = recording->name().find_last_of('~');
  if (pos == std::string::npos || pos == 0) {
    createTimer(scraperEvent, event, event2, getAux(db, recording, reason, event2) );
    return;
  }
  cSv folderName(recording->name().substr(0, pos + 1) );
  if (recording->seasonNumber() == -100) {
    createTimer(scraperEvent, event, event2, getAux(db, recording, reason, event2), cToSvConcat(folderName, event->Title()).c_str() );
    return;
  }
// TV show. Test: Use short text (episodeName) as name?
// we test whether the name of the recording is it's short text
  bool useShortText = folderName.find(event->Title() ) != std::string_view::npos;
  if (!useShortText) {
    const cRecording *rec;
    {
      LOCK_RECORDINGS_READ;
      rec = Recordings->GetById(recording->id());
    }
    if (!rec || !rec->Info() ) {
      if (!rec) esyslog("tvscraper: ERROR createTimer, rec not found, name %.*s", (int)recording->name().length(), recording->name().data());
      else esyslog("tvscraper: ERROR createTimer, rec->Info not found, name %.*s", (int)recording->name().length(), recording->name().data());
      return;
    }
    cSv recName = recording->name().substr(pos + 1);
    if (recName.substr(0, 1) == "%") recName.remove_prefix(1);
    const char *shortText = rec->Info()->ShortText();
    if (!shortText || ! *shortText) shortText = rec->Info()->Description();
    useShortText = recName == cSv(shortText);
    if (!useShortText && shortText && *shortText) {
      cNormedString nsBaseNameOrTitle(recName);
      int distTitle     = nsBaseNameOrTitle.sentence_distance(rec->Info()->Title() );
      int distShortText = nsBaseNameOrTitle.sentence_distance(shortText);
      if (distTitle > 600 && distShortText > 600) useShortText = true;
      else useShortText = distShortText <= distTitle;
    }
  }

  if (useShortText) {
// structure of old recording: title/episode_name
    if (event->ShortText() && *event->ShortText() )
      createTimer(scraperEvent, event, event2, getAux(db, recording, reason, event2), cToSvConcat(folderName, event->ShortText() ).c_str() );
    else if (event->Description() && *event->Description() )
      createTimer(scraperEvent, event, event2, getAux(db, recording, reason, event2), cToSvConcat(folderName, cSv(event->Description()).substr(0, 50) ).c_str() );
    else
      createTimer(scraperEvent, event, event2, getAux(db, recording, reason, event2), cToSvConcat(folderName, getEpisodeName(db, scraperEvent, event) ).c_str() );
  } else // structure of old recording: title
    createTimer(scraperEvent, event, event2, getAux(db, recording, reason, event2), cToSvConcat(folderName, event->Title()).c_str() );
  return;
}

bool canTimerBeDeleted(const cTimer *ti, const char *context = nullptr) {
// return true except timer is recording or Matches (Matches: would be recording, but cannot as other timers have higher priorities)
  time_t Now = time(NULL);
  if (!ti || ti->Recording() || ti->Matches(Now, false, 0) || ti->InVpsMargin() || (ti->Event() && ti->Event()->StartTime() <= Now + 10*60) ) return false;
  if (context && config.enableDebug) esyslog("tvscraper: %s, timer %s deleted", context, *(ti->ToDescr() ));
  return true;
}

bool timerGetEvents(const cEvent *&event1, const cEvent *&event2, const cTimer *ti) {
// return false if timer overlaps zero or one events
// otherwise, return the two longest overlapping events
  if (!ti || !ti->Event() || ti->HasFlags(tfVps) ) return false;
  const cSchedule *Schedule = ti->Event()->Schedule();
  if (!Schedule ) return false;

  // Set up the time frame within which to check events:
  ti->Matches(0, true);
  time_t TimeFrameBegin = ti->StartTime() - 2*60;
  time_t TimeFrameEnd   = ti->StopTime() + 2*60;
// create a list of events, with 100% overlap to this timer
  std::multimap<int, const cEvent *> events;
  for (const cEvent *e = Schedule->Events()->First(); e; e = Schedule->Events()->Next(e)) {
    if (e->StartTime() < TimeFrameBegin) continue; // skip events before the timer starts
    if (e->EndTime() > TimeFrameEnd) break; // the rest is after the timer ends
    events.insert(std::pair{e->Duration(), e});
  }
  if (events.size() < 2) return false;
  auto itMax = events.crbegin();
  event1 = itMax->second;
  itMax++;
  event2 = itMax->second;
  return true;
}

bool AdjustSpawnedTimer(cTimer *ti)
{
  if (ti->Event()) {
    if (ti->Event()->Schedule()) { // events may be deleted from their schedule in cSchedule::DropOutdated()!
      // Adjust the timer to shifted start/stop times of the event if necessary:
      time_t tstart = ti->Event()->StartTime();
      time_t tstop = ti->Event()->EndTime();
      int MarginStart = 0;
      int MarginStop  = 0;
#if APIVERSNUM < 20502
      MarginStart = Setup.MarginStart * 60;
      MarginStop  = Setup.MarginStop * 60;
#else
      ti->CalcMargins(MarginStart, MarginStop, ti->Event());
#endif
      tstart -= MarginStart;
      tstop  += MarginStop;
      return AdjustSpawnedTimer(ti, tstart, tstop);
    }
  }
  return false;
}

bool AdjustSpawnedScraperTimers(const cTVScraperDB &db) {
  bool TimersModified = false;
  cTimer *ti_del = NULL;
#if VDRVERSNUM >= 20301
  LOCK_TIMERS_WRITE;
  for (cTimer *ti = Timers->First(); ti; ti = Timers->Next(ti)) if (ti->Local())
#else
  for (cTimer *ti = Timers.First(); ti; ti = Timers.Next(ti))
#endif
  {
    if (ti_del) {
      if (canTimerBeDeleted(ti_del, "AdjustSpawnedScraperTimers") )
        Timers->Del(ti_del);
      ti_del = NULL;
    }
    bool exists = false;
    cSv xmlAux;
    if (ti->Aux()) xmlAux = partInXmlTag(ti->Aux(), "tvscraper", &exists);
    if (exists) {
// this is "our" timer
      if (!ti->Event() || !ti->Event()->Schedule() ) {
// Timer has no event, or event is not in schedule any more. Delete this timer
        ti_del = ti;
      } else {
// Timer has event, and event is in schedule. Adjust times to event, if required (not for VPS timers)
        if (parse_unsigned<unsigned>(partInXmlTag(xmlAux, "numEvents")) == 2) {
          const cEvent *event1;
          const cEvent *event2;
          if (!timerGetEvents(event1, event2, ti)) ti_del = ti;
          else {
            if (EventsMovieOrTvEqual(db, event1, event2) ) TimersModified |= AdjustSpawnedTimer(ti, event1, event2);
            else ti_del = ti;
          }
        } else if (!ti->HasFlags(tfVps)) TimersModified |= AdjustSpawnedTimer(ti);
      }
    }
  }
  if (ti_del) {
    if (canTimerBeDeleted(ti_del, "AdjustSpawnedScraperTimers 2") )
      Timers->Del(ti_del);
    ti_del = NULL;
  }
  return TimersModified;
}

bool checkTimer(const cEventMovieOrTv &scraperEvent, std::set<cTimerMovieOrTv, std::less<>> &myTimers) {
// return true if it is required to create a new timer
  std::set<cTimerMovieOrTv, std::less<>>::iterator found = myTimers.find(scraperEvent);
  if (found != myTimers.end() && scraperEvent.m_hd <= found->m_hd ) {
    (*found).m_needed = true;
    return false;
  }
  return true;
}

// timerForEvent ********************************
bool timerForEvent(const cTVScraperDB &db, const cEventMovieOrTv &scraperEvent, std::set<cTimerMovieOrTv, std::less<>> &myTimers, const std::set<cScraperRec, std::less<>> &recordings, const std::set<int> &collections) {
// for the given event, check:
//   is there a recording, which can be improved -> timer
//   else:
//      is event part of a collection, that shall be recorded -> timer
//      is event part of a TV show, that shall be recorded -> timer
// return false if no timer is available or created. true, otherwise

  cMovieOrTvAT movieOrTvAT(&scraperEvent);
  movieOrTvAT.m_language = 0;
  bool betterRecordingFound = false;
  const cScraperRec *improveableRecording = NULL;
  for (auto found = recordings.lower_bound(movieOrTvAT);
       found != recordings.end() && equalWoLanguageMovieOrTvAT(&(*found), &scraperEvent);
       found ++) {
// there is a recording
    if (betterRecordingFound && scraperEvent.m_language != found->m_language) continue; // a recording which cannot be improved, in another language was already found. So we only check recordings in the same language.
    if ((scraperEvent.m_hd >  found->hd()) ||
        (scraperEvent.m_hd == found->hd() && found->numberOfErrors() > 0)) {
// the recording can be improved with the event
      improveableRecording = &(*found);
      continue;  // continue to check: There might be another recording in the same language as event, which cannot be improved
    }
// the recording is equal or better than the event
    if (scraperEvent.m_language == found->m_language) return false;  // there is already a recording, in the same language, which cannot be improved with the event
    betterRecordingFound = true;  // there is already a recording, in another language, which cannot be improved with the event
  }
  if (improveableRecording) {
    if (checkTimer(scraperEvent, myTimers)) createTimer(db, scraperEvent, "improve", improveableRecording);
    return true;
  }
  if (betterRecordingFound) return false; // if we want timers for collections, in other languages, we can remove this

// there is no recording with this movie / TV show
  if (scraperEvent.m_season_number == -100) {
// movie. Check: record because of collection?
    int collection_id = db.GetMovieCollectionID(scraperEvent.m_movie_tv_id);
    if (collection_id <= 0) return false;
    if (collections.find(collection_id)  == collections.end() ) return false;
// find recording with this collection (so new recording will be in same folder)
    cMovieOrTvAT movieOrTvAT(0);
    movieOrTvAT.m_season_number = -100;
    movieOrTvAT.m_episode_number = 0;
    movieOrTvAT.m_language = 0;
    for (cSql &statement: cSql(&db, "SELECT movie_id FROM movies3 WHERE movie_collection_id = ?", collection_id)) {
      movieOrTvAT.m_movie_tv_id = statement.getInt(0);
      auto found = recordings.lower_bound(movieOrTvAT);
      if (found != recordings.end() && equalWoLanguageMovieOrTvAT(&(*found), &movieOrTvAT) ) {
        if (checkTimer(scraperEvent, myTimers)) createTimer(db, scraperEvent, "collection", &(*found));
        return true;
      }
    }
// no recording with this collection found (?)
    if (checkTimer(scraperEvent, myTimers)) createTimer(db, scraperEvent, "collection", NULL);
    return true;
  }
// TV show. Check: record because of all episodes shall be recorded?
  if (!config.TV_ShowSelected(scraperEvent.m_movie_tv_id)) return false;
// find recording with this TV show, so same folder can be used
  auto found_s = recordings.find(scraperEvent.m_movie_tv_id); // this ensures that m_season_number != -100 ... (see < implementation ...)
  const cScraperRec *rec = found_s == recordings.end()?NULL:&(*found_s);
  if (checkTimer(scraperEvent, myTimers)) createTimer(db, scraperEvent, "TV show, missing episode", rec);
  return true;
}


bool timersForEvents(const cTVScraperDB &db) {
  std::set<cScraperRec, std::less<>> recordings;
  if (!getRecordings(db, recordings) ) return false;
  std::set<int> collections;
  if (!getCollections(db, recordings, collections) ) return false;

  AdjustSpawnedScraperTimers(db);
  std::set<cTimerMovieOrTv, std::less<>> otherTimers;
  std::set<cTimerMovieOrTv, std::less<>> myTimers;
  getAllTimers(db, otherTimers, myTimers);
//  esyslog("tvscraper: timersForEvents 5");

  for (const cEventMovieOrTv &scraperEvent: getAllEvents(db) ) {
    auto found = otherTimers.find(scraperEvent);
    if (found != otherTimers.end() && scraperEvent.m_hd <= found->m_hd ) continue;
    timerForEvent(db, scraperEvent, myTimers, recordings, collections);
  }
// delete obsolete timers
  for (const cTimerMovieOrTv &timerToDelete: myTimers) if (!timerToDelete.m_needed) {
#if VDRVERSNUM >= 20301
    LOCK_TIMERS_WRITE;
    cTimer *ti = Timers->GetById(timerToDelete.m_timerId);
    if (canTimerBeDeleted(ti, "timersForEvents") )
      Timers->Del(ti);
#else
    cTimer *ti = Timers.GetById(timerToDelete.m_timerId);
    if (canTimerBeDeleted(ti, "timersForEvents") )
      Timers.Del(ti);
#endif
  }
  return true;
}

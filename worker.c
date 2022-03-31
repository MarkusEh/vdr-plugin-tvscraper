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
if (config.GetReadOnlyClient() ) return;
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
        csEventOrRecording sEvent(event);
        cSearchEventOrRec SearchEventOrRec(&sEvent, overrides, moviedbScraper, tvdbScraper, db);  
	if( SearchEventOrRec.Scrap() ) waitCondition.TimedWait(mutex, 100);
    }
} else {
    dsyslog("tvscraper: Schedule is not availible, skipping");
}
}
}

void cTVScraperWorker::ScrapRecordings(void) {
  if (config.GetReadOnlyClient() ) return;
  db->ClearRecordings2();
#if APIVERSNUM < 20301
  for (cRecording *rec = Recordings.First(); rec; rec = Recordings.Next(rec)) {
#else
  LOCK_RECORDINGS_READ;
  for (const cRecording *rec = Recordings->First(); rec; rec = Recordings->Next(rec)) {
#endif
    if (config.enableDebug) esyslog("tvscraper: Scrap recording \"%s\"", rec->FileName() );

    if (overrides->IgnorePath(rec->FileName())) continue;
    csRecording csRecording(rec);
    const cRecordingInfo *recInfo = rec->Info();
    const cEvent *recEvent = recInfo->GetEvent();
    if (recEvent) {
          cSearchEventOrRec SearchEventOrRec(&csRecording, overrides, moviedbScraper, tvdbScraper, db);  
          SearchEventOrRec.Scrap();
          if (!Running() ) break;
    }
  }
}

void cTVScraperWorker::CheckRunningTimers(void) {
if (config.GetReadOnlyClient() ) return;
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
    csEventOrRecording sEvent(event);
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
        csRecording sRecording(recording);
	if (!db->SetRecording(&sRecording)) {
	    if (ConnectScrapers() && recording) { 
              cSearchEventOrRec SearchEventOrRec(&sRecording, overrides, moviedbScraper, tvdbScraper, db);  
              SearchEventOrRec.Scrap();
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
    DisconnectScrapers();
    db->BackupToDisc();
    dsyslog("tvscraper: scanning video dir done");
    continue;
}
CheckRunningTimers();
if (!Running() ) break;
if (StartScrapping()) {
    dsyslog("tvscraper: start scraping epg");
    if (ConnectScrapers()) {
	ScrapEPG();
    }
    DisconnectScrapers();
    cMovieOrTv::DeleteAllIfUnused(db);
    db->BackupToDisc();
    dsyslog("tvscraper: epg scraping done");
}
waitCondition.TimedWait(mutex, loopSleep);
}
}

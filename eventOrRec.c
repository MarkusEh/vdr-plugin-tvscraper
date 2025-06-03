#include "eventOrRec.h"
#include <dirent.h>
#define TIMERRECFILE      "/.timer"

csEventOrRecording::csEventOrRecording(const cEvent *event):
  m_event(event),
  m_description(event?config.splitDescription(event->Description()):cSv())
{
}
csEventOrRecording::csEventOrRecording(const cStaticEvent *sEvent):
  m_event(nullptr),
  m_description(sEvent?config.splitDescription(sEvent->Description()):cSv())
{
}
csEventOrRecording::csEventOrRecording(const cRecordingInfo *info):
  m_event(info?info->GetEvent():nullptr),
  m_description(info?config.splitDescription(info->Description()):cSv())
{
}
void csEventOrRecording::AddYears(cYears &years) const {
  years.addYears(Description() );
  years.addYears(ShortText() );
  years.addYears(Title() );
}
int csEventOrRecording::DurationDistance(int DurationInMin) {
// -1 : currently no data available, but should be available later (recording length unkown as recording is ongoing or destination of cut/copy/move
// -2 : no data available
// all others: distance ...

  if (DurationInMin <=0 ) return -2; // no data
  int durationInMinLow, durationInMinHigh;
  int durationRange = DurationRange(durationInMinLow, durationInMinHigh);
  if (durationRange < 0) return durationRange;
  if (DurationInMin > durationInMinHigh) return DurationInMin - durationInMinHigh;
  if (DurationInMin < durationInMinLow)  return durationInMinLow - DurationInMin;
  return 0;
}
int csEventOrRecording::DurationRange(int &durationInMinLow, int &durationInMinHigh) {
//  0 : data available
// -2 : no data available
// OLD !!!!! return true, if data is available
  if (!EventDuration() ) return -2;
  durationInMinLow  = DurationLowSec() / 60 - 1;
  durationInMinHigh = DurationHighSec() / 60 + (10 * DurationHighSec())  / 60 / 60;  // add 10 mins for 60 min duration, more for longer durations. This is because often a cut version of the movie is broadcasted
//  if (Recording() && Title() && strcmp("The Expendables 3", Title() ) == 0) 
//  esyslog("tvscraper: csEventOrRecording::DurationRange, title = %s, durationInMinLow  = %i,  durationInMinHigh = %i", Title(), durationInMinLow, durationInMinHigh);
  return 0;
}

cSv csEventOrRecording::EpisodeSearchString() const {
  if(ShortText() && *ShortText() ) return ShortText();
  return Description().substr(0, 100);
}
 
/*
std::string csEventOrRecording::ChannelName() const {
 tChannelID channelID = ChannelID();
 if (!channelID.Valid() ) {
   esyslog("tvscraper, csEventOrRecording::ChannelName, invalid channel ID, title %s", Title() );
   return m_unknownChannel;
 }

  LOCK_CHANNELS_READ;
  const cChannel *channel = Channels->GetByChannelID(channelID);
  if (!channel) {
    esyslog("tvscraper, csEventOrRecording::ChannelName, no channel for channel ID %s, title %s", ChannelIDs().c_str(), Title() );
    return m_unknownChannel;
  }
  return channel->Name();
}
*/

// Recording

csRecording::csRecording(const cRecording *recording):
  csEventOrRecording(recording?recording->Info():(cRecordingInfo*)nullptr),
  m_recording(recording)
{
  if (!recording ) {
    esyslog("tvscraper: ERROR: csRecording::csRecording !recording");
    return;
  }
  if (!recording->Info() ) {
    esyslog("tvscraper: ERROR: csRecording::csRecording !recording->Info()");
    return;
  }
  if (!recording->Info()->GetEvent() ) {
    esyslog("tvscraper: ERROR: csRecording::csRecording !recording->Info()->GetEvent()");
  }
//  if (cSv(recording->Info()->Title()) == "A Fish Called Wanda") m_debug = true;
  if (m_debug) esyslog("tvscraper, debug recording %s", recording->Info()->Title());
}

bool csRecording::recordingLengthIsChanging() {
  int use = m_recording->IsInUse();
  if (((use & ruDst     ) == ruDst)      |
      ((use & ruPending ) == ruPending)  |
      ((use & ruCanceled) == ruCanceled) ) return true;

  struct stat buffer;
  return stat(cToSvConcat(m_recording->FileName(), TIMERRECFILE).c_str(), &buffer) == 0;
}

int csRecording::DurationRange(int &durationInMinLow, int &durationInMinHigh) {
//  0 : data available
// -1 : currently no data available, but should be available later (recording length unknown as recording is ongoing or destination of cut/copy/move
// -2 : no data available
// OLD !!!! return true, if data is available
  if (recordingLengthIsChanging() ) return -1;
  if (!m_recording->FileName() ) return -2;
  
  if (!EventDuration() && !m_recording->LengthInSeconds() ) return -2;

  if (m_recording->IsEdited() || DurationInSecMarks() != -1) {
// length of recording without adds
    int durationInSec = m_recording->IsEdited()?m_recording->LengthInSeconds():DurationInSecMarks();
    durationInMinLow  = durationInSec / 60 - 2;  // 2 min for inaccuracies
    durationInMinHigh = durationInSec / 60 +  (15 * durationInSec)  / 60 / 60;  // add 15 mins for 60 min duration, more for longer durations. This is because often a cut version of the movie is broadcasted. Also, the markad results are not really reliable
    return 0;
  } else {
    return csEventOrRecording::DurationRange(durationInMinLow, durationInMinHigh);
  }
}

int csRecording::DurationInSecMarks_int(void) {
// return -1 if no data is available
// otherwise, duration of cut recording
// also set m_durationInSecMarks, to the returned value
  m_durationInSecMarks = -1;
  if (!m_recording) return m_durationInSecMarks;
  if (!m_recording->HasMarks() ) return m_durationInSecMarks;
  cMarks marks;
  marks.Load(m_recording->FileName(), m_recording->FramesPerSecond(), m_recording->IsPesRecording() );
  int numSequences = marks.GetNumSequences();
  if (numSequences == 0) return m_durationInSecMarks;  // only one begin mark at index 0 -> no data

// do the calculation
  const cMark *BeginMark = marks.GetNextBegin();
  if (!BeginMark) return m_durationInSecMarks;
  int durationInFrames = 0;
  while (const cMark *EndMark = marks.GetNextEnd(BeginMark)) {
    durationInFrames += EndMark->Position() - BeginMark->Position();
    BeginMark = marks.GetNextBegin(EndMark);
  }
  if (BeginMark) {  // the last sequence had no actual "end" mark
    durationInFrames += m_recording->LengthInSeconds() * m_recording->FramesPerSecond() - BeginMark->Position();
  }
  int durationInSeconds = durationInFrames / m_recording->FramesPerSecond();

// calculation finished, sanity checks:
  if (m_recording->LengthInSeconds() < durationInSeconds) {
    esyslog("tvscraper: ERROR: recording length shorter than length of cut recording, recording length %i length of cut recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, m_recording->FileName() );
    return m_durationInSecMarks;
  }

  if (numSequences == 1) {
// no adds found, just recording margin at start / stop
    if (EventDuration() ) {
// event duration is available, and should be equal to duration of cut recording
      if (abs(EventDuration() - durationInSeconds) < 4*60) m_durationInSecMarks = durationInSeconds;
      else isyslog("tvscraper: GetDurationInSecMarks: sanity check, one sequence, more than 4 mins difference to event length. Event  length %i length of cut out of recording %i filename \"%s\"", EventDuration(), durationInSeconds, m_recording->FileName() );
    } else {
// event duration is not available, cut recording should be recording - timer margin at start / stop
      if (DurationWithoutMarginSec() < durationInSeconds) m_durationInSecMarks = durationInSeconds;
      else isyslog("tvscraper: GetDurationInSecMarks: sanity check, one sequence, more than 20 mins cut. Recording length %i length of cut out of recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, m_recording->FileName() );
    }
    if (m_durationInSecMarks != -1 && config.enableDebug) isyslog("tvscraper: GetDurationInSecMarks: sanity check ok, one sequence, Recording length %i length of cut out of recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, m_recording->FileName() );
    return m_durationInSecMarks;
  }

// more than one sequence, ads found
// guess a reasonable min length
  if (durationInSeconds < DurationLowSec() ) {
    isyslog("tvscraper: GetDurationInSecMarks: sanity check, too much cut out of recording. Recording length %i length of cut recording %i expected min length of cut recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, DurationLowSec(), m_recording->FileName() );
    return m_durationInSecMarks;
  }

  if (config.enableDebug) isyslog("tvscraper: GetDurationInSecMarks: sanity check ok, Recording length %i length of cut out of recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, m_recording->FileName() );

  m_durationInSecMarks = durationInSeconds;
  return m_durationInSecMarks;
}

template<class TV=cSv, class C_IT=const char*>
int markad_offset(const_split_iterator<TV, C_IT> &col, cStr filename, cSv line) {
  if (++col == iterator_end() ) {
    esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", offset not found", filename.c_str(), (int)line.length(), line.data());
    return -3;
  }
  if (++col == iterator_end() ) {
    esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", offset not found (2)", filename.c_str(), (int)line.length(), line.data());
    return -3;
  }
  return lexical_cast<int>(*col, -3, "offset markad.vps");
}
void csRecording::readMarkadVps() const {
// set m_markad_vps_used, m_markad_vps_length and m_markad_vps_start

  if (m_markad_vps_length > -4) return; // was already called, data available
  m_markad_vps_length = -3;  // checked, but unknown
  m_markad_vps_used = -3;
  m_markad_vps_start = -3;

  cToSvConcat filename(m_recording->FileName(), "/markad.vps");
  cToSvFile markad_vps(filename);

  int pause_start = -3;
  int last_start = -3;
  int length = 0;
  for (cSv line: const_split_iterator(cSv(markad_vps), '\n')) {
    if (line.empty() ) continue; // ignore empty lines
    m_markad_vps_length = -1;
    if (line == "VPSTIMER=NO") { m_markad_vps_used = 0; continue; }
    if (line == "VPSTIMER=YES") { m_markad_vps_used = 1; continue; }
// Note: There are old versions of markad.vps without this line
    const_split_iterator col(line, ' ');
    if (*col == "START:") {
      if (m_markad_vps_start >= 0)
        esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", two or more times keyword START:", filename.c_str(), (int)line.length(), line.data());
      m_markad_vps_start = markad_offset(col, filename, line);
      if (m_markad_vps_start == -3) return; // error in convert already reported
      last_start = m_markad_vps_start;
      continue;
    }
    if (m_markad_vps_start < 0) {
      esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", keyword START: missing as first keyword", filename.c_str(), (int)line.length(), line.data());
      return;
    }
    if (*col == "PAUSE_START:") {
      if (pause_start != -3) {
        esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", keyword PAUSE_START: 2 times, without PAUSE_STOP", filename.c_str(), (int)line.length(), line.data());
        return;
      }
      pause_start = markad_offset(col, filename, line);
      if (pause_start == -3) return; // error in convert already reported
      length += (pause_start - last_start);
      continue;
    }
    if (*col == "PAUSE_STOP:") {
      if (pause_start == -3) {
        esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", keyword PAUSE_STOP: without preceeding PAUSE_START", filename.c_str(), (int)line.length(), line.data());
        return;
      }
      int pause_stop = markad_offset(col, filename, line);
      if (pause_stop == -3) return; // error in convert already reported
      last_start = pause_stop;
      pause_start = -3;
      continue;
    }
    if (*col == "STOP:") {
      if (pause_start != -3) {
        esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", keyword STOP: after PAUSE_START", filename.c_str(), (int)line.length(), line.data());
        return;
      }
      int vps_stop = markad_offset(col, filename, line);
      if (vps_stop == -3) return; // error in convert already reported
      length += (vps_stop - last_start);
      m_markad_vps_length = length;
      return;
    }
    esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", unknown keyword", filename.c_str(), (int)line.length(), line.data());
  }
  if (m_markad_vps_start >= 0)
    esyslog("tvscraper: ERROR parsing %s, EOF, keyword STOP: missing", filename.c_str());
}

bool csRecording::getTvscraperTimerInfo(bool &vps, int &lengthInSeconds) {
// return false if no info is available
  CONCATENATE(filename_old, m_recording->FileName(), "/tvscrapper.json");
  CONCATENATE(filename_new, m_recording->FileName(), "/tvscraper.json");
  const char *filename = filename_old;
  struct stat buffer;
  if (stat (filename, &buffer) != 0) filename = filename_new;

  cJsonDocumentFromFile document(filename);
  if (document.HasParseError() ) return false;
  rapidjson::Value::ConstMemberIterator timer_j = document.FindMember("timer");
  if (timer_j == document.MemberEnd() ) return false;  // timer information not available

  if (!getValue(timer_j->value, "vps", vps, filename ) ) return false;
  time_t start, stop;
  if (!getValue(timer_j->value, "start_time", start, filename ) ) return false;
  if (!getValue(timer_j->value, "stop_time", stop, filename ) ) return false;
  lengthInSeconds = stop - start;
  return true;
}

bool csRecording::getEpgsearchTimerInfo(bool &vps, int &lengthInSeconds) {
// false: No info available
// otherwise: Epgsearch was used, timer length
  if (!m_recording->Info()->Aux() ) return false;
  cSv epgsearchAux = partInXmlTag(m_recording->Info()->Aux(), "epgsearch");
  if (epgsearchAux.empty()) return false;
  cSv epgsearchStart = partInXmlTag(epgsearchAux, "start");
  if (epgsearchStart.empty()) return false;
  cSv epgsearchStop = partInXmlTag(epgsearchAux, "stop");
  if (epgsearchStop.empty()) return false;
  time_t start = parse_unsigned<time_t>(epgsearchStart);
  time_t stop  = parse_unsigned<time_t>(epgsearchStop);
  lengthInSeconds = stop - start;
  vps = (start == m_recording->Info()->GetEvent()->StartTime() ) &&
        (lengthInSeconds == m_recording->Info()->GetEvent()->Duration() );
  return true;
}
int csRecording::durationDeviationNoVps() {
// vps was NOT used.
  if (m_debug) dsyslog("tvscraper: csRecording::durationDeviationNoVps");
  bool vps;
  int lengthInSeconds;
  if (getTvscraperTimerInfo(vps, lengthInSeconds) ) {
    if (vps) esyslog("tvscraper: ERROR, inconsistent VPS data in %s", m_recording->FileName() );
    if (m_debug) esyslog("tvscraper: csRecording::durationDeviationNoVps, getTvscraperTimerInfo lengthInSeconds = %d, m_recording->LengthInSeconds() = %d", lengthInSeconds, m_recording->LengthInSeconds());
    return std::max(0, lengthInSeconds - m_recording->LengthInSeconds());
  } else if (getEpgsearchTimerInfo(vps, lengthInSeconds) ) {
    if (vps) esyslog("tvscraper: ERROR, inconsistent VPS data (epgsearch) in %s", m_recording->FileName() );
    return std::max(0, lengthInSeconds - m_recording->LengthInSeconds());
  } else
    return std::max(0, m_recording->Info()->GetEvent()->Duration() + (::Setup.MarginStart + ::Setup.MarginStop) * 60 - m_recording->LengthInSeconds());
}
int csRecording::durationDeviationVps(int s_runtime, bool markadMissingTimes) {
// VPS was used, but we don't have the VPS start / end mark
  int deviation = m_recording->Info()->GetEvent()->Duration() - m_recording->LengthInSeconds();
  if (deviation <= 0) return 0; // recording is longer than event duration -> no error
  if (!markadMissingTimes && deviation <= 6*60) return 0; // we assume VPS was used, und report only huge deviations > 6min. Except if markad found VPS, but has no VPS times. This indicates that the VPS start event was detected too late
  if (s_runtime <= 0) return deviation;
  return std::min(deviation, std::abs(s_runtime*60 - m_recording->LengthInSeconds()));
}
int csRecording::durationDeviation(int s_runtime) {
// return deviation between actual recording length, and planned duration (timer / vps)
// return -1 if no information is available
  if (!m_recording || !m_recording->FileName() || !m_recording->Info() || !m_recording->Info()->GetEvent() ) return -1;
  if (m_recording->IsEdited() || cSv(m_recording->Info()->Aux()).find("<isEdited>true</isEdited>") != std::string_view::npos) return 0;  // we assume, who ever edited the recording checked for completeness
  int inUse = m_recording->IsInUse();
  if ((inUse != ruNone) && (inUse != ruReplay)) return -1;  // still recording => incomplete, this check is useless
  if (m_debug) dsyslog("tvscraper: csRecording::durationDeviation 1, s_runtime %d", s_runtime);
  if (!m_recording->Info()->GetEvent()->Vps() ) return durationDeviationNoVps();
  if (m_debug) dsyslog("tvscraper: csRecording::durationDeviation event has VPS, s_runtime %d", s_runtime);
// event has VPS. Was VPS used?
  readMarkadVps();
  m_vps_used = m_markad_vps_used;
  int lengthInSeconds = -3;
  if (m_vps_used == -3) {
    bool vps;
    if (getTvscraperTimerInfo(vps, lengthInSeconds) ) {
// information in tvscraper.json is available
      if (vps) m_vps_used = 1;
      else m_vps_used = 0;
    }
  }
  if (m_vps_used == -3) {
    bool vps;
    if (getEpgsearchTimerInfo(vps, lengthInSeconds) ) {
      if (vps) m_vps_used = 1;
      else m_vps_used = 0;
    }
  }
  if (m_vps_used == 0) {
// no VPS used
    if (lengthInSeconds != -3) return std::max(0, lengthInSeconds - m_recording->LengthInSeconds());
    return durationDeviationNoVps();
  }
  if (m_vps_used == 1) {
// VPS used
    if (m_markad_vps_length > 0) return std::max(0, m_markad_vps_start + m_markad_vps_length - m_recording->LengthInSeconds() );
    return durationDeviationVps(s_runtime, m_markad_vps_used == 1);
  }

// still unclear whether VPS was used, no final inforamtion is available. Guess
  int deviationNoVps = m_recording->Info()->GetEvent()->Duration() + (::Setup.MarginStart + ::Setup.MarginStop) * 60 - m_recording->LengthInSeconds();
  int deviationVps = m_recording->Info()->GetEvent()->Duration() - m_recording->LengthInSeconds();
  if (std::abs(deviationVps) < std::abs(deviationNoVps)) return durationDeviationVps(s_runtime);
  return std::max(0, deviationNoVps);
}

csEventOrRecording *GetsEventOrRecording(const cEvent *event, const cRecording *recording) {
  if (event) return new csEventOrRecording(event);
  if (recording && recording->Info() && recording->Info()->GetEvent() ) return new csRecording(recording);
  return NULL;
}

int GetNumberOfTsFiles(const cRecording* recording) {
// find our number of ts files
  if (!recording || !recording->FileName() ) return -1;
  DIR *dir = opendir(recording->FileName());
  if (dir == nullptr) return -1;
  struct dirent *ent;
  int number_ts_files = 0;
  while ((ent = readdir (dir)) != NULL)
// POSIX defines it as char d_name[], a character array of unspecified
//       size, with at most NAME_MAX characters preceding the terminating null byte ('\0').
    if (strlen(ent->d_name) == 8 && strcmp(ent->d_name + 5, ".ts") == 0) {
      bool only_digits = true;
      for (int i = 0; i < 5; i++) if (ent->d_name[i] < '0' || ent->d_name[i] > '9') only_digits = false;
      if (only_digits) ++number_ts_files;
  }
  closedir (dir);
  return number_ts_files;
}


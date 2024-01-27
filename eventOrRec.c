#include "eventOrRec.h"
#include <dirent.h>

csEventOrRecording::csEventOrRecording(const cEvent *event):
  m_event(event)
{
}
void csEventOrRecording::AddYears(cYears &years) const {
  years.addYears(Description() );
  years.addYears(ShortText() );
  years.addYears(Title() );
}
int csEventOrRecording::DurationDistance(int DurationInMin) {
  int durationInMinLow, durationInMinHigh;
  if (DurationInMin <=0 || !DurationRange(durationInMinLow, durationInMinHigh) ) return -2; // no data
  if (DurationInMin > durationInMinHigh) return DurationInMin - durationInMinHigh;
  if (DurationInMin < durationInMinLow)  return durationInMinLow - DurationInMin;
  return 0;
}
bool csEventOrRecording::DurationRange(int &durationInMinLow, int &durationInMinHigh) {
// return true, if data is available
  if (!m_event->Duration() ) return false;
  durationInMinLow  = DurationLowSec() / 60 - 1;
  durationInMinHigh = DurationHighSec() / 60 + (10 * DurationHighSec())  / 60 / 60;  // add 10 mins for 60 min duration, more for longer durations. This is because often a cut version of the movie is broadcasted
//  if (Recording() && Title() && strcmp("The Expendables 3", Title() ) == 0) 
//  esyslog("tvscraper: csEventOrRecording::DurationRange, title = %s, durationInMinLow  = %i,  durationInMinHigh = %i", Title(), durationInMinLow, durationInMinHigh);
  return true;
}

cSv csEventOrRecording::EpisodeSearchString() const {
  if(ShortText() && *ShortText() ) return ShortText();
  if(Description() ) {
    if (strlen(Description() ) <= 100) return Description();
    else return cSv(Description(), 100);
  }
  return cSv();
}
 
std::string csEventOrRecording::ChannelName() const {
 tChannelID channelID = ChannelID();
 if (!channelID.Valid() ) {
   esyslog("tvscraper, csEventOrRecording::ChannelName, invalid channel ID, title %s", Title() );
   return m_unknownChannel;
 }

#if APIVERSNUM < 20301
  const cChannel *channel = Channels.GetByChannelID(channelID);
#else
  LOCK_CHANNELS_READ;
  const cChannel *channel = Channels->GetByChannelID(channelID);
#endif
  if (!channel) {
    esyslog("tvscraper, csEventOrRecording::ChannelName, no channel for channel ID %s, title %s", ChannelIDs().c_str(), Title() );
    return m_unknownChannel;
  }
  return channel->Name();
}

// Recording

csRecording::csRecording(const cRecording *recording) :
  csEventOrRecording((recording && recording->Info())?recording->Info()->GetEvent():NULL),
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
}

bool csRecording::DurationRange(int &durationInMinLow, int &durationInMinHigh) {
// return true, if data is available
  if (!m_recording->FileName() ) return false;
  if (!m_event->Duration() && !m_recording->LengthInSeconds() ) return false;

  if (m_recording->IsEdited() || DurationInSecMarks() != -1) {
// length of recording without adds
    int durationInSec = m_recording->IsEdited()?m_recording->LengthInSeconds():DurationInSecMarks();
    durationInMinLow  = durationInSec / 60 - 2;  // 2 min for inaccuracies
    durationInMinHigh = durationInSec / 60 +  (15 * durationInSec)  / 60 / 60;  // add 15 mins for 60 min duration, more for longer durations. This is because often a cut version of the movie is broadcasted. Also, the markad results are not really reliable
    return true;
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
    if (m_event->Duration() ) {
// event duration is available, and should be equal to duration of cut recording
      if (abs(m_event->Duration() - durationInSeconds) < 4*60) m_durationInSecMarks = durationInSeconds;
        else esyslog("tvscraper: GetDurationInSecMarks: sanity check, one sequence, more than 4 mins difference to event length. Event  length %i length of cut out of recording %i filename \"%s\"", m_event->Duration(), durationInSeconds, m_recording->FileName() );
    } else {
// event duration is not available, cut recording sould be recording - timer margin at start / stop
      if (DurationWithoutMarginSec() < durationInSeconds) m_durationInSecMarks = durationInSeconds;
    else esyslog("tvscraper: GetDurationInSecMarks: sanity check, one sequence, more than 20 mins cut. Recording length %i length of cut out of recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, m_recording->FileName() );
    }
    if (m_durationInSecMarks != -1 && config.enableDebug) esyslog("tvscraper: GetDurationInSecMarks: sanity check ok, one sequence, Recording length %i length of cut out of recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, m_recording->FileName() );
    return m_durationInSecMarks;
  }

// more than one sequence, ads found
// guess a reasonable min length
  if (durationInSeconds < DurationLowSec() ) {
    esyslog("tvscraper: GetDurationInSecMarks: sanity check, too much cut out of recording. Recording length %i length of cut recording %i expected min length of cut recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, DurationLowSec(), m_recording->FileName() );
    return m_durationInSecMarks;
  }

  if (config.enableDebug) esyslog("tvscraper: GetDurationInSecMarks: sanity check ok, Recording length %i length of cut out of recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, m_recording->FileName() );

  m_durationInSecMarks = durationInSeconds;
  return m_durationInSecMarks;
}

int parseMarkadVpsKeyword(std::ifstream &markad, const char *fname) {
// return 0: EOF, or error
// 1 start
// 2 pause start
// 3 pause stop
// 4 stop
  if (!markad) return 0;
  std::string line;
  markad >> line;
  if (line.empty() ) return 0;
  if (line.compare(0, 6,  "START:", 6) == 0) return 1;
  if (line.compare(0, 5,  "STOP:", 5) == 0) return 4;
  if (line.compare(0, 5,  "PAUSE"  , 5) != 0) {
    esyslog("tvscraper: ERROR parsing markad.vps, line = %s, file = %s", line.c_str(), fname );
    return 0;
  }
// check PAUSE_START: / PAUSE_STOP:
  if (line.compare(5, 7,  "_START:") == 0) return 2;
  if (line.compare(5, 6,  "_STOP:") == 0) return 3;
// check PAUSE START: / PAUSE STOP:
  line = "";
  markad >> line;
  if (line.empty() ) {
    esyslog("tvscraper: ERROR parsing markad.vps, line empty after PAUSE, file = %s", fname);
    return 0;
  }
  if (line.compare(0, 6,  "START:", 6) == 0) return 2;
  if (line.compare(0, 5,  "STOP:", 5) == 0) return 3;
  esyslog("tvscraper: ERROR parsing markad.vps, line after PAUSE = %s, file = %s", line.c_str(), fname );
  return 0;
}
int parseMarkadVps(std::ifstream &markad, int &time, const char *fname) {
// return 0: EOF, or error
// 1 start
// 2 pause start
// 3 pause stop
// 4 stop
// time: timestamp in sec (since recording start)
  time = -1;
  int keyword = parseMarkadVpsKeyword(markad, fname);
  if (keyword == 0) return 0;
  if (!markad) {
    esyslog("tvscraper: ERROR parsing markad.vps, EOF after keyword = %d, file = %s", keyword, fname);
    return 0;
  }
  std::string line;
  markad >> line;
  if (line.empty() ) {
    esyslog("tvscraper: ERROR parsing markad.vps, empty line after keyword = %d, file = %s", keyword, fname);
    return 0;
  }
  if (!markad) {
    esyslog("tvscraper: ERROR parsing markad.vps, EOF after keyword = %d and line %s, file = %s", keyword, line.c_str() , fname);
    return 0;
  }
  markad >> time;
  return keyword;
}

int csRecording::getVpsLength() {
// -4: not checked.  (will not be returned)
// -3: markad.vps not available or wrong format
// -2: VPS not used.
// -1: VPS used, but no time available
// >0: VPS length in seconds
  if (m_vps_length > -4) return m_vps_length;
  CONCATENATE(filename, m_recording->FileName(), "/markad.vps");
  struct stat buffer;
  if (stat (filename, &buffer) != 0) { m_vps_length = -3; return m_vps_length; }

  std::ifstream markad(filename);
  if (!markad) { m_vps_length = -3; return m_vps_length; }
  std::string l1;
  markad >> l1;
  if (l1 == "VPSTIMER=NO") { m_vps_length = -2; return m_vps_length; }
  if (l1 != "VPSTIMER=YES") {
//    esyslog("tvscraper: ERROR: markad.vps, wrong format, first line=%s, path %s", l1.c_str(), m_recording->FileName());
// remove this error. There are old versions of markad.vps without this line
    m_vps_length = -3;
    return m_vps_length;
  }
  m_vps_length = -1;  // VPS was used. Check the length
  if (!markad) return m_vps_length;
  int time_start_sequence, time_end_sequence;
  int keyword = parseMarkadVps(markad, time_start_sequence, m_recording->FileName());
  if (keyword == 0) return m_vps_length;
  if (keyword != 1) {
    esyslog("tvscraper: ERROR: markad.vps, first keyword = %d, path %s", keyword, m_recording->FileName());
    return m_vps_length;
  }
  m_vps_start = time_start_sequence;
  for (int cur_length = 0;;) {
    keyword = parseMarkadVps(markad, time_end_sequence, m_recording->FileName());
    if (keyword != 4 && keyword != 2) {
      esyslog("tvscraper: ERROR: markad.vps, keyword %d after start sequence, path %s", keyword, m_recording->FileName());
      return m_vps_length;
    }
    if (time_start_sequence >= time_end_sequence) {
      esyslog("tvscraper: ERROR:time_start_sequence %d > time_end_sequence %d, keyword %d,  path %s", time_start_sequence, time_end_sequence, keyword, m_recording->FileName());
      return m_vps_length;
    }
    cur_length += (time_end_sequence - time_start_sequence);
    if (keyword == 4) {
      m_vps_length = cur_length;
      return m_vps_length;
    }
    keyword = parseMarkadVps(markad, time_start_sequence, m_recording->FileName());
    if (keyword != 3) {
      esyslog("tvscraper: ERROR: markad.vps, keyword %d after pause start sequence, path %s", keyword, m_recording->FileName());
      return m_vps_length;
    }
  }
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
  bool vps;
  int lengthInSeconds;
  if (getTvscraperTimerInfo(vps, lengthInSeconds) ) {
    if (vps) esyslog("tvscraper: ERROR, inconsistent VPS data in %s", m_recording->FileName() );
    return std::max(0, lengthInSeconds - m_recording->LengthInSeconds());
  } else if (getEpgsearchTimerInfo(vps, lengthInSeconds) ) {
    if (vps) esyslog("tvscraper: ERROR, inconsistent VPS data (epgsearch) in %s", m_recording->FileName() );
    return std::max(0, lengthInSeconds - m_recording->LengthInSeconds());
  } else
    return std::max(0, m_recording->Info()->GetEvent()->Duration() + (::Setup.MarginStart + ::Setup.MarginStop) * 60 - m_recording->LengthInSeconds());
}
int csRecording::durationDeviationVps(int s_runtime) {
// VPS was used, but we don't have the VPS start / end mark
  int deviation = m_recording->Info()->GetEvent()->Duration() - m_recording->LengthInSeconds();
  if (deviation <= 0) return 0; // recording is longer than event duration -> no error
  if (deviation <= 6*60) return 0; // we assume VPS was used, und report only huge deviations > 6min
  if (s_runtime <= 0) return deviation;
  return std::min(deviation, std::abs(s_runtime*60 - m_recording->LengthInSeconds()));
}
int csRecording::durationDeviation(int s_runtime) {
// return deviation between actual reording length, and planned duration (timer / vps)
// return -1 if no information is available
  if (!m_recording || !m_recording->FileName() || !m_recording->Info() || !m_recording->Info()->GetEvent() ) return -1;
  if (m_recording->IsEdited() ) return 0;  // we assume, who ever edited the recording checked for completeness
  int inUse = m_recording->IsInUse();
  if ((inUse != ruNone) && (inUse != ruReplay)) return -1;  // still recording => incomplete, this check is useless
  if (!m_recording->Info()->GetEvent()->Vps() ) return durationDeviationNoVps();
// event has VPS. Was VPS used?
  int vps_used_markad = getVpsLength();
  if (vps_used_markad == -2) return durationDeviationNoVps();
  if (vps_used_markad >=  0) return std::abs(vps_used_markad + m_vps_start - m_recording->LengthInSeconds() );
  if (vps_used_markad == -1) return durationDeviationVps(s_runtime);
// still unclear whether VPS was used
  bool vps;
  int lengthInSeconds;
  if (getTvscraperTimerInfo(vps, lengthInSeconds) ) {
// information in tvscraper.json is available
    if(!vps) return std::max(0, lengthInSeconds - m_recording->LengthInSeconds());
// vps was used, but the VPS start/stop marks are missing
    return durationDeviationVps(s_runtime);
  }
  if (getEpgsearchTimerInfo(vps, lengthInSeconds) ) {
// epgsearch was used, and information in aux is available
    if(!vps) return std::max(0, lengthInSeconds - m_recording->LengthInSeconds());
// vps was used, but the VPS start/stop marks are missing
    return durationDeviationVps(s_runtime);
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
    if (ent->d_name && strlen(ent->d_name) == 8 && strcmp(ent->d_name + 5, ".ts") == 0) {
      bool only_digits = true;
      for (int i = 0; i < 5; i++) if (ent->d_name[i] < '0' || ent->d_name[i] > '9') only_digits = false;
      if (only_digits) ++number_ts_files;
  }
  closedir (dir);
  return number_ts_files;
}


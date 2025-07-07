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
  if (EventDuration() == 0 && RecordingId() == -1) return -2;
  durationInMinLow  = DurationLowSec() / 60 - 1;
  durationInMinHigh = (DurationHighSec() + (10 * DurationHighSec())/60 ) / 60;  // add 10 mins for 60 min duration, more for longer durations. This is because often a cut version of the movie is broadcasted
  if (m_debug)
    dsyslog("tvscraper: csEventOrRecording::DurationRange, title = %s, durationInMinLow  = %i,  durationInMinHigh = %i", Title(), durationInMinLow, durationInMinHigh);
  return 0;
}

cSv csEventOrRecording::EpisodeSearchString() const {
  if(ShortText() && *ShortText() ) return ShortText();
  return Description().substr(0, 100);
}
 
// ============================================================
// ========================  Recording ========================
// ============================================================
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
/*
  if (cSv(recording->Info()->Title()) == "Das Familiengericht") m_debug = true;
  if (cSv(recording->Info()->Title()) == "Star Trek - Deep Space Nine") m_debug = true;
  if (cSv(recording->Info()->Title()) == "Konferenz der Tiere") m_debug = true;
*/
  if (m_debug) dsyslog2("debug recording ", recording->Info()->Title());
}
/* Consistency checks
note: use m_recording->Start() as timer start time
  m_recording->Start() == epgsearch_start == tvscraper_start. Otherwise, report isyslog
  m_recording->Start() + epgsearch_bstart == event->StartTime(). Otherwise, report isyslog

  m_timer_stop == m_tvscraper_stop == m_epgsearch_stop == event->StopTime() + epgsearch_bstop
    -> calculate from whatever data we have, left has more priority. Report inconsistencies in isyslog

==== VPS used, and consistency
if epgsearch_start is available:  m_epgsearch_vps = EventVps() == start && m_epgsearch_length == m_event->Duration();
if epgsearch_bstart is available: m_epgsearch_vps = bstart == 0 && bstop == 0

if (!event-Vps() ):   (event has no VPS)
  markad_vps, tvscraper_vps, m_epgsearch_vps must not be true

vps_used == tvscraper_vps == markad_vps == m_epgsearch_vps
    -> calculate from whatever data we have, left has more priority. Report inconsistencies in isyslog

last resort: vps_used = std::abs(deviationVps) < std::abs(deviationNoVps);

==== VPS recording
// note: VDR ignores PAUSE_START / PAUSE_STOP vps events
  m_recording->LengthInSeconds() == m_markad_vps_stop(+15 secs in newer VDR versions)   -> duration deviation
if markad failed to collect VPS information: we assume a missing event status "recording starts in few secs".
  m_recording->LengthInSeconds() == event->length()  -> report small deviations as duration deviation, without this event status a relyable VPS rec seems not to be possible
if no markad_vps information at all (markad did not try to get VPS events for this recording) -> markad.vps is missing:
  note: we cannot really check here
  m_recording->LengthInSeconds() == event->length()  -> report only large deviations, also check runtime of assigned movie


==== not a VPS recording
m_recording->Start() == CreationTimestampOfFirst*ts File
m_recording->Start() + m_recording->LengthInSeconds() == LastChangeOfLast*ts File;  -> otherwise, rec was interupted

if bstart & bstop available:
  m_recording->Start() + epgsearch_bstart == event->StartTime()   //  if bstart is available
  event->StopTime() + epgsearch_bstop == lastChangeOfLast*ts File //  if bstop  is available  -> otherwise, last  part of rec is missing

if m_timer_stop avialable:
  deviation = (m_timer_stop - m_timer_start) - m_recording->LengthInSeconds()
    < 0: dsyslog, but return 0 (ingnore small deviations < 5 sec); >= 0: return deviation
  LastChangeOfLast*ts File == m_timer_stop   // if m_timer_stop  is available -> otherwise, last  part of rec is missing
  m_recording->Start()     == m_timer_start  // if m_timer_start is available -> otherwise, first part of rec is missing


==== marks sanity check min length:
: VPS recording (VPS used)
  VPS length if availabe -> use this as upper & lower limit
  otherwise:
  lower limit = m_recording->LengthInSeconds() - ads

: non VPS recording
  lower limit: m_recording->LengthInSeconds() - max(0, margin()) - ads

==== min length / max lenght for matching of extenal db entry:
if recording is cut:   use m_recording->LengthInSeconds() as lower limit, + buffer as upper limit (buffer because stations don't send complete movie in full length)
if recording used VPS: use m_recording->LengthInSeconds() as lower limit, + buffer as upper limit
if marks available and sane: use marks-> length()         as lower limit, + buffer as upper limit
*/


void csRecording::read_ext_data() const {
/*
read and validate:
for int:    s_int_not_available    -> no data available
for time_t: s_time_t_not_available -> no data available
-- int m_markad_vps_start
-- int m_markad_vps_length
-- int m_epgsearch_bstart
-- int m_epgsearch_bstop
- calculated:
-- bool m_vps_used  -> will be always availabe, guessed in worst case
-- time_t m_timer_stop
*/

  if (m_ext_data_read) return;  // already done ...
  m_ext_data_read = true;
  readTvscraper();
  readMarkadVps();
  readEpgsearch();
  readTsFileTimestamps();

// =============================================================================================================
// vps used?, including consistency check
// we start with this, as other checks depend on m_vps_used
// =============================================================================================================
  int epgsearch_vps_used = s_int_not_available;
  if (m_epgsearch_start != s_time_t_not_available)
    epgsearch_vps_used = (EventVps() == m_epgsearch_start && (m_epgsearch_stop-m_epgsearch_start) == m_event->Duration())?1:0;
  if (m_epgsearch_bstart != s_int_not_available)
    epgsearch_vps_used = (m_epgsearch_bstart == 0 && m_epgsearch_bstop == 0)?1:0;

  if (m_debug) dsyslog("tvscraper, DEBUG %s, epgsearch_vps_used %s, m_epgsearch_start %lld, m_epgsearch_stop %lld, m_epgsearch_bstart %d, m_epgsearch_bstop %d",
     m_recording->FileName(), epgsearch_vps_used==1?"true":epgsearch_vps_used==0?"false":"not available",
     (long long)m_epgsearch_start, (long long)m_epgsearch_stop, m_epgsearch_bstart, m_epgsearch_bstop);

  if (EventVps() ) {
    if (m_tvscraper_vps_used != s_int_not_available) {
      m_vps_used = m_tvscraper_vps_used==1;
      if (m_markad_vps_used != s_int_not_available && m_tvscraper_vps_used != m_markad_vps_used)
        isyslog("tvscraper, recording %s vps used inconsistent, m_tvscraper_vps_used %d, m_markad_vps_used %d", m_recording->FileName(), m_tvscraper_vps_used, m_markad_vps_used);
      if (epgsearch_vps_used != s_int_not_available && m_tvscraper_vps_used != epgsearch_vps_used)
        isyslog("tvscraper, recording %s vps used inconsistent, m_tvscraper_vps_used %d, epgsearch_vps_used %d", m_recording->FileName(), m_tvscraper_vps_used, epgsearch_vps_used);
    } else if (m_markad_vps_used != s_int_not_available) {
      m_vps_used = m_markad_vps_used==1;
      if (epgsearch_vps_used != s_int_not_available && m_markad_vps_used != epgsearch_vps_used)
        isyslog("tvscraper, recording %s vps used inconsistent, m_markad_vps_used %d, epgsearch_vps_used %d", m_recording->FileName(), m_markad_vps_used, epgsearch_vps_used);
    } else if (epgsearch_vps_used != s_int_not_available) {
      m_vps_used = epgsearch_vps_used==1;
    } else {
      int deviationVps = m_event->Duration() - m_recording->LengthInSeconds();
      int deviationNoVps = (int)(m_event->StartTime() + m_event->Duration() + ::Setup.MarginStop*60 - m_recording->Start()) - m_recording->LengthInSeconds();
      m_vps_used = std::abs(deviationVps) < std::abs(deviationNoVps);
    }
  } else {
// !EventVps()
    m_vps_used = false;
    if (m_tvscraper_vps_used==1 || m_markad_vps_used==1 || epgsearch_vps_used==1)
      isyslog("tvscraper, recording %s vps used inconsistent, !EventVps(), m_tvscraper_vps_used %d, m_markad_vps_used %d, epgsearch_vps_used %d", m_recording->FileName(), m_tvscraper_vps_used, m_markad_vps_used, epgsearch_vps_used);
  }

// =============================================================================================================
// get m_timer_stop, consistency check: tvscraper start / stop, epgsearch start / stop, m_recording->Start()
// =============================================================================================================
  m_timer_stop = s_time_t_not_available;
  if (m_tvscraper_start != s_time_t_not_available) {
// tvscraper start / stop are available
    if (m_tvscraper_start != m_recording->Start() )
      isyslog("tvscraper, recording %s timer start inconsistent, m_tvscraper_start %lld, m_recording->Start() %lld, m_tvscraper_vps_used %d", m_recording->FileName(), (long long)m_tvscraper_start, (long long)m_recording->Start(), m_tvscraper_vps_used);
// for vps timers, VDR returns event->Start as start time. This is also used as recording->Start(), but recording->Start() rounds to minutes. So we have to round m_tvscraper_start before the comparison
    m_timer_stop = m_tvscraper_stop;
  }

  if (m_epgsearch_start != s_time_t_not_available && !m_vps_used) {
// epgsearch start / stop are available
//  For VPS timers, m_epgsearch_start == EventVps() and m_epgsearch_stop = EventVps() + event->Duration()
//  For VPS timers, m_recording->Start() == roundMinutes(event->Start()). And event->Start() and EventVps() might be identical, or not ...
    if (m_epgsearch_start != m_recording->Start() )
      isyslog("tvscraper, recording %s timer start inconsistent, m_epgsearch_start %lld, m_recording->Start() %lld", m_recording->FileName(), (long long)m_epgsearch_start, (long long)m_recording->Start());
    if (m_timer_stop == s_time_t_not_available) m_timer_stop = m_epgsearch_stop;
    else if (m_timer_stop != m_epgsearch_stop)
      dsyslog("tvscraper DEBUG, recording %s timer stop inconsistent, m_tvscraper_stop %lld, m_epgsearch_stop %lld", m_recording->FileName(), (long long)m_tvscraper_stop, (long long)m_epgsearch_stop);
  }

  if (m_epgsearch_bstart != s_int_not_available && !m_vps_used) {
// epgsearch bstart / bstop are available
// for VPS timers, m_epgsearch_bstart == 0 && m_epgsearch_bstop == 0
    if (roundMinutes(m_event->StartTime() - m_epgsearch_bstart) != m_recording->Start() )
      dsyslog("tvscraper DEBUG, recording %s start inconsistent, m_event->StartTime() %lld, m_recording->Start() %lld, m_epgsearch_bstart %d", m_recording->FileName(), (long long)m_event->StartTime(), (long long)m_recording->Start(), m_epgsearch_bstart);
    if (m_timer_stop == s_time_t_not_available) m_timer_stop = roundMinutes(m_event->StartTime() + m_event->Duration() + m_epgsearch_bstop);
    else if (m_timer_stop != roundMinutes(m_event->StartTime() + m_event->Duration() + m_epgsearch_bstop))
      dsyslog("tvscraper DEBUG, recording %s timer stop inconsistent, m_timer_stop %lld, m_event->EndTime() %lld, m_epgsearch_bstop %d", m_recording->FileName(), (long long)m_timer_stop, (long long)(m_event->StartTime()+m_event->Duration()), m_epgsearch_bstop);
  }
  if (m_vps_used && m_timer_stop == s_time_t_not_available) m_timer_stop = roundMinutes(m_event->StartTime() + m_event->Duration());

// =============================================================================================================
// print result, if we debug
  if (m_debug) dsyslog("tvscraper, DEBUG %s, m_vps_used %s, m_recording->Start() %lld, m_timer_stop %lld, m_event->Duration() %d, m_recording->LengthInSeconds() %d",
     m_recording->FileName(), m_vps_used?"true":"false", (long long)m_recording->Start(), (long long)m_timer_stop,
     m_event->Duration(), m_recording->LengthInSeconds() );
}

bool csRecording::recordingLengthIsChanging() const {
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
  if (recordingLengthIsChanging() ) return -1;
  if (!m_recording->FileName() || !m_recording->LengthInSeconds() ) return -2;

  int durationInSec = -4;
  if (IsEdited() ) durationInSec = m_recording->LengthInSeconds();
  if (durationInSec <= 0) {
    read_ext_data();
    if (m_markad_vps_length != s_int_not_available) durationInSec = m_markad_vps_length;
  }
  if (durationInSec <= 0) durationInSec = DurationInSecMarks();

  if (durationInSec >  0) {
// length of recording without adds
    durationInMinLow  = durationInSec / 60 - 2;  // 2 min for inaccuracies
    durationInMinHigh = durationInSec / 60 +  (15 * durationInSec)  / 60 / 60;  // add 15 mins for 60 min duration, more for longer durations. This is because often a cut version of the movie is broadcasted. Also, the markad results are not always reliable
    return 0;
  }
  return csEventOrRecording::DurationRange(durationInMinLow, durationInMinHigh);
}

int csRecording::DurationInSecMarks_int(void) {
// return -1 if no data is available
// otherwise, duration of cut recording
// also set m_durationInSecMarks to the returned value
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

// ====  calculation finished, sanity checks: ========================================
  if (m_recording->LengthInSeconds() < durationInSeconds) {
    esyslog("tvscraper: ERROR: recording length shorter than length of cut recording, recording length %i length of cut recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, m_recording->FileName() );
    return m_durationInSecMarks;
  }

  if (numSequences == 1) {
// no adds found, just recording margin at start / stop
    if (EventDuration() ) {
// event duration is available, and should be equal to duration of cut recording
      if (durationInSeconds + 4*60 > std::min(EventDuration(), m_recording->LengthInSeconds()) ) m_durationInSecMarks = durationInSeconds;
      else dsyslog3("Sanity check, one sequence, more than 4 mins difference to event length. Event length ", EventDuration(), " length of cut out of recording ", durationInSeconds, " filename \"", m_recording->FileName(), "\"");
    } else {
// event duration is not available, cut recording should be recording - timer margin at start / stop
      if (DurationWithoutMarginSec() <= durationInSeconds + 60) m_durationInSecMarks = durationInSeconds;
      else dsyslog("tvscraper: DEBUG GetDurationInSecMarks: sanity check, one sequence, more than expected margin cut. Recording length %i length of cut out of recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, m_recording->FileName() );
    }
    if (m_durationInSecMarks != -1 && config.enableDebug) dsyslog("tvscraper: GetDurationInSecMarks: sanity check ok, one sequence, Recording length %i length of cut out of recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, m_recording->FileName() );
    return m_durationInSecMarks;
  }

// more than one sequence, ads found
// guess a reasonable min length
  if (durationInSeconds < DurationLowSec() ) {
    dsyslog("tvscraper: DEBUG GetDurationInSecMarks: sanity check, too much cut out of recording. Recording length %i length of cut recording %i expected min length of cut recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, DurationLowSec(), m_recording->FileName() );
    return m_durationInSecMarks;
  }

  if (config.enableDebug) dsyslog("tvscraper: DEBUG GetDurationInSecMarks: sanity check ok, Recording length %i length of cut out of recording %i filename \"%s\"", m_recording->LengthInSeconds(), durationInSeconds, m_recording->FileName() );

  m_durationInSecMarks = durationInSeconds;
  return m_durationInSecMarks;
}

bool csRecording::isTsTimestampReasonable(time_t tsTimestamp) const {
  if (tsTimestamp + 30*60 < m_recording->Start()) return false;  // VPS recs can have timestamps < recording->Start()
  if (tsTimestamp         > m_recording->Start() + 10*60*60) return false;
  return true;
}

// ===========================================================================
// ================== readTsFileTimestamps ===================================
// ===========================================================================
void csRecording::readTsFileTimestamps() const {
// if there is more than 1 ts file, compare last change time of previous ts file with creation time of next ts file.

  int l_errors = 0;
#if VDRVERSNUM >= 20505
  l_errors = m_recording->Info()->Errors();
#endif

// use index to validate timestaps
  cToSvConcat index_filename(m_recording->FileName(), "/index");
  if (m_recording->IsPesRecording()) index_filename.concat(".vdr");
  cToSvFile index(index_filename);
  if (!index.exists()) dsyslog2("file ", index_filename, " does not exist");
  else if (cSv(index).empty()) esyslog2("file ", index_filename, " is empty");
  if (m_debug) dsyslog2("file ", index_filename, " length ", index.length(), " frames ", index.length()/8);
  const unsigned char *index_ptr = (const unsigned char *)index.data();
  if (cSv(index).empty()) index_ptr = nullptr;

  cToSvConcat ts_file_name(m_recording->FileName(), "/");
  size_t ts_file_name_0_length = ts_file_name.length();
  time_t l_modification_last_ts_file = s_time_t_not_available;  // to avoid compiler warnings
  time_t l_birth_last_ts_file = s_time_t_not_available;  // to avoid compiler warnings
  bool birthDateAvailable = true;
  bool tsTimestampsReasonable = true; // note: in case of any unreasonable timestamp we ignore all timestamps
  size_t index_pos = 0;
  for (m_number_of_ts_files = 1; ; ++m_number_of_ts_files) {
    ts_file_name.erase(ts_file_name_0_length);
    if (m_recording->IsPesRecording()) ts_file_name.appendInt<3>(m_number_of_ts_files).concat(".vdr");
    else ts_file_name.appendInt<5>(m_number_of_ts_files).concat(".ts");

    struct statx l_stat;
// Modify - the last time the file was modified (content has been modified)
// Change - the last time meta data of the file was changed (e.g. permissions)
// statx: On success, zero is returned.  On error, -1 is returned, and errno is set to indicate the error.
    if (statx(0, ts_file_name.c_str(), 0, STATX_BTIME | STATX_MTIME, &l_stat) == 0) {
// files does exist

// check availability of dates, and whether they or OK or modified, e.g. by copy, touch, ...
      birthDateAvailable &= (l_stat.stx_mask & STATX_BTIME) && l_stat.stx_btime.tv_sec != 0;
// some tools copying VDR files seem to set the mod. timestamp to m_recording->Start(), so exclude this
      tsTimestampsReasonable &= (l_stat.stx_mask & STATX_MTIME) && isTsTimestampReasonable(l_stat.stx_mtime.tv_sec) && l_stat.stx_mtime.tv_sec != m_recording->Start();
      if (birthDateAvailable & tsTimestampsReasonable)
        tsTimestampsReasonable = l_stat.stx_mtime.tv_sec >= l_stat.stx_btime.tv_sec && isTsTimestampReasonable(l_stat.stx_btime.tv_sec);

// check index
      if (index_ptr) {
        int w_n_rep = 0;
        size_t new_index_pos;
        for (new_index_pos=index_pos; new_index_pos < index.length(); new_index_pos+=8) {
          uint16_t file_number;
          if (m_recording->IsPesRecording()) file_number = index_ptr[new_index_pos+5];
          else memcpy(&file_number, index_ptr+new_index_pos+6, 2);
          if (file_number == m_number_of_ts_files+1) break;
          if (file_number != m_number_of_ts_files) {
            if (++w_n_rep < 4)
              esyslog3("ts file ", ts_file_name, " number ", m_number_of_ts_files, " != file_number ", file_number, " new_index_pos= ", new_index_pos, " index.length()= ", index.length());
            break;
          }
        }
        double d_time_for_this_ts_file_index = (double)((new_index_pos-index_pos)/8)/m_recording->FramesPerSecond();
// we cut length seconds behind decimal point, to be consistent with vdr: see cRecording::LengthInSeconds()
        int time_for_this_ts_file_index = d_time_for_this_ts_file_index;

        if (birthDateAvailable & tsTimestampsReasonable) {
          int time_for_this_ts_file_timestamp = l_stat.stx_mtime.tv_sec - l_stat.stx_btime.tv_sec;
          if (l_stat.stx_btime.tv_nsec > l_stat.stx_mtime.tv_nsec) --time_for_this_ts_file_timestamp;
          if (m_debug) dsyslog2("ts file ", ts_file_name, " length timestamps: ", time_for_this_ts_file_timestamp, " s, lenght index file: ", time_for_this_ts_file_index, " s");

          if (std::abs(time_for_this_ts_file_timestamp - time_for_this_ts_file_index) > 2 && l_errors < 30) {
            isyslog2("ts file ", ts_file_name, " mismatch of timestamps and length according to index file, length timestamps: ", time_for_this_ts_file_timestamp, " s, lenght index file: ", time_for_this_ts_file_index, " s, new index pos ", new_index_pos, " index.length()= ", index.length(), " recording length ", m_recording->LengthInSeconds() );
            if (std::abs(2*time_for_this_ts_file_timestamp - time_for_this_ts_file_index) < 4)
              isyslog2("Frames per second in the info file might be wrong, correct value seems to be ", 2*m_recording->FramesPerSecond() );
          }
        }

        index_pos = new_index_pos;
      }
      if (m_number_of_ts_files == 1) {
// first loop
        if (birthDateAvailable & tsTimestampsReasonable) {
          m_creation_first_ts_file = l_stat.stx_btime.tv_sec;
          m_gaps_between_ts_files = 0;
        }
      } else {
// not first loop
        tsTimestampsReasonable &= l_stat.stx_mtime.tv_sec > l_modification_last_ts_file;
        if (birthDateAvailable & tsTimestampsReasonable) {
          tsTimestampsReasonable &= l_stat.stx_btime.tv_sec+30>l_modification_last_ts_file;  // allow 30 s overlap
          tsTimestampsReasonable &= l_stat.stx_btime.tv_sec   <l_modification_last_ts_file + 3*60*60;  // allow 3 hours gap
          if (tsTimestampsReasonable && l_stat.stx_btime.tv_sec>l_modification_last_ts_file+1) {
// ignore gaps <= 1s. Tests show small gaps, and VDR will not restart in 1s
            m_gaps_between_ts_files += l_stat.stx_btime.tv_sec - l_modification_last_ts_file;
            if (l_stat.stx_btime.tv_sec>l_modification_last_ts_file+2) {
              if (m_vps_used) {
// some stations send wrong VPS data, resulting in very short ts files which can be ignored
                if (l_stat.stx_mtime.tv_sec > l_stat.stx_btime.tv_sec + 120 && l_modification_last_ts_file > l_birth_last_ts_file + 120)
                  isyslog("tvscraper, ERROR VPS recording was most likely interrupted before file %s and is incomplete, gap of %d seconds",
                          ts_file_name.c_str(), (int)(l_stat.stx_btime.tv_sec - l_modification_last_ts_file));
                }
              else
                isyslog("tvscraper, ERROR recording was most likely interrupted before file %s and is incomplete, gap of %d seconds",
                        ts_file_name.c_str(), (int)(l_stat.stx_btime.tv_sec - l_modification_last_ts_file));
            }
          }
        }
      }
      l_modification_last_ts_file = l_stat.stx_mtime.tv_sec;
      l_birth_last_ts_file = l_stat.stx_btime.tv_sec;
    } else {
// file does not exist
      --m_number_of_ts_files;
      if (m_number_of_ts_files > 0) {
        if (index_ptr && index_pos < index.length() ) {
          uint16_t file_number;
          if (m_recording->IsPesRecording()) file_number = index_ptr[index_pos+5];
          else memcpy(&file_number, index_ptr+index_pos+6, 2);
          esyslog2("rec ", m_recording->FileName(), ", index file points to non existing ts file number ", file_number);
        }
        if (!birthDateAvailable) {
//        dsyslog2("rec ", m_recording->FileName(), ", birthdates not available");
          m_creation_first_ts_file = s_time_t_not_available;
          m_gaps_between_ts_files = s_int_not_available;
        }
        if (tsTimestampsReasonable) m_modification_last_ts_file = l_modification_last_ts_file;
        else {
// timestamps not reasonable -> no timestamp related data.
//   Report this only if recording is from 2024 or newer (Jan 01 2024 00:00:00 GMT+0100 == 1704063600)
//   Old recordings are often copied, moved, ... so having unreasonable timestamps is "normal"
          if (m_debug || (m_recording->Start() > 1704063600 && l_errors < 10)) dsyslog2("rec ", m_recording->FileName(), ", timestamps disabled as timestamps are not reasonable");
          m_creation_first_ts_file = s_time_t_not_available;
          m_gaps_between_ts_files = s_int_not_available;
          m_modification_last_ts_file = s_time_t_not_available;
        }
      } else {
// no ts files -> no data available
        isyslog("tvscraper, ERROR rec %s has no *ts or *vdr files", m_recording->FileName());
        m_creation_first_ts_file = s_time_t_not_available;
        m_modification_last_ts_file = s_time_t_not_available;
        m_gaps_between_ts_files = s_int_not_available;
      }
      if (m_debug) dsyslog("tvscraper, DEBUG %s, m_gaps_between_ts_files = %d, m_creation_first_ts_file = %lld, m_modification_last_ts_file = %lld",
          m_recording->FileName(), m_gaps_between_ts_files, (long long)m_creation_first_ts_file, (long long)m_modification_last_ts_file);
      return;
    }
  }
}

template<class TV=cSv, class C_IT=const char*>
int markad_offset(const_split_iterator<TV, C_IT> &col, int return_on_error, cStr filename, cSv line) {
// return return_on_error in case of error
  if (++col == iterator_end() ) {
    esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", offset not found", filename.c_str(), (int)line.length(), line.data());
    return return_on_error;
  }
  if (++col == iterator_end() ) {
    esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", offset not found (2)", filename.c_str(), (int)line.length(), line.data());
    return return_on_error;
  }
  return lexical_cast<int>(*col, return_on_error, "offset markad.vps");
}

void csRecording::readMarkadVps() const {
  m_markad_vps_used = s_int_not_available;
  m_markad_vps_start = s_int_not_available;
  m_markad_vps_stop  = s_int_not_available;   // to check comletness of recording: reording->Length() == m_markad_vps_stop
  m_markad_vps_length = s_int_not_available;  // to match length in external database, and for markads sanity check
// set m_markad_vps_used, m_markad_vps_length and m_markad_vps_start
/*
 Example:
VPSTIMER=NO
START: 29.08.2024-16:13:26 506
PAUSE_START: 29.08.2024-16:33:10 1690
PAUSE_STOP: 29.08.2024-16:34:32 1772
STOP: 29.08.2024-16:58:04 3184
Die letzte Spalte ist Offset in Sekunden zu Aufnahme Start.
Achtung! Nicht der Offset zu dem timestamp in *.rec (recording->Start())! Sondern zum tatsächlichen Start der Aufnahme. Die unterscheiden sich :(
*/

  cToSvConcat filename(m_recording->FileName(), "/markad.vps");
  cToSvFile markad_vps(filename);
  if (!markad_vps.exists() ) return;

  int last_start = -1;
  int length = 0;
  for (cSv line: const_split_iterator(cSv(markad_vps), '\n')) {
    if (line.empty() ) continue; // ignore empty lines
    if (line == "VPSTIMER=NO") { m_markad_vps_used = 0; continue; }
    if (line == "VPSTIMER=YES") { m_markad_vps_used = 1; continue; }
// Note: There are old versions of markad.vps without this line
    const_split_iterator col(line, ' ');
    if (*col == "START:") {
      if (m_markad_vps_start != s_int_not_available) {
        esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", two or more times keyword START:", filename.c_str(), (int)line.length(), line.data());
        return;
      }
      m_markad_vps_start = markad_offset(col, s_int_not_available, filename, line);
      if (m_markad_vps_start == s_int_not_available) return; // error in convert already reported
      last_start = m_markad_vps_start;
      continue;
    }
    if (m_markad_vps_start == s_int_not_available) {
      esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", keyword START: missing as first keyword", filename.c_str(), (int)line.length(), line.data());
      return;
    }
    if (*col == "PAUSE_START:") {
      if (last_start == -1) {
        esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", two or more times keyword PAUSE_START", filename.c_str(), (int)line.length(), line.data());
        return;
      }
      int pause_start = markad_offset(col, s_int_not_available, filename, line);
      if (pause_start == s_int_not_available) return; // error in convert already reported
      length += (pause_start - last_start);
      last_start = -1;
      continue;
    }
    if (*col == "PAUSE_STOP:") {
      if (last_start != -1) {
        esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", keyword PAUSE_STOP: without preceeding PAUSE_START", filename.c_str(), (int)line.length(), line.data());
        return;
      }
      int pause_stop = markad_offset(col, s_int_not_available, filename, line);
      if (pause_stop == s_int_not_available) return; // error in convert already reported
      last_start = pause_stop;
      continue;
    }
    if (*col == "STOP:") {
      if (last_start == -1) {
        esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", keyword STOP: after PAUSE_START", filename.c_str(), (int)line.length(), line.data());
        return;
      }
      m_markad_vps_stop = markad_offset(col, s_int_not_available, filename, line);
      if (m_markad_vps_stop == s_int_not_available) return; // error in convert already reported
      length += (m_markad_vps_stop - last_start);
      m_markad_vps_length = length;
      if (m_debug) dsyslog("tvscraper, DEBUG %s, m_markad_vps_used %s, m_markad_vps_start %d, m_markad_vps_stop %d, m_markad_vps_length %d",
         filename.c_str(), m_markad_vps_used==1?"true":m_markad_vps_used==0?"false":"not available", m_markad_vps_start, m_markad_vps_stop, m_markad_vps_length);
      return;
    }
    esyslog("tvscraper: ERROR parsing %s, line \"%.*s\", unknown keyword", filename.c_str(), (int)line.length(), line.data());
  }
  if (m_markad_vps_start != s_int_not_available)
    esyslog("tvscraper: ERROR parsing %s, EOF, keyword STOP: missing", filename.c_str());
  if (m_debug) dsyslog("tvscraper, DEBUG %s, m_markad_vps_used %s, m_markad_vps_start %d, m_markad_vps_stop %d, m_markad_vps_length %d",
     filename.c_str(), m_markad_vps_used==1?"true":m_markad_vps_used==0?"false":"not available", m_markad_vps_start, m_markad_vps_stop, m_markad_vps_length);
}

void csRecording::readTvscraper() const {
// set defaults (no info in tvscraper.json)
  m_tvscraper_vps_used = s_int_not_available;
  m_tvscraper_start = s_time_t_not_available;
  m_tvscraper_stop  = s_time_t_not_available;

  cToSvConcat filename(m_recording->FileName(), "/tvscraper.json");
  struct stat buffer;
  if (stat (filename.c_str(), &buffer) != 0) filename.erase().concat(m_recording->FileName(), "/tvscrapper.json");
  cJsonDocumentFromFile document(filename);

  if (document.HasParseError() ) return;
  rapidjson::Value::ConstMemberIterator timer_j = document.FindMember("timer");
  if (timer_j == document.MemberEnd() ) return;  // timer information not available

  bool vps;
  if (getValue(timer_j->value, "vps", vps, filename.c_str(), &document) ) m_tvscraper_vps_used = vps?1:0;
  getValue(timer_j->value, "start_time", m_tvscraper_start, filename.c_str() ,&document);
  getValue(timer_j->value, "stop_time", m_tvscraper_stop, filename.c_str() ,&document);
  if (m_tvscraper_start != s_time_t_not_available) m_tvscraper_start = roundMinutes(m_tvscraper_start);
  if (m_tvscraper_stop  != s_time_t_not_available) m_tvscraper_stop  = roundMinutes(m_tvscraper_stop );

// consistency with start / stop: you only need to check one of them to find out if data is availabe
  if ((m_tvscraper_stop == s_time_t_not_available && m_tvscraper_start != s_time_t_not_available) ||
      (m_tvscraper_stop != s_time_t_not_available && m_tvscraper_start == s_time_t_not_available)) {
    esyslog("tvscraper ERROR in %s, tvscraper.json data inconsistent, m_tvscraper_start %lld, m_tvscraper_stop %lld", m_recording->FileName(), (long long)m_tvscraper_start, (long long)m_tvscraper_stop);
    m_tvscraper_start = s_time_t_not_available;
    m_tvscraper_stop = s_time_t_not_available;
  }
  if (m_debug) dsyslog("tvscraper, DEBUG %s, m_tvscraper_vps_used %s, m_tvscraper_start %lld, m_tvscraper_stop %lld",
     filename.c_str(), m_tvscraper_vps_used==1?"true":m_tvscraper_vps_used==0?"false":"not available", (long long)m_tvscraper_start, (long long)m_tvscraper_stop);
}

void csRecording::readEpgsearch() const {
// set defaults (no info in aux)
  m_epgsearch_start = s_time_t_not_available;
  m_epgsearch_stop  = s_time_t_not_available;
  m_epgsearch_bstart = s_int_not_available;
  m_epgsearch_bstop  = s_int_not_available;

  if (!m_recording->Info()->Aux() ) return;
  cSv epgsearchAux = partInXmlTag(m_recording->Info()->Aux(), "epgsearch");
  if (epgsearchAux.empty()) return;
// there is an epgsearch tag in aux

// read start / stop
  cSv epgsearchStart = partInXmlTag(epgsearchAux, "start");
  if (!epgsearchStart.empty()) m_epgsearch_start = lexical_cast<time_t>(epgsearchStart, s_time_t_not_available, "epgsearch aux start");
// lexical_cast(cSv sv, T returnOnError = T(), const char *context = nullptr)
  if (m_epgsearch_start != s_time_t_not_available) m_epgsearch_start = roundMinutes(m_epgsearch_start);
// we remove here the seconds as VDR saves only hours/mins for timers, no seconds
  cSv epgsearchStop = partInXmlTag(epgsearchAux, "stop");
  if (!epgsearchStop.empty()) m_epgsearch_stop  = lexical_cast<time_t>(epgsearchStop, s_time_t_not_available, "epgsearch aux stop");
  if (m_epgsearch_stop != s_time_t_not_available) m_epgsearch_stop = roundMinutes(m_epgsearch_stop);

// consistency with start / stop: you only need to check one of them to find out if data is availabe
  if ((m_epgsearch_stop == s_time_t_not_available && m_epgsearch_start != s_time_t_not_available) || (m_epgsearch_stop != s_time_t_not_available && m_epgsearch_start == s_time_t_not_available)) {
    esyslog("tvscraper ERROR in %s, epgsearch data inconsistent, m_epgsearch_start %lld, m_epgsearch_stop %lld", m_recording->FileName(), (long long)m_epgsearch_start, (long long)m_epgsearch_stop);
    m_epgsearch_start = s_time_t_not_available;
    m_epgsearch_stop = s_time_t_not_available;
  }

// read bstart / bstop
  cSubstring epgsearchBstart = substringInXmlTag(epgsearchAux, "bstart");
  cSubstring epgsearchBstop  = substringInXmlTag(epgsearchAux, "bstop");
  if (!epgsearchBstart.found() && !epgsearchBstop.found() ) return;
  if (!epgsearchBstart.found() || !epgsearchBstop.found() ) {
    esyslog2("in ", m_recording->FileName(), " epgsearch data inconsistent, epgsearchBstart \"", epgsearchBstart.substr(epgsearchAux), "\", epgsearchBstop \"", epgsearchBstop.substr(epgsearchAux), "\"");
    return;
  }

  m_epgsearch_bstart = lexical_cast<int>(epgsearchBstart.substr(epgsearchAux), s_int_not_available, "epgsearch aux bstart");
  m_epgsearch_bstop  = lexical_cast<int>(epgsearchBstop.substr(epgsearchAux),  s_int_not_available, "epgsearch aux bstop");
// consistency with bstart / bstop: you only need to check one of them to find out if data is availabe
  if (m_epgsearch_bstart == s_int_not_available || m_epgsearch_bstop == s_int_not_available) {
    esyslog("tvscraper ERROR in %s, cannot convert epgsearch data to time_t, m_epgsearch_bstart %lld, m_epgsearch_bstop %lld", m_recording->FileName(), (long long)m_epgsearch_bstart, (long long)m_epgsearch_bstop);
    m_epgsearch_bstart = s_int_not_available;
    m_epgsearch_bstop  = s_int_not_available;
  }
}

int csRecording::durationDeviationVps(int s_runtime) const {
// VPS was used
  if (m_markad_vps_stop != s_int_not_available) return std::max(0, m_markad_vps_stop - m_recording->LengthInSeconds() );
  int deviation = m_event->Duration() - m_recording->LengthInSeconds();
  if (deviation <= 0) return 0; // recording is longer than event duration -> no error
  if (m_markad_vps_used == s_int_not_available && deviation <= 6*60) return 0; // no markad data, VPS was used -> we report only huge deviations > 6min

// markad found VPS, but has no VPS times. This indicates that the VPS start event was detected too late
// -> we also report small deviations
  if (s_runtime <= 0) return deviation;
  return std::min(deviation, std::abs(s_runtime*60 - m_recording->LengthInSeconds()));
}

int csRecording::margin() const {
// return length of timer - length of event. Note: this can be < 0!
// for VPS recordings and edited recordings, return 0
  read_ext_data();
  if (m_vps_used || IsEdited() ) return 0;

  if (m_timer_stop != s_time_t_not_available) return (int)(m_timer_stop - m_recording->Start() ) - EventDuration();
  return (int)(m_event->StartTime() - m_recording->Start()) + ::Setup.MarginStop*60;
}

int csRecording::durationDeviationNoVps() const {
  if (m_timer_stop != s_time_t_not_available) return (int)(m_timer_stop - m_recording->Start()) - m_recording->LengthInSeconds();
  else return (int)(m_event->StartTime() + m_event->Duration() + ::Setup.MarginStop*60 - m_recording->Start()) - m_recording->LengthInSeconds();
}
int csRecording::durationDeviationTimestamps(int deviation) const {
  if (m_gaps_between_ts_files != s_int_not_available) return std::max(m_gaps_between_ts_files, deviation);
  return deviation;
}
int csRecording::durationDeviation(int s_runtime) const {
// return deviation between actual recording length, and planned duration (timer / vps)
// return -1 if no information is available
  if (!m_recording || !m_recording->FileName() || !m_recording->Info() || !m_recording->Info()->GetEvent() ) return -1;
  if (IsEdited() ) return 0;  // we assume, who ever edited the recording checked for completeness
  if (recordingLengthIsChanging() ) return -1;  // still recording => incomplete, this check is useless
  if (m_debug) dsyslog("tvscraper DEBUG: csRecording::durationDeviation 1, s_runtime %d", s_runtime);
  read_ext_data();
  if (m_vps_used) return durationDeviationTimestamps(durationDeviationVps(s_runtime));
// no VPS used
  int deviation = std::max(0, durationDeviationNoVps());

// start time / file creation time
  if (m_creation_first_ts_file != s_time_t_not_available) {
    deviation = std::max(deviation, (int)(m_creation_first_ts_file - m_recording->Start() ));
    if (std::abs(m_recording->Start() - m_creation_first_ts_file) > 60)
      dsyslog("tvscraper DEBUG, recording %s start time inconsistent, recording->Start() %lld, creation_time %lld", m_recording->FileName(), (long long)m_recording->Start(), (long long)m_creation_first_ts_file);
  }


  if (m_modification_last_ts_file != s_time_t_not_available) {
// disable this check for epgsearch timers. Reason: timer stop time might have been changed by epgsearch, after start of recording.
// even if epgsearch updated aux in timer, aux in recording is not updated by VDR
    if (m_timer_stop != s_time_t_not_available && m_epgsearch_start == s_time_t_not_available && m_epgsearch_bstart == s_int_not_available) {
      deviation = std::max(deviation, (int)(m_timer_stop - m_modification_last_ts_file));
      if (std::abs(m_timer_stop - m_modification_last_ts_file) > 60)
        dsyslog("tvscraper DEBUG, recording %s stop time inconsistent, m_timer_stop, %lld, last change time %lld", m_recording->FileName(), (long long)m_timer_stop, (long long)m_modification_last_ts_file);
    } else {
// check: m_recording->Start() + m_recording->LengthInSeconds() == LastChangeOfLast*ts File;  -> otherwise, rec was interrupted
      if (std::abs(m_recording->Start() + m_recording->LengthInSeconds() - m_modification_last_ts_file) > 60)
        dsyslog("tvscraper DEBUG, recording %s stop time inconsistent, recording->Start() + Length, %lld, last change time %lld", m_recording->FileName(), (long long)(m_recording->Start()+m_recording->LengthInSeconds()), (long long)m_modification_last_ts_file);
    }
  }

  return durationDeviationTimestamps(deviation);
}

csEventOrRecording *GetsEventOrRecording(const cEvent *event, const cRecording *recording) {
  if (event) return new csEventOrRecording(event);
  if (recording && recording->Info() && recording->Info()->GetEvent() ) return new csRecording(recording);
  return nullptr;
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


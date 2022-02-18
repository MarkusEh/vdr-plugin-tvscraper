#include "eventOrRec.h"

csEventOrRecording::csEventOrRecording(const cEvent *event):
  m_event(event)
{
}
void csEventOrRecording::AddYears(vector<int> &years) const {
  ::AddYears(years, Title() );
  ::AddYears(years, ShortText() );
  ::AddYears(years, Description() );
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
  durationInMinHigh = DurationHighSec() / 60 + (10 * DurationHighSec())  / 60 / 60;  // add 10 mins for 60 min duration, mor for longer durations. This is because often a cut version of the movie is broadcasted
//  if (Recording() && Title() && strcmp("The Expendables 3", Title() ) == 0) 
//  esyslog("tvscraper: csEventOrRecording::DurationRange, title = %s, durationInMinLow  = %i,  durationInMinHigh = %i", Title(), durationInMinLow, durationInMinHigh);
  return true;
}

const char *csEventOrRecording::EpisodeSearchString() const {
  if(ShortText() && *ShortText() ) return ShortText();
  if(Description() ) return Description();
  return "";
}

// Recording

csRecording::csRecording(const cRecording *recording) :
  csEventOrRecording(recording->Info()->GetEvent() ),
  m_recording(recording)
{
}

bool csRecording::DurationRange(int &durationInMinLow, int &durationInMinHigh) {
// return true, if data is available
  if (!m_recording->FileName() ) return false;
  if (!m_event->Duration() && !m_recording->LengthInSeconds() ) return false;

  if (m_recording->IsEdited() || DurationInSecMarks() != -1) {
// length of recording without adds
    int durationInSec = m_recording->IsEdited()?m_recording->LengthInSeconds():DurationInSecMarks();
    durationInMinLow  = durationInSec / 60 - 1;  // 1 min for inaccuracies
    durationInMinHigh = durationInSec / 60 +  (12 * durationInSec)  / 60 / 60;  // add 12 mins for 60 min duration, mor for longer durations. This is because often a cut version of the movie is broadcasted 
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

csEventOrRecording *GetsEventOrRecording(const cEvent *event, const cRecording *recording) {
  if (event) return new csEventOrRecording(event);
  if (recording) return new csRecording(recording);
  return NULL;
}

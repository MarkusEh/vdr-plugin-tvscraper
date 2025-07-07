#ifndef __TVSCRAPER_EVENTORREC_H
#define __TVSCRAPER_EVENTORREC_H
#include <string>
#include "sEvent.h"

int GetNumberOfTsFiles(const cRecording* recording);

class csEventOrRecording {

public:
  csEventOrRecording(const cEvent *event);
  csEventOrRecording(const cRecordingInfo *info);
  csEventOrRecording(const cStaticEvent *sEvent);
  virtual ~csEventOrRecording() {};

  virtual tEventID EventID() const { return m_event->EventID(); }
  virtual time_t StartTime() const { return m_event->StartTime(); }
  virtual time_t RecordingStartTime() const { return 0; }
  virtual int RecordingId() const { return -1; }
  virtual time_t EndTime() const { return m_event->EndTime(); }
  virtual time_t EventVps() const { return m_event->Vps(); }
  virtual int EventDuration() const { return m_event->Duration(); } // this is always the event duration, for recordings it is m_recording->Info()->Event()->Duration()
  virtual int DurationInSec() const { return EventDuration(); }
  virtual const cRecording *Recording() const { return nullptr; }
  virtual void AddYears(cYears &years) const;
  virtual int DurationRange(int &durationInMinLow, int &durationInMinHigh);
  int DurationDistance(int DurationInMin);
  virtual cSv EpisodeSearchString() const;
  virtual const tChannelID ChannelID() const { return m_event->ChannelID(); }
  virtual const char *Title() const { return m_event->Title(); }
  virtual const char *ShortText() const { return m_event->ShortText(); }
  cSv Description() const { return m_description; }
  const cLanguage *GetLanguage() const { return config.GetLanguage(ChannelID()); }
  virtual int durationDeviation(int s_runtime) const { return 6000; }
  static constexpr const char *m_unknownChannel = "Channel name unknown";
protected:
  virtual int DurationWithoutMarginSec(void) const { return EventDuration(); }
  virtual int DurationHighSec() const { return EventDuration(); }
  virtual int DurationLowSec() const { return RemoveAdvTimeSec(DurationWithoutMarginSec() ); } // note: we remove ads time also for VPS events as some stations have VPS events wit ads.
  const cEvent *m_event;
  const cSv m_description;
  bool m_debug = false;
  int RemoveAdvTimeSec(int durationSec) const { return durationSec - durationSec / 3 - 2*60; } // 33% adds, 2 mins extra adds
};

class csRecording : public csEventOrRecording {

public:
  csRecording(const cRecording *recording);
  virtual const cRecording *Recording() const { return m_recording; }
  virtual time_t StartTime() const { return m_event->StartTime()?m_event->StartTime(): m_recording->Start(); } // Timervorlauf, die Aufzeichnung startet 2 min früher
  virtual time_t RecordingStartTime() const { return m_recording->Start(); }
  virtual int RecordingId() const { return m_recording->Id() ; }
  bool IsEdited() const { return m_recording->IsEdited() || cSv(m_recording->Info()->Aux()).find("<isEdited>true</isEdited>") != cSv::npos; }
  virtual int DurationInSec() const { return m_event->Duration()?m_event->Duration():m_recording->FileName() ? m_recording->LengthInSeconds() : 0; }
  virtual int DurationRange(int &durationInMinLow, int &durationInMinHigh);
  virtual const tChannelID ChannelID() const { return m_recording->Info()->ChannelID(); }
  virtual const char *Title() const { return m_recording->Info()->Title(); }
  virtual const char *ShortText() const { return m_recording->Info()->ShortText(); }
  virtual int durationDeviation(int s_runtime) const;
protected:
  virtual int DurationWithoutMarginSec(void) const { return m_recording->LengthInSeconds() - std::max(0, margin()); } // correct for validation of marks. For matches to movies: If the recording is good, also correct. Otherwise: We might not identify the correct movie, but we have another (bigger) problem: bad recording ...
  virtual int DurationHighSec(void) const {
// note: only if recording not cut and no valid marks
    read_ext_data();
    if (m_vps_used) {
      if (m_markad_vps_length != s_int_not_available) return m_markad_vps_length+2;
      return m_recording->LengthInSeconds();
    }
    return DurationWithoutMarginSec();
  }
  virtual int DurationLowSec() const {
// note: only if recording not cut and no valid marks
// also used as sanity check for marks
    read_ext_data();
    if (m_vps_used) {
      if (m_markad_vps_length != s_int_not_available) return m_markad_vps_length-2;
      return RemoveAdvTimeSec(m_recording->LengthInSeconds());  // VPS recs. might have ads, but there shouldn't be a margin
    }
    return RemoveAdvTimeSec(DurationWithoutMarginSec());
  }

private:
// return length of timer - length of event. Note: this can be < 0!
// for VPS recordings and edited recordings, return 0
  int margin() const;
  void readTvscraper() const;   // must only be called in read_ext_data()
  void readMarkadVps() const;   // must only be called in read_ext_data()
  void readEpgsearch() const;   // must only be called in read_ext_data()
  void readTsFileTimestamps() const; // must only be called in read_ext_data()
  bool isTsTimestampReasonable(time_t tsTimestamp) const;
  bool recordingLengthIsChanging() const;
  int DurationInSecMarks(void) { return m_durationInSecMarks?m_durationInSecMarks:DurationInSecMarks_int(); }
  int DurationInSecMarks_int(void);
  int durationDeviationVps(int s_runtime) const;
  int durationDeviationNoVps() const;
  int durationDeviationTimestamps(int deviation) const;
// member vars
  const cRecording *m_recording;

  mutable int m_durationInSecMarks = 0; // 0: Not initialized; -1: checked, no data

// ================ "external" data from tvscraper, markad, epgsearch
  void read_ext_data() const;   // all the following member variables are filled with ext data
  mutable bool m_ext_data_read = false;
public:
  static const int    s_int_not_available    = std::numeric_limits<int>::max();     // for int:    unknown, data not available
  static const time_t s_time_t_not_available = std::numeric_limits<time_t>::max();  // for time_t: unknown, data not available
private:

  mutable int m_markad_vps_used;   // 1: true (vps was used). 0: false (vps was not used)
  mutable int m_markad_vps_length; // VPS length in seconds. If this is available, also m_markad_vps_start is available (no length without start mark ...)
  mutable int m_markad_vps_start;  // timestamp of start mark relative to start of recording. Note vdr starts recording at status 2 "starts in a few seconds"
  mutable int m_markad_vps_stop;   // timestamp of stop mark relative to start of recording -> recording length = m_markad_vps_stop
  mutable int m_tvscraper_vps_used;   // 1: true (vps was used). 0: false (vps was not used)
  mutable time_t m_tvscraper_start;
  mutable time_t m_tvscraper_stop;
  mutable time_t m_epgsearch_start;
  mutable time_t m_epgsearch_stop;
  mutable int m_epgsearch_bstart;  //  note: m_epgsearch_bstart & m_epgsearch_bstop can be < 0!
  mutable int m_epgsearch_bstop;
// calculated:
//mutable time_t m_timer_start;  // note: this is m_recording->start()
  mutable time_t m_timer_stop;  // set from: m_tvscraper_stop, m_epgsearch_stop, m_epgsearch_bstop: in this order
  mutable bool m_vps_used;  // always available after read_ext_data() is called, but might be guessed

  mutable time_t m_creation_first_ts_file;
  mutable time_t m_modification_last_ts_file;
  mutable int m_gaps_between_ts_files;
  mutable uint16_t m_number_of_ts_files;
};

class csStaticEvent : public csEventOrRecording {

public:
  csStaticEvent(const cStaticEvent *sEvent):
    csEventOrRecording(sEvent),
    m_sEvent(sEvent)
   {}

  virtual ~csStaticEvent() {};
  virtual tEventID EventID() const { return m_sEvent->EventID(); }
  virtual time_t StartTime() const { return m_sEvent->StartTime(); }
  virtual time_t EndTime() const { return m_sEvent->EndTime(); }
  virtual time_t EventVps() const { return m_sEvent->Vps(); }
  virtual int EventDuration() const { return m_sEvent->Duration(); } // this is always the event duration, for recordings it is m_recording->Info()->Event()->Duration()
  virtual int DurationInSec() const { return EventDuration(); }
  virtual const tChannelID ChannelID() const { return m_sEvent->ChannelID(); }
  virtual const char *Title() const { return m_sEvent->Title(); }
  virtual const char *ShortText() const { return m_sEvent->ShortText(); }

private:
  const cStaticEvent *m_sEvent;
};

csEventOrRecording *GetsEventOrRecording(const cEvent *event, const cRecording *recording);
#endif //__TVSCRAPER_EVENTORREC_H

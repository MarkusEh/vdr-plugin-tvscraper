#ifndef __TVSCRAPER_EVENTORREC_H
#define __TVSCRAPER_EVENTORREC_H
#include <string>
#include "sEvent.h"

int GetNumberOfTsFiles(const cRecording* recording);

class csEventOrRecording {

public:
  csEventOrRecording(const cEvent *event);
  virtual ~csEventOrRecording() {};

  virtual tEventID EventID() const { return m_event->EventID(); }
  virtual time_t StartTime() const { return m_event->StartTime(); }
  virtual time_t RecordingStartTime() const { return 0; }
  virtual time_t EndTime() const { return m_event->EndTime(); }
  virtual time_t EventVps() const { return m_event->Vps(); }
  virtual int EventDuration() const { return m_event->Duration(); } // this is always the event duration, for recordings it is m_recording->Info()->Event()->Duration()
  virtual int DurationInSec() const { return EventDuration(); }
  virtual const cRecording *Recording() const { return NULL; }
  virtual void AddYears(cYears &years) const;
  virtual bool DurationRange(int &durationInMinLow, int &durationInMinHigh);
  int DurationDistance(int DurationInMin);
  virtual cSv EpisodeSearchString() const;
  virtual const tChannelID ChannelID() const { return m_event->ChannelID(); }
  virtual const std::string ChannelIDs() const { return std::string(cToSvConcat(ChannelID() )); }
  virtual std::string ChannelName() const; // Warning: This locks the channels, so ensure correct lock order !!!!
  virtual const char *Title() const { return m_event->Title(); }
  virtual const char *ShortText() const { return m_event->ShortText(); }
  virtual const char *Description() const { return m_event->Description(); }
  const cLanguage *GetLanguage() const { return config.GetLanguage(ChannelID()); }
  virtual int durationDeviation(int s_runtime) { return 6000; }
  static constexpr const char *m_unknownChannel = "Channel name unknown";
protected:
  virtual int DurationWithoutMarginSec(void) const { return EventDuration(); }
  virtual int DurationLowSec() const { return EventVps()?DurationWithoutMarginSec():RemoveAdvTimeSec(DurationWithoutMarginSec() ); } // note: for recording only if not cut, and no valid marks
  virtual int DurationHighSec() const { return EventDuration(); } // note: for recording only if not cut, and no valid marks
  const cEvent *m_event;
  bool m_debug = false;
private:
  int RemoveAdvTimeSec(int durationSec) const { return durationSec - durationSec / 3 - 2*60; } // 33% adds, 2 mins extra adds
};

class csRecording : public csEventOrRecording {

public:
  csRecording(const cRecording *recording);
  virtual const cRecording *Recording() const { return m_recording; }
  virtual time_t StartTime() const { return m_event->StartTime()?m_event->StartTime(): m_recording->Start(); } // Timervorlauf, die Aufzeichnung startet 2 min früher
  virtual time_t RecordingStartTime() const { return m_recording->Start(); }
  virtual int DurationInSec() const { return m_event->Duration()?m_event->Duration():m_recording->FileName() ? m_recording->LengthInSeconds() : 0; }
  virtual bool DurationRange(int &durationInMinLow, int &durationInMinHigh);
  virtual const tChannelID ChannelID() const { return m_recording->Info()->ChannelID(); }
  virtual const std::string ChannelIDs() const { return (EventID()&&ChannelID().Valid())?std::string(cToSvConcat(ChannelID() )):m_recording->Name(); } // if there is no eventID or no ChannelID(), use Name instead
  virtual std::string ChannelName() const { const char *cn = m_recording->Info()->ChannelName(); return (cn&&*cn)?cn:m_unknownChannel; }
  virtual const char *Title() const { return m_recording->Info()->Title(); }
  virtual const char *ShortText() const { return m_recording->Info()->ShortText(); }
  virtual const char *Description() const { return m_recording->Info()->Description(); }
  virtual int durationDeviation(int s_runtime);
protected:
  virtual int DurationWithoutMarginSec(void) const { return m_event->Duration()?m_event->Duration():(m_recording->LengthInSeconds() - 14*60); } // remove margin, 4 min at start, 10 min at stop
  virtual int DurationHighSec(void) const { return m_event->Duration()?m_event->Duration():m_recording->LengthInSeconds(); } // note: for recording only if not cut, and no valid marks
  int getVpsLength();
private:
  int DurationInSecMarks(void) { return m_durationInSecMarks?m_durationInSecMarks:DurationInSecMarks_int(); }
  int DurationInSecMarks_int(void);
  bool getTvscraperTimerInfo(bool &vps, int &lengthInSeconds);
  bool getEpgsearchTimerInfo(bool &vps, int &lengthInSeconds);
  int durationDeviationNoVps();
  int durationDeviationVps(int s_runtime);
// member vars
  const cRecording *m_recording;
  int m_durationInSecMarks = 0; // 0: Not initialized; -1: checked, no data
//   int m_vps_used = -10; // -1: no information available -2: no markad information; -3 no tvscraper inforamtion; 0: NOT used. 1: uesd
  int m_vps_length = -4; // -4: not checked. -3: markad.vps not available. -2 No VPS used. -1: VPS used, but no time available >0: VPS length in seconds
  int m_vps_start = 0; // only if m_vps_length > 0: timestamp of start mark. Note vdr starts recording at status 2 "starts in a few seconds", so recording length = m_vps_length + m_vps_start
};

class csStaticEvent_sik : public csEventOrRecording {

public:
  csStaticEvent_sik(const cEvent *event):
    csEventOrRecording(nullptr),
    m_eventID(event->EventID()),
    m_startTime(event->StartTime()),
    m_endTime(event->EndTime()),
    m_eventVps(event->Vps()),
    m_eventDuration(event->Duration()),
    m_channelID(event->ChannelID()),
    m_title(event->Title()),
    m_shortText(event->ShortText()),
    m_description(event->Description())
   {}

  virtual ~csStaticEvent_sik() {};
  virtual tEventID EventID() const { return m_eventID; }
  virtual time_t StartTime() const { return m_startTime; }
  virtual time_t EndTime() const { return m_endTime; }
  virtual time_t EventVps() const { return m_eventVps; }
  virtual int EventDuration() const { return m_eventDuration; }
  virtual const tChannelID ChannelID() const { return m_channelID; }
  virtual const char *Title() const { return m_title.c_str(); }
  virtual const char *ShortText() const { return m_shortText.c_str(); }
  virtual const char *Description() const { return m_description.c_str(); }

private:
  tEventID m_eventID;
  time_t m_startTime;
  time_t m_endTime;
  time_t m_eventVps;
  int m_eventDuration;
  tChannelID m_channelID;
  std::string m_title;
  std::string m_shortText;
  std::string m_description;
};

class csStaticEvent : public csEventOrRecording {

public:
  csStaticEvent(const cStaticEvent *sEvent):
    csEventOrRecording(nullptr),
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
  virtual const char *Description() const { return m_sEvent->Description(); }

private:
  const cStaticEvent *m_sEvent;
};

csEventOrRecording *GetsEventOrRecording(const cEvent *event, const cRecording *recording);
#endif //__TVSCRAPER_EVENTORREC_H

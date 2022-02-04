#include <string>

class csEventOrRecording {

public:
  csEventOrRecording(const cEvent *event);
  virtual ~csEventOrRecording() {};

  virtual tEventID EventID() const { return m_event->EventID(); }
  virtual time_t StartTime() const { return m_event->StartTime(); }
  virtual time_t EndTime() const { return m_event->EndTime(); }
  virtual int DurationInSec() const { return m_event->Duration(); }
  virtual const cString ChannelIDs() const { return ChannelID().ToString(); }
  virtual const cRecording *Recording() const { return NULL; }
  virtual void AddYears(vector<int> &years) const;
  virtual bool DurationRange(int &durationInMinLow, int &durationInMinHigh);
  int DurationDistance(int DurationInMin);
  virtual const char *EpisodeSearchString() const;
  virtual const char *Title() const { return m_event->Title(); }
protected:
  virtual const tChannelID ChannelID() const { return m_event->ChannelID(); }
  virtual const char *ShortText() const { return m_event->ShortText(); }
  virtual const char *Description() const { return m_event->Description(); }
  virtual int DurationWithoutMarginSec(void) const { return m_event->Duration(); }
  virtual int DurationLowSec(void) const { return m_event->Vps()?DurationWithoutMarginSec():RemoveAdvTimeSec(DurationWithoutMarginSec() ); } // note: for recording only if not cut, and no valid marks
  virtual int DurationHighSec(void) const { return m_event->Duration(); } // note: for recording only if not cut, and no valid marks
  const cEvent *m_event;
private:
  int RemoveAdvTimeSec(int durationSec) const { return durationSec - durationSec / 3 - 2*60; } // 33% adds, 2 mins extra adds
};

class csRecording : public csEventOrRecording {

public:
  csRecording(const cRecording *recording);
  virtual const cRecording *Recording() const { return m_recording; }
  virtual time_t StartTime() const { return m_event->StartTime()?m_event->StartTime(): m_recording->Start(); } // Timervorlauf, die Aufzeichnung startet 2 min früher
  virtual int DurationInSec() const { return m_event->Duration()?m_event->Duration():m_recording->FileName() ? m_recording->LengthInSeconds() : 0; }
  virtual bool DurationRange(int &durationInMinLow, int &durationInMinHigh);
  virtual const cString ChannelIDs() const { return (EventID()&&ChannelID().Valid())?ChannelID().ToString():cString(m_recording->Name() ); } // if there is no eventID or no ChannelID(), use Name instead
  virtual const char *Title() const { return m_recording->Info()->Title(); }
protected:
  virtual const tChannelID ChannelID() const { return m_recording->Info()->ChannelID(); }
  virtual const char *ShortText() const { return m_recording->Info()->ShortText(); }
  virtual const char *Description() const { return m_recording->Info()->Description(); }
  virtual int DurationWithoutMarginSec(void) const { return m_event->Duration()?m_event->Duration():(m_recording->LengthInSeconds() - 14*60); } // remove margin, 4 min at start, 10 min at stop
  virtual int DurationHighSec(void) const { return m_event->Duration()?m_event->Duration():m_recording->LengthInSeconds(); } // note: for recording only if not cut, and no valid marks
private:
  int DurationInSecMarks(void) { return m_durationInSecMarks?m_durationInSecMarks:DurationInSecMarks_int(); }
  int DurationInSecMarks_int(void);
// member vars
  const cRecording *m_recording;
  int m_durationInSecMarks = 0; // 0: Not initialized; -1: checked, no data
};

csEventOrRecording *GetsEventOrRecording(const cEvent *event, const cRecording *recording);

#ifndef __TVSCRAPER_SEVENT_H
#define __TVSCRAPER_SEVENT_H
#include <string>
#include "tools/stringhelpers.h"

class cStaticEvent {

public:
  cStaticEvent(const cEvent *event):
    m_eventID(event->EventID()),
    m_startTime(event->StartTime()),
    m_endTime(event->EndTime()),
    m_vps(event->Vps()),
    m_duration(event->Duration()),
    m_channelID(event->ChannelID()),
    m_title(cSv(event->Title())),
    m_shortText(cSv(event->ShortText())),
    m_description(cSv(event->Description()))
   {}

  tEventID EventID() const { return m_eventID; }
  time_t StartTime() const { return m_startTime; }
  time_t EndTime() const { return m_endTime; }
  time_t Vps() const { return m_vps; }
  int Duration() const { return m_duration; }
  const tChannelID ChannelID() const { return m_channelID; }
  const char *Title() const { return m_title.c_str(); }
  const char *ShortText() const { return m_shortText.c_str(); }
  const char *Description() const { return m_description.c_str(); }
template<class changeEventF>
  bool ChangeEvent(changeEventF changeEventFunction) {
// return false if no event was found
// return true if event was found and changeEventFunction was called
    LOCK_SCHEDULES_WRITE;
    cSchedule *pSchedule = (cSchedule *)Schedules->GetSchedule(m_channelID);
    if (!pSchedule) return false;
    cEvent *event = const_cast<cEvent *>(pSchedule->GetEvent(m_eventID));
    if (!event) return false;
    changeEventFunction(event);
    return true;
  }
  void SetShortText(const char *ShortText) {
    if (ChangeEvent([ShortText](cEvent *event) { event->SetShortText(ShortText); }))
      m_shortText = cSv(ShortText);
  }
  void SetDescription(const char *Description) {
    if (ChangeEvent([Description](cEvent *event) { event->SetDescription(Description); }))
      m_description = cSv(Description);
  }

private:
  tEventID m_eventID;
  time_t m_startTime;
  time_t m_endTime;
  time_t m_vps;
  int m_duration;
  tChannelID m_channelID;
  std::string m_title;
  std::string m_shortText;
  std::string m_description;
  cEvent *getEvent(cStateKey &SchedulesStateKey) {
    cSchedules *Schedules = cSchedules::GetSchedulesWrite(SchedulesStateKey);
    cSchedule *pSchedule = (cSchedule *)Schedules->GetSchedule(m_channelID);
    if (!pSchedule) return nullptr;
    return const_cast<cEvent *>(pSchedule->GetEvent(m_eventID));
  }
};

#endif //__TVSCRAPER_SEVENT_H

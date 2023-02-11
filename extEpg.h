#include <memory>
#include "rapidjson/document.h"

//***************************************************************************
// cExtEpgHandler
//***************************************************************************

class cExtEpgHandler : public cEpgHandler
{
  public:
    cExtEpgHandler() : cEpgHandler() { }
    ~cExtEpgHandler() { }

    virtual bool SetDescription(cEvent *Event, const char *Description) {
// return true if we want to prevent a change of Event->Description()
      if (!Event->Description() ) return false;
      if (strstr (Event->Description(), "tvsp") == NULL) return false;
// the decription contains tvsp, so it was provided externally and should not be changed by VDR
      return true;
    }
    virtual bool SetShortText(cEvent *Event, const char *ShortText) {
// return true if we want to prevent a change of Event->ShortText()
      if (!ShortText || !*ShortText) return true;  // prevent deletion of an existing short text.
      return false;
    }
};

class cTvspEvent
{
  public:
    cTvspEvent(const rapidjson::Value& tvspEvent, rapidjson::SizeType line);
    time_t m_startTime;
    time_t m_endTime;
    rapidjson::SizeType m_line;
};
bool operator<(const cTvspEvent &e1, const cTvspEvent &e2) { return e1.m_startTime < e2.m_startTime; }

class cTvspEpgOneDay
{
  public:
    cTvspEpgOneDay(const sChannelMapEpg *channelMapEpg, time_t startTime);
    bool enhanceEvent(cEvent *event); // return true if the event is in "my" time frame (one day )
  private:
    void initJson(time_t startTime);
    int eventMatch(vector<cTvspEvent>::const_iterator event_it, const cEvent *event) const;
    bool findTvspEvent(vector<cTvspEvent>::const_iterator &event_it, const cEvent *event) const;
    const sChannelMapEpg *m_channelMapEpg;
    time_t m_start;
    time_t m_end;
    std::shared_ptr<cLargeString> m_json;
    std::shared_ptr<rapidjson::Document> m_document;
    std::shared_ptr<std::vector<cTvspEvent>> m_events;
    const int c_always_accepted_deviation = 60;  // seconds
    const int c_never_accepted_deviation = 60 * 30;  // seconds
};
class cTvspEpg
{
  public:
    cTvspEpg(const sChannelMapEpg *channelMapEpg):m_channelMapEpg(channelMapEpg) { }
    void enhanceEvent(cEvent *event) {
      if (!m_channelMapEpg) return;
      if (event->StartTime() > time(0) + 7*24*60*60) return; // only one week supported
      for (auto &tvspEpgOneDay: m_tvspEpgOneDayS) if (tvspEpgOneDay.enhanceEvent(event)) return;
      cTvspEpgOneDay tvspEpgOneDay(m_channelMapEpg, event->StartTime());
      tvspEpgOneDay.enhanceEvent(event);
      m_tvspEpgOneDayS.push_back(tvspEpgOneDay);
    }
  private:
    const sChannelMapEpg *m_channelMapEpg;
    std::vector<cTvspEpgOneDay> m_tvspEpgOneDayS;
};

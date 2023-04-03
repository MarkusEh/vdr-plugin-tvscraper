#include <memory>
#include "../../rapidjson/document.h"
#include "../../extEpgPlugin.h"
#include "../../tools/largeString.h"

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
    cTvspEpgOneDay(const std::string &extChannelId, time_t startTime);
    bool enhanceEvent(cEvent *event, std::vector<cTvMedia> &extEpgImages); // return true if the event is in "my" time frame (one day )
  private:
    void initJson(const std::string &extChannelId, time_t startTime);
    int eventMatch(std::vector<cTvspEvent>::const_iterator event_it, const cEvent *event) const;
    bool findTvspEvent(std::vector<cTvspEvent>::const_iterator &event_it, const cEvent *event) const;

    time_t m_start;
    time_t m_end;
    std::shared_ptr<cLargeString> m_json;
    std::shared_ptr<rapidjson::Document> m_document;
    std::shared_ptr<std::vector<cTvspEvent>> m_events;
    const int c_always_accepted_deviation = 60;  // seconds
    const int c_never_accepted_deviation = 60 * 30;  // seconds
};
class cTvspEpg: public iExtEpgForChannel
{
  public:
    cTvspEpg(const std::string &extChannelId):
      iExtEpgForChannel(),
      m_extChannelId(extChannelId) {};
    virtual void enhanceEvent(cEvent *event, std::vector<cTvMedia> &extEpgImages) {
      if (event->StartTime() > time(0) + 7*24*60*60) return; // only one week supported
      for (auto &tvspEpgOneDay: m_tvspEpgOneDayS) if (tvspEpgOneDay.enhanceEvent(event, extEpgImages)) return;
      cTvspEpgOneDay tvspEpgOneDay(m_extChannelId, event->StartTime());
      tvspEpgOneDay.enhanceEvent(event, extEpgImages);
      m_tvspEpgOneDayS.push_back(tvspEpgOneDay);
    }
  private:
    const std::string &m_extChannelId;
    std::vector<cTvspEpgOneDay> m_tvspEpgOneDayS;
    bool m_debug;
};

class cExtEpgTvsp: public iExtEpg
{
  public:
    cExtEpgTvsp() {}
    virtual const char *getSource() { return "tvsp"; }
    virtual bool myDescription(const char *description) {
// return true if this description was created by us
      if (!description) return false;
// the descriptions created by us contain "tvsp"
      return strstr(description, "tvsp") != NULL;
    }
    virtual std::shared_ptr<iExtEpgForChannel> getHandlerForChannel(const std::string &extChannelId) {
      return std::make_shared<cTvspEpg>(extChannelId);
    }
};

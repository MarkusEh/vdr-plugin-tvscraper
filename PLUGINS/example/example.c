#include "example.h"
#include <tools/curlfuncs.h>
#include <tools/fuzzy.c>

// cTvspEvent ***********************************************

cTvspEvent::cTvspEvent(const rapidjson::Value& tvspEvent, rapidjson::SizeType line):
  m_line(line)
{
  if (!getValue(tvspEvent, "timestart", m_startTime)) m_startTime = 0;
  if (!getValue(tvspEvent, "timeend", m_endTime)) m_endTime = 0;
}

// cTvspEpgOneDay ***********************************************

cTvspEpgOneDay::cTvspEpgOneDay(cCurl *curl, cSv extChannelId, time_t startTime, bool debug):
  m_document(std::make_shared<cJsonDocumentFromUrl>(curl) ),
  m_events(std::make_shared<std::vector<cTvspEvent>>() ),
  m_debug(debug)
  {
    initJson(extChannelId, startTime);
  }


void cTvspEpgOneDay::initJson(cSv extChannelId, time_t startTime) {
  const int hourDayBegin = 5; // the day beginns at 5:00 (from tvsp point of view ...)
  struct tm tm_r;
  struct tm *time = localtime_r(&startTime, &tm_r);
  if (time->tm_hour < hourDayBegin) {
// we have to go one day back, from tvsp point of view this is still the prev. day
    startTime -= hourDayBegin*60*60;
    time = localtime_r(&startTime, &tm_r);
  }
  cToSvConcat url("http://live.tvspielfilm.de/static/broadcast/list/",
                  extChannelId, '/');
              url.appendInt<4>(time->tm_year + 1900).concat('-').
                  appendInt<2>(time->tm_mon + 1).concat('-').
                  appendInt<2>(time->tm_mday);

// if (extChannelId[0] == 'A') esyslog("tvscraper epg about to download %s", url.c_str() );
// calculate m_start: today 5 am
  time->tm_sec = 0;
  time->tm_min = 0;
  time->tm_hour = hourDayBegin;
  m_start = mktime(time);
  m_end = m_start + 24*60*60;

// download json file
  m_document->set_enableDebug(m_debug);
  if (!m_document->download_and_parse(url)) return; // no data
  if (!m_document->IsArray() ) {
    esyslog("tvscraper: ERROR cTvspEpgOneDay::initJson, parse %s failed, document is not an array", url.c_str() );
    return; // no data
  }
  for (rapidjson::SizeType i = 0; i < m_document->Size(); i++) {
    cTvspEvent tvspEvent((*m_document)[i], i);
    if (tvspEvent.m_startTime == 0 || tvspEvent.m_endTime == 0)
      esyslog("tvscraper: ERROR cTvspEpgOneDay::initJson, parse %s failed, i=%d, timestart/timeend", url.c_str(), (int)i );
    else m_events->push_back(tvspEvent);
  }
  if (m_events->size() != 0) std::sort(m_events->begin(), m_events->end());
}

int cTvspEpgOneDay::eventMatch(std::vector<cTvspEvent>::const_iterator event_it, const cStaticEvent *event) const {
  int deviationStart = std::abs(event_it->m_startTime - event->StartTime() );
  int deviationEnd   = std::abs(event_it->m_endTime - event->EndTime() );
  int deviation = deviationStart + deviationEnd;
  if (deviation <= c_always_accepted_deviation || deviation >= c_never_accepted_deviation) return deviation;
// complare event->Title() with event_it title
  const rapidjson::Value& tvspEvent_j = (*m_document)[event_it->m_line];
  const char *tvsp_title;
  if (!getValue(tvspEvent_j, "title", tvsp_title) ) return deviation;
  if (!tvsp_title || !event->Title() ) return deviation;
  int sd = cNormedString(event->Title() ).sentence_distance(tvsp_title);  // sd == 0 if titles are identical
  if (sd > 600) return c_never_accepted_deviation;
  return c_always_accepted_deviation + (deviation - c_always_accepted_deviation) * sd / 600;
}
bool cTvspEpgOneDay::findTvspEvent(std::vector<cTvspEvent>::const_iterator &event_it, const cStaticEvent *event) const {
// return true if event was found
  if (m_events->size() == 0) return false;
//  esyslog("tvscraper: cTvspEpgOneDay::findTvspEvent, event->StartTime() %d", (int)event->StartTime() );
  event_it = std::lower_bound(m_events->begin(), m_events->end(), event->StartTime(),
    [](const cTvspEvent &e1, time_t en)
      { return e1.m_startTime < en; });
  if (event_it == m_events->end() ) {
    event_it--;
    return eventMatch(event_it, event) < c_never_accepted_deviation;
  }
  int dh = eventMatch(event_it, event);
  if (dh == 0) return true; // exact match found
  if (event_it == m_events->begin() ) return dh < c_never_accepted_deviation;
  event_it--;
  int dl = eventMatch(event_it, event);
  if (dh < dl) { dl = dh; event_it++; }
  return dl < c_never_accepted_deviation;
}

bool cTvspEpgOneDay::enhanceEvent(cStaticEvent *event, std::vector<cTvMedia> &extEpgImages) {
// return true if the event is in "my" time frame (one day )
  if (event->StartTime() <  m_start) return false;
  if (event->StartTime() >= m_end) return false;
// note: from here on, we always return true, because the event is in our time frame

  std::vector<cTvspEvent>::const_iterator event_it;
  if (!findTvspEvent(event_it, event)) {
//    esyslog("tvscraper: cTvspEpgOneDay::enhanceEvent, not found, event->StartTime() %d", (int)event->StartTime() );
    return true;
  }
// event found, set the event description
// note: VDR will remove control charaters (always): *p == 0x86 || *p == 0x87 || *p == 0x0D
//  if Setup.EPGBugfixLevel > 1: VDR will remove "space", following the first "space" character. "space" is everything < 31
  const rapidjson::Value& tvspEvent_j = (*m_document)[event_it->m_line];
  std::string descr;
  descr.reserve(700);
  const char *s;
  int i;
// Fazit / conclusion
  if (getValue(tvspEvent_j, "conclusion", s) ) { stringAppendRemoveControlCharacters(descr, s); descr += "\n"; }
// block: Genre, Kategorie, Land, Jahr
  if (getValue(tvspEvent_j, "genre", s) ) stringAppend(descr, "Genre: ", s, "\n");
  if (getValue(tvspEvent_j, "sart_id", s) ) {
    if (strcmp(s, "SP") == 0) descr += "Kategorie: Spielfilm\n";
    if (strcmp(s, "SE") == 0) descr += "Kategorie: Serie\n";
    if (strcmp(s, "SPO") == 0) descr += "Kategorie: Sport\n";
    if (strcmp(s, "KIN") == 0) descr += "Kategorie: Kinder\n";
    if (strcmp(s, "U") == 0) descr += "Kategorie: Show\n";
  }
  if (getValue(tvspEvent_j, "country", s) ) stringAppend(descr, "Land: ", s, "\n");
  if (getValue(tvspEvent_j, "year", i) ) stringAppend(descr, "Jahr: ", i, "\n");
  if (getValue(tvspEvent_j, "originalTitle", s) ) stringAppend(descr, "Originaltitel: ", s, "\n");
  if (getValue(tvspEvent_j, "fsk", i) ) stringAppend(descr, "FSK: ", i, "\n");
  descr += "\n";

// serie: episode, ...
  bool ra = false;
  if (getValue(tvspEvent_j, "episodeTitle", s) ) {
    descr += "Episode: ";
    stringAppendRemoveControlCharacters(descr, s);
    descr += "\n";
    if (getValue(tvspEvent_j, "seasonNumber", s) ) stringAppend(descr, "Staffel tvsp: ", s, "\n");
    if (getValue(tvspEvent_j, "episodeNumber", s) ) stringAppend(descr, "Folge tvsp: ", s, "\n");
    descr += "\n";
  }
// rating
  ra = false;
  if (getBool(tvspEvent_j, "isTipOfTheDay") == 1) { ra = true; descr += "Tagestipp";}
  if (getBool(tvspEvent_j, "isTopTip") == 1) { descr += ra?" / ":""; ra = true; descr += "Toptipp";}
  if (getValue(tvspEvent_j, "thumbIdNumeric", i) && i > 1) { descr += ra?" / ":""; ra = true; descr += (i==3)?"Sehr empfehlenswert":"Empfehlenswert"; }
  if (ra) descr += "\n";
  ra = false;
  if (getValue(tvspEvent_j, "ratingAction", i) && i > 0) { descr += ra?" / ":""; ra = true; descr += "Action "; descr += std::string(i, '*'); }
  if (getValue(tvspEvent_j, "ratingDemanding", i) && i > 0) { descr += ra?" / ":""; ra = true; descr += "Anspruch "; descr += std::string(i, '*'); }
  if (getValue(tvspEvent_j, "ratingErotic", i) && i > 0) { descr += ra?" / ":""; ra = true; descr += "Erotik "; descr += std::string(i, '*'); }
  if (getValue(tvspEvent_j, "ratingHumor", i) && i > 0) { descr += ra?" / ":""; ra = true; descr += "Humor "; descr += std::string(i, '*'); }
  if (getValue(tvspEvent_j, "ratingSuspense", i) && i > 0) { descr += ra?" / ":""; ra = true; descr += "Spannung "; descr += std::string(i, '*'); }
  if (ra) descr += "\n";


// preview & text
  if (getValue(tvspEvent_j, "preview", s) ) { descr += "\n"; stringAppendRemoveControlCharacters(descr, s); descr += "\n";}
  if (getValue(tvspEvent_j, "text", s) ) { descr += "\n"; stringAppendRemoveControlCharactersKeepNl(descr, s); descr += "\n";}
// actors
  bool first = true;
  rapidjson::Value::ConstMemberIterator actors_it = tvspEvent_j.FindMember("actors");
  if (actors_it != tvspEvent_j.MemberEnd() && actors_it->value.IsArray() ) {
// there is an actors array
    for (rapidjson::SizeType i = 0; i < actors_it->value.Size(); i++) {
      for (rapidjson::Value::ConstMemberIterator itr = actors_it->value[i].MemberBegin();
                                                itr != actors_it->value[i].MemberEnd(); ++itr) {
        if (itr->value.IsString()) {
          descr += first?"\nDarsteller: ":", ";
          first = false;
          stringAppendRemoveControlCharacters(descr, itr->value.GetString());
          const char *name = itr->name.GetString();
          if (name && *name) {
            descr += " (";
            stringAppendRemoveControlCharacters(descr, name);
            descr += ")";
          }
        }
      }
    }
  }
  if (!first) descr += "\n";
  if (getValue(tvspEvent_j, "director", s) ) stringAppend(descr, "\nRegisseur: ", s, "\n");
  descr += "\nQuelle: tvsp";
  if (!event->ShortText() || !*event->ShortText() &&
       event->Description() && strlen(event->Description()) > 3 && cSv(event->Description()).substr(0, 3) == "..." ) {
      event->SetShortText(event->Description() );
  }
  event->SetDescription(descr.c_str());
  if (!event->ShortText() || !*event->ShortText() ) {
// add a short text only if no short text is available from EIT (the TV station)
    if (getValue(tvspEvent_j, "episodeTitle", s) ) event->SetShortText(s);
    else if (getValue(tvspEvent_j, "conclusion", s) ) {
      std::string conclusion;
      stringAppendRemoveControlCharacters(conclusion, s);
      event->SetShortText(conclusion.c_str() );
    } else {
      std::string shortText;
      if (getValue(tvspEvent_j, "genre", s) ) shortText += s;
      if (getValue(tvspEvent_j, "country", s) ) { if (!shortText.empty()) shortText += " / "; shortText += s; }
      if (getValue(tvspEvent_j, "year", i) )    { if (!shortText.empty()) shortText += " / "; stringAppend(shortText, i); }
      event->SetShortText(shortText.c_str());
    }
  }

// image from external EPG provider
  for (auto &val: cJsonArrayIterator(tvspEvent_j, "images") ) {
    if (getValue(val, "size4", s) ) {
      cTvMedia tvMedia;
      tvMedia.width = 952;
      tvMedia.height = 714;
      tvMedia.path = s; // note: s is the URL
      extEpgImages.push_back(tvMedia);
      break;
// size1: 130x101
// size2: 320x250
// size3: 476x357
// size4: 952x714
    }
  }
  return true;
}


//***************************************************************************

extern "C" iExtEpg* extEpgPluginCreator(bool debug) {
// debug indicates to write more messages to syslog
  return new cExtEpgTvsp(debug);
}

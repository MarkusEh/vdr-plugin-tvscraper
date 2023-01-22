// cTvspEvent ***********************************************

cTvspEvent::cTvspEvent(const rapidjson::Value& tvspEvent, rapidjson::SizeType line):
  m_line(line)
{
  if (!getValue(tvspEvent, "timestart", m_startTime)) m_startTime = 0;
  if (!getValue(tvspEvent, "timeend", m_endTime)) m_endTime = 0;
}

// cTvspEpgOneDay ***********************************************

cTvspEpgOneDay::cTvspEpgOneDay(const sChannelMapEpg *channelMapEpg, time_t startTime):
  m_channelMapEpg(channelMapEpg),
  m_json(std::make_shared<cLargeString>("cTvspEpgOneDay", 20000)),
  m_document(std::make_shared<rapidjson::Document>() ),
  m_events(std::make_shared<std::vector<cTvspEvent>>() )
  {
    initJson(startTime);
  }


void cTvspEpgOneDay::initJson(time_t startTime) {
  const int hourDayBegin = 5; // the day beginns at 5:00 (from tvsp point of view ...)
  struct tm tm_r;
  struct tm *time = localtime_r(&startTime, &tm_r);
  if (time->tm_hour < hourDayBegin) {
// we have to go one day back, from tvsp point of view this is still the prev. day
    startTime -= hourDayBegin*60*60;
    time = localtime_r(&startTime, &tm_r);
  }
  std::stringstream date;
  date << "http://live.tvspielfilm.de/static/broadcast/list/";
  date << m_channelMapEpg->extid << "/";
  date << (time->tm_year + 1900) << '-'
  << std::setfill('0') << std::setw(2) << (time->tm_mon + 1)
  << '-' << std::setfill('0') << std::setw(2) << time->tm_mday;
  std::string url = date.str();
// if (m_channelMapEpg->extid[0] == 'A') esyslog("tvscraper epg about to download %s", url.c_str() );
// calculate m_start: today 5 am
  time->tm_sec = 0;
  time->tm_min = 0;
  time->tm_hour = hourDayBegin;
  m_start = mktime(time);
  m_end = m_start + 24*60*60;

// download json file
  curl_slist *headerList = NULL;
  headerList = curl_slist_append(headerList, "Accept: application/json");
//headerList = curl_slist_append(headerList, "If-None-Match: " + etag);

  if (!CurlGetUrl(url.c_str(), *m_json, headerList)) {
    esyslog("tvscraper: ERROR cTvspEpgOneDay::initJson, download %s failed", url.c_str() );
    return; // no data
  }
  m_document->ParseInsitu(m_json->data() );
  if (m_document->HasParseError() ) {
    esyslog("tvscraper: ERROR cTvspEpgOneDay::initJson, parse %s failed, error code %d", url.c_str(), (int)m_document->GetParseError () );
    return; // no data
  }
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

int cTvspEpgOneDay::eventMatch(vector<cTvspEvent>::const_iterator event_it, const cEvent *event) const {
  int deviationStart = std::abs(event_it->m_startTime - event->StartTime() );
  int deviationEnd   = std::abs(event_it->m_endTime - event->EndTime() );
  int deviation = deviationStart + deviationEnd;
  if (deviation <= c_always_accepted_deviation || deviation >= c_never_accepted_deviation) return deviation;
// complare event->Title() with event_it title
  const rapidjson::Value& tvspEvent_j = (*m_document)[event_it->m_line];
  const char *tvsp_title;
  if (!getValue(tvspEvent_j, "title", tvsp_title) ) return deviation;
  if (!tvsp_title || !event->Title() ) return deviation;
  int sd = sentence_distance(tvsp_title, event->Title() );  // sd == 0 if titles are identical
  if (sd > 600) return c_never_accepted_deviation;
  return c_always_accepted_deviation + (deviation - c_always_accepted_deviation) * sd / 600;
}
bool cTvspEpgOneDay::findTvspEvent(vector<cTvspEvent>::const_iterator &event_it, const cEvent *event) const {
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

bool cTvspEpgOneDay::enhanceEvent(cEvent *event) {
// return true if the event is in "my" time frame (one day )
  if (event->StartTime() <  m_start) return false;
  if (event->StartTime() >= m_end) return false;
// note: from here on, we always return true, because the event is in our time frame

  vector<cTvspEvent>::const_iterator event_it;
  if (!findTvspEvent(event_it, event)) {
//    esyslog("tvscraper: cTvspEpgOneDay::enhanceEvent, not found, event->StartTime() %d", (int)event->StartTime() );
    return true;
  }
// event found, set the event description
  const rapidjson::Value& tvspEvent_j = (*m_document)[event_it->m_line];
  std::string descr;
  const char *s;
  int i;
// Fazit / conclusion
  if (getValue(tvspEvent_j, "conclusion", s) ) { appendRemoveControlCharacters(descr, s); descr += "\n\n"; }
// block: Genre, Kategorie, Land, Jahr
  if (getValue(tvspEvent_j, "genre", s) ) { descr += "Genre: "; descr += s; descr += "\n"; }
  if (getValue(tvspEvent_j, "sart_id", s) ) {
    if (strcmp(s, "SP") == 0) descr += "Kategorie: Spielfilm\n";
    if (strcmp(s, "SE") == 0) descr += "Kategorie: Serie\n";
    if (strcmp(s, "SPO") == 0) descr += "Kategorie: Sport\n";
    if (strcmp(s, "KIN") == 0) descr += "Kategorie: Kinder\n";
    if (strcmp(s, "U") == 0) descr += "Kategorie: Show\n";
  }
  if (getValue(tvspEvent_j, "country", s) ) { descr += "Land: "; descr += s; descr += "\n"; }
  if (getValue(tvspEvent_j, "year", i) ) { descr += "Jahr: "; descr += to_string(i); descr += "\n"; }
  if (getValue(tvspEvent_j, "originalTitle", s) ) { descr += "Originaltitel: "; descr += s; descr += "\n"; }
  if (getValue(tvspEvent_j, "fsk", i) ) { descr += "FSK: "; descr += to_string(i); descr += "\n"; }
  descr += "\n";

// serie: episode, ...
  bool ra = false;
  if (getValue(tvspEvent_j, "episodeTitle", s) ) {
    descr += "Episode: ";
    appendRemoveControlCharacters(descr, s);
    descr += "\n";
    if (getValue(tvspEvent_j, "seasonNumber", s) ) { descr += "Staffel: "; descr += s; descr += "\n"; }
    if (getValue(tvspEvent_j, "episodeNumber", s) ) { descr += "Folge: "; descr += s; descr += "\n"; }
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
  if (getValue(tvspEvent_j, "preview", s) ) { descr += "\n"; appendRemoveControlCharacters(descr, s); descr += "\n";}
  if (getValue(tvspEvent_j, "text", s) ) { descr += "\n"; appendRemoveControlCharacters(descr, s); descr += "\n";}
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
          appendRemoveControlCharacters(descr, itr->value.GetString());
          descr += " (";
          appendRemoveControlCharacters(descr, itr->name.GetString());
          descr += ")";
        }
      }
    }
  }
  if (!first) descr += "\n";
  if (getValue(tvspEvent_j, "director", s) ) { descr += "\nRegisseur: "; descr += s; descr += "\n"; }
  descr += "\nQuelle: tvsp";
  event->SetDescription(descr.c_str());

// image from external EPG provider
  rapidjson::Value::ConstMemberIterator images_it = tvspEvent_j.FindMember("images");
  if (images_it != tvspEvent_j.MemberEnd() && images_it->value.IsArray() && images_it->value.Size() > 0) {
// there is an images array. Download first image
    if (getValue(images_it->value[0], "size4", s) ) {
      Download(s, getEpgImagePath(event, true));
    }
  }
  return true;
}

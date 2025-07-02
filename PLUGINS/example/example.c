#include "example.h"
#include <tools/curlfuncs.h>
#include <tools/fuzzy.c>

// see also https://github.com/google/gumbo-parser/blob/master/examples/clean_text.cc
#include <time.h>

static cSv get_text(GumboNode* node) {
  if (node->v.element.children.length == 0) return "get_text: children.length == 0";
  if (node->v.element.children.length >  1) return "get_text: children.length > 1";

  GumboNode* node_text = static_cast<GumboNode*>(node->v.element.children.data[0]);
  if (node_text->type == GUMBO_NODE_TEXT || node_text->type == GUMBO_NODE_WHITESPACE)
    return remove_trailing_whitespace(remove_leading_whitespace(node_text->v.text.text));
  return "node_text->type != GUMBO_NODE_TEXT && node_text->type != GUMBO_NODE_WHITESPACE";
}

static cSv get_text2(GumboNode* node_text) {
// return the text in the fist sub-node having text ...
  if (node_text->type == GUMBO_NODE_TEXT || node_text->type == GUMBO_NODE_WHITESPACE)
    return remove_trailing_whitespace(remove_leading_whitespace(node_text->v.text.text));

  if (node_text->type == GUMBO_NODE_ELEMENT &&
      node_text->v.element.tag != GUMBO_TAG_SCRIPT &&
      node_text->v.element.tag != GUMBO_TAG_STYLE) {
    GumboVector* children = &node_text->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
      cSv t = get_text2(static_cast<GumboNode*>(children->data[i]) );
      if (!t.empty()) return t;
    }
  }
  return cSv();
}

GumboNode* getNodeWithClass(GumboNode* node, cSv class_name) {
// return first node with class == class_name, in node and all subnodes

  if (node->type != GUMBO_NODE_ELEMENT) return nullptr;
  GumboAttribute* class_;
  if ((class_ = gumbo_get_attribute(&node->v.element.attributes, "class"))
      && cSv(class_->value) == class_name) return node;

  GumboVector* children = &node->v.element.children;
  for (unsigned int i = 0; i < children->length; ++i) {
    GumboNode* result = getNodeWithClass(static_cast<GumboNode*>(children->data[i]), class_name);
    if (result) return result;
  }
  return nullptr;
}
GumboNode* getNodeWithClass(GumboNode* node, GumboTag tag, cSv class_name) {
// return first node with class == class_name and tag == tag, in node and all subnodes

  if (node->type != GUMBO_NODE_ELEMENT) return nullptr;
  GumboAttribute* class_;
  if ((node->v.element.tag == tag)
      && (class_ = gumbo_get_attribute(&node->v.element.attributes, "class"))
      && cSv(class_->value) == class_name) return node;

  GumboVector* children = &node->v.element.children;
  for (unsigned int i = 0; i < children->length; ++i) {
    GumboNode* result = getNodeWithClass(static_cast<GumboNode*>(children->data[i]), tag, class_name);
    if (result) return result;
  }
  return nullptr;
}
GumboNode* getNode(GumboNode* node, GumboTag tag) {
// return first node with tag == tag, in node and all subnodes

  if (node->type != GUMBO_NODE_ELEMENT) return nullptr;
  if (node->v.element.tag == tag) return node;

  GumboVector* children = &node->v.element.children;
  for (unsigned int i = 0; i < children->length; ++i) {
    GumboNode* result = getNode(static_cast<GumboNode*>(children->data[i]), tag);
    if (result) return result;
  }
  return nullptr;
}
cSv get_shorttext(GumboNode* node) {
  GumboNode* headline = getNodeWithClass(node, GUMBO_TAG_H1, "headline headline--article broadcast stage-heading");
  if (!headline) return cSv();

  GumboVector* children = &headline->parent->v.element.children;
  for (unsigned int i = headline->index_within_parent + 1; i < children->length; ++i) {
    GumboNode* sibling = static_cast<GumboNode*>(children->data[i]);
    GumboAttribute* class_;
    if ((sibling->v.element.tag == GUMBO_TAG_DIV)
        && (class_ = gumbo_get_attribute(&sibling->v.element.attributes, "class"))
        && cSv(class_->value) == "stage-underline gray") {
      return get_text2(sibling);
    }
  }
  return cSv();
}

bool cTvspEpgOneDay::eventExists(time_t startTime) {
  for (auto &event: *m_events)
    if (event.m_startTime == startTime) return true;
  return false;
}
void cTvspEpgOneDay::addEvents(GumboNode* node) {
  if (node->type != GUMBO_NODE_ELEMENT) return;

  if (node->v.element.tag == GUMBO_TAG_A) {
    GumboAttribute* href  = gumbo_get_attribute(&node->v.element.attributes, "href");
    GumboAttribute* title  = gumbo_get_attribute(&node->v.element.attributes, "title");
    cSv node_string(m_html_string.data() + node->v.element.start_pos.offset, node->v.element.end_pos.offset - node->v.element.start_pos.offset);
    GumboAttribute* data_tracking_point = gumbo_get_attribute(&node->v.element.attributes, "data-tracking-point");
    GumboNode* node_strong = getNode(node, GUMBO_TAG_STRONG);

    if (href && title && data_tracking_point && node_strong) {
      cSv data_tracking_point_string;
      size_t found_s = node_string.find("data-tracking-point='{");
      if (found_s != cSv::npos) {
        size_t found_e = node_string.find("}'", found_s+21);
        if (found_e != cSv::npos) data_tracking_point_string = node_string.substr(found_s+21, found_e - found_s-20);
      }
// we need the manually found attribute as data_tracking_point is incomplete if there is a ' in data_tracking_point.
// the manually determined attribute ends only at "'}", and not at "'" -> it is complete
//      if (data_tracking_point_string != cSv(data_tracking_point->value) ) dsyslog3("data_tracking_point_string = '", data_tracking_point_string, "' data_tracking_point->value = '", data_tracking_point->value, "'");
      rapidjson::Document jd_data_tracking_point;
      std::string data_tracking_point_string_s(data_tracking_point_string);
      jd_data_tracking_point.ParseInsitu(data_tracking_point_string_s.data());
//    jd_data_tracking_point.Parse(data_tracking_point->value);
      if (jd_data_tracking_point.HasParseError() ) {
        dsyslog3("tvsp title = \"", title->value, "\", failed parsing data_tracking_point_string = \"", data_tracking_point_string, "\"");
//      dsyslog3("tvsp title = \"", title->value, "\", failed parsing data_tracking_point->value = \"", data_tracking_point->value, "\"");
      } else {
        rapidjson::Value::MemberIterator res = jd_data_tracking_point.FindMember("broadcastTime");
        if (res != jd_data_tracking_point.MemberEnd() && res->value.IsString() ) {
// convert time to epoch time
          struct tm s_tm;
          // std::cout << "strptime not processed " << strptime(res->value.GetString(), "%Y-%m-%dTi%H:%M:%S", &s_tm) << std::endl;
          strptime(res->value.GetString(), "%Y-%m-%dT%H:%M:%S", &s_tm);  // 2025-05-15T20:00:00+00:00
          s_tm.tm_isdst = -1;  // let mktime find out if daylight saving time (DST) is in effect
          time_t start_time = mktime(&s_tm);
          if (!eventExists(start_time) ) m_events->emplace_back(href->value, start_time, get_text2(node_strong));
        }
      }
    }
  }

  GumboVector* children = &node->v.element.children;
  for (unsigned int i = 0; i < children->length; ++i) {
    addEvents(static_cast<GumboNode*>(children->data[i]));
  }
}

template<size_t N>
void add_single_rating(cToSvConcat<N> &description, GumboNode* node) {
  if (node->type != GUMBO_NODE_ELEMENT) return;

  GumboAttribute* class_;
  if (node->v.element.tag == GUMBO_TAG_LI
      && (class_ = gumbo_get_attribute(&node->v.element.attributes, "class"))
      && cSv(class_->value) == "content-rating__rating-genre__list-item" ) {

    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
      GumboNode* child = static_cast<GumboNode*>(children->data[i]);
      if (child->v.element.tag == GUMBO_TAG_SPAN
        && (class_ = gumbo_get_attribute(&child->v.element.attributes, "class"))
        && cSv(class_->value) == "content-rating__rating-genre__list-item__label" ) {
        description << get_text2(child) << ": ";
        GumboNode* r = getNodeWithClass(node, "content-rating__rating-genre__list-item__rating rating-0");
        if (r) { description << "0\n"; return; }
        r = getNodeWithClass(node, "content-rating__rating-genre__list-item__rating rating-1");
        if (r) { description << "1\n"; return; }
        r = getNodeWithClass(node, "content-rating__rating-genre__list-item__rating rating-2");
        if (r) { description << "2\n"; return; }
        r = getNodeWithClass(node, "content-rating__rating-genre__list-item__rating rating-3");
        if (r) { description << "3\n"; return; }
        description << "not found\n"; return;
      }
    }
  }
}
template<size_t N>
void add_rating_d(cToSvConcat<N> &description, GumboNode* node_) {
  GumboNode* node = getNodeWithClass(node_, GUMBO_TAG_UL, "content-rating__rating-genre__list");
  if (!node) return;

  GumboVector* children = &node->v.element.children;
  for (unsigned int i = 0; i < children->length; ++i) {
    add_single_rating(description, static_cast<GumboNode*>(children->data[i]));
  }
}
template<size_t N>
void add_rating(cToSvConcat<N> &description, GumboNode* node) {
  GumboNode* node_r = getNodeWithClass(node, "content-rating__rating-genre");
  if (!node_r) return;
  description << "\n";
  GumboNode* node_thumb = getNodeWithClass(node_r, "content-rating__rating-genre__thumb rating-1");
// rating-1 -> thumb up
// rating-2 -> thumb right (geht so ...)
// rating-3 -> thumb down

  if (node_thumb) {
    GumboNode* node_top_rated = getNodeWithClass(node, "content-rating__top-rated"); // note: this is outside of node_r!
    if (node_top_rated) description << "Sehr empfehlenswert\n";
    else description << "Empfehlenswert\n";
  }
  add_rating_d(description, node_r);
  description << "\n";
}
template<size_t N>
void add_conclusion(cToSvConcat<N> &description, GumboNode* node_) {
  GumboNode* node = getNodeWithClass(node_, GUMBO_TAG_DIV, "content-rating__rating-genre__conclusion");
  if (!node) return;
  description << get_text2(node) << "\n";
}
template<size_t N>
void add_description(cToSvConcat<N> &description, GumboNode* node) {
  GumboNode* node_descr = getNodeWithClass(node, "broadcast-detail__description");
  if (!node_descr) return;
  GumboVector* children = &node_descr->v.element.children;
  for (unsigned int i = 0; i < children->length; ++i) {
    GumboNode* child = static_cast<GumboNode*>(children->data[i]);
    if (child->v.element.tag == GUMBO_TAG_P) description << get_text2(child) << "\n\n";
  }
}

template<size_t N>
void add_infos_crew_cast(cToSvConcat<N> &description, GumboNode* node) {
  if (node->type != GUMBO_NODE_ELEMENT) return;
  GumboAttribute* class_;
  if ((node->v.element.tag == GUMBO_TAG_DIV)
      && (class_ = gumbo_get_attribute(&node->v.element.attributes, "class"))
      && cSv(class_->value) == "definition-list") {

    int mode = 0; // 0-> attributes
    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
      if (static_cast<GumboNode*>(children->data[i])->v.element.tag == GUMBO_TAG_P) {
        cSv p_text = get_text(static_cast<GumboNode*>(children->data[i]) );
        if (p_text ==  "Crew") mode = 1; // Crew
        if (p_text ==  "Cast") mode = 2; // Cast
        if (mode > 0) description << "\n" << p_text << "\n\n";
      }
      if (static_cast<GumboNode*>(children->data[i])->v.element.tag == GUMBO_TAG_DL) {
        GumboVector* dl_children = &static_cast<GumboNode*>(children->data[i])->v.element.children;
        for (unsigned int dl_i = 0; dl_i < dl_children->length; ++dl_i) {
          if (static_cast<GumboNode*>(dl_children->data[dl_i])->v.element.tag == GUMBO_TAG_DT) {
            description << get_text(static_cast<GumboNode*>(dl_children->data[dl_i])) << ": ";
            while (static_cast<GumboNode*>(dl_children->data[dl_i])->v.element.tag != GUMBO_TAG_DD &&
                   dl_i < dl_children->length) ++dl_i;
            if (dl_i < dl_children->length) {
                description << get_text2(static_cast<GumboNode*>(dl_children->data[dl_i])) << "\n";
            } else description << " ERROR: no value found" << "\n";
          }
        }
      }
    }
  }
  GumboVector* children = &node->v.element.children;
  for (unsigned int i = 0; i < children->length; ++i) {
    add_infos_crew_cast(description, static_cast<GumboNode*>(children->data[i]));
  }
}

template<size_t N>
cSv add_infos_series(cToSvConcat<N> &description, GumboNode* node) {
  GumboNode* serial_info = getNodeWithClass(node, GUMBO_TAG_SECTION, "serial-info");
  if (!serial_info) return cSv();

  GumboNode* episode = getNodeWithClass(node, GUMBO_TAG_H2, "broadcast-info");
  if (!episode) return cSv();
  cSv episode_name = get_text2(episode);
  if (episode_name.empty()) return cSv();
  description << "Episode: " << episode_name << "\n";
  description << get_text2(serial_info) << "\n\n";
  return episode_name;
}

cSv cTvspEpgOneDay::get_img(GumboNode* node, cStaticEvent *event, cSv url) const {
  GumboNode* tv_detail = getNodeWithClass(node, GUMBO_TAG_DIV, "ratio-container  --tv-detail ");
  if (!tv_detail) {
    GumboNode* broadcast_detail = getNodeWithClass(node, GUMBO_TAG_SECTION, "broadcast-detail__stage ");
    if (!broadcast_detail) {
      if (++(*m_error_messages_img) < 3)
      dsyslog2("no EPG image found, neither 'ratio-container  --tv-detail ' nor 'broadcast-detail__stage ' found for event title '", event->Title(), "' short text '", event->ShortText(), "' start ", event->StartTime(), " channel ", event->ChannelID(), " url ", url);
      return cSv();
    }
    tv_detail = getNode(broadcast_detail, GUMBO_TAG_UNKNOWN); // GUMBO_TAG_PICTURE is not known
    if (!tv_detail) {
      if (++(*m_error_messages_img) < 3)
      dsyslog2("no EPG image found, picture tag (GUMBO_TAG_UNKNOWN) not found for event title '", event->Title(), "' short text '", event->ShortText(), "' start ", event->StartTime(), " channel ", event->ChannelID(), " url ", url );
      return cSv();
    }
  }
  GumboNode* img = getNode(tv_detail, GUMBO_TAG_IMG);
  if (!img) {
    if (++(*m_error_messages_img) < 3)
    dsyslog2("no EPG image found, img tag not found for event title '", event->Title(), "' short text '", event->ShortText(), "' start ", event->StartTime(), " channel ", event->ChannelID(), " url ", url );
    return cSv();
  }
  GumboAttribute* src = gumbo_get_attribute(&img->v.element.attributes, "src");
  if (!src) {
    if (++(*m_error_messages_img) < 3)
    dsyslog2("no EPG image found, src attribute not found for event title '", event->Title(), "' short text '", event->ShortText(), "' start ", event->StartTime(), " channel ", event->ChannelID(), " url ", url );
    return cSv();
  }
  return src->value;
}


// cTvspEvent ***********************************************

cTvspEvent::cTvspEvent(cSv url, time_t startTime, cSv tvspTitle):
  m_url(url),
  m_startTime(startTime),
  m_endTime(0),
  m_tvspTitle(tvspTitle) { }

// cTvspEpgOneDay ***********************************************

cTvspEpgOneDay::cTvspEpgOneDay(cCurl *curl, cSv extChannelId, time_t startTime, bool debug, int *error_messages_img):
  m_curl(curl),
  m_events(std::make_shared<std::vector<cTvspEvent>>() ),
  m_debug(debug),
  m_error_messages_img(error_messages_img)
  {
    initEvents(extChannelId, startTime);
  }


void cTvspEpgOneDay::initEvents(cSv extChannelId, time_t startTime) {
  const int hourDayBegin = 5; // the day beginns at 5:00 (from tvsp point of view ...)
  struct tm tm_r;
  struct tm *time = localtime_r(&startTime, &tm_r);
  if (time->tm_hour < hourDayBegin) {
// we have to go one day back, from tvsp point of view this is still the prev. day
    startTime -= hourDayBegin*60*60;
    time = localtime_r(&startTime, &tm_r);
  }

  for (int page = 1; page < 100; ++page) {
// https://www.tvspielfilm.de/tv-programm/sendungen/?page=2&date=2025-05-17&channel=ARD
    cToSvConcat url_for_one_day("https://www.tvspielfilm.de/tv-programm/sendungen/?page=", page, "&date=");
    url_for_one_day.appendInt<4>(time->tm_year + 1900).concat('-').
                    appendInt<2>(time->tm_mon + 1).concat('-').
                    appendInt<2>(time->tm_mday);
    url_for_one_day.concat("&channel=", extChannelId);

    if (page != 1 && m_html_string.find(url_for_one_day) == std::string::npos) break;
    m_html_string.clear();
    m_curl->GetUrl(url_for_one_day, m_html_string);
    GumboOutput* output = gumbo_parse(m_html_string.c_str());
    addEvents(output->root);

    gumbo_destroy_output(&kGumboDefaultOptions, output);
  }
  m_html_string.clear();

// calculate m_start: today 5 am
  time->tm_sec = 0;
  time->tm_min = 0;
  time->tm_hour = hourDayBegin;
  m_start = mktime(time);
  m_end = m_start + 24*60*60;

  if (m_events->size() != 0) std::sort(m_events->begin(), m_events->end());
}

int cTvspEpgOneDay::eventMatch(std::vector<cTvspEvent>::const_iterator event_it, const cStaticEvent *event) const {
  int deviationStart = std::abs(event_it->m_startTime - event->StartTime() );
  int deviation = deviationStart;
  if (deviation <= c_always_accepted_deviation || deviation >= c_never_accepted_deviation) return deviation;
// compare event->Title() with event_it title
  if (event_it->m_tvspTitle.empty() || !event->Title() ) return deviation;
  int sd = cNormedString(event->Title() ).sentence_distance(event_it->m_tvspTitle);  // sd == 0 if titles are identical
  if (sd > 600) return c_never_accepted_deviation;
  return c_always_accepted_deviation + (deviation - c_always_accepted_deviation) * sd / 600;
  return deviation;
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

// create descripion of event
  std::string html_string;
  m_curl->GetUrl(event_it->m_url, html_string);
  GumboOutput* output = gumbo_parse(html_string.c_str());

  cToSvConcat description;

  cSv short_text = get_shorttext(output->root);
  cSv episode_name = add_infos_series(description, output->root);
  add_conclusion(description, output->root);
  add_rating(description, output->root);
  add_description(description, output->root);
// attributes, start with blank line
  description << "\n";
  const_split_iterator sp_head(short_text, '|');
  ++sp_head;
  if (sp_head != iterator_end() ) description << "Genre: " << remove_trailing_whitespace(remove_leading_whitespace(*sp_head)) << "\n";
  else  description << "Genre: " << remove_trailing_whitespace(remove_leading_whitespace(short_text)) << "\n";
  add_infos_crew_cast(description, output->root);



  description += "\nQuelle: tvsp";
  if (!event->ShortText() || !*event->ShortText() &&
       event->Description() && strlen(event->Description()) > 3 && cSv(event->Description()).substr(0, 3) == "..." ) {
      event->SetShortText(event->Description() );
  }
  event->SetDescription(description.c_str());
  if (!event->ShortText() || !*event->ShortText() ) {
// add a short text only if no short text is available from EIT (the TV station)
    if (!episode_name.empty() ) event->SetShortText(cToSvConcat(episode_name).c_str() );
    else event->SetShortText(cToSvConcat(short_text).c_str() );
  }

// image from external EPG provider
  cSv img = get_img(output->root, event, event_it->m_url);
  if (!img.empty()) {
    cTvMedia tvMedia;
    tvMedia.width = 780;  // 640
    tvMedia.height = 438; // 360
    cSv::size_type q_pos = img.find('?');
    if (q_pos == cSv::npos) tvMedia.path = img;
    else tvMedia.path = img.substr(0, q_pos);
    extEpgImages.push_back(tvMedia);
  }

  gumbo_destroy_output(&kGumboDefaultOptions, output);
  return true;
}


//***************************************************************************

extern "C" iExtEpg* extEpgPluginCreator(bool debug) {
// debug indicates to write more messages to syslog
  return new cExtEpgTvsp(debug);
}

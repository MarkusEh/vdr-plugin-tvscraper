#include <string>
#include <sstream>
#include <vector>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "tvdbseries.h"
#include "tvdbmedia.h"
#include "tvdbactors.h"
#include "thetvdbscraper.h"

using namespace std;

cTVDBSeries::cTVDBSeries(cTVScraperDB *db, cTVDBScraper *TVDBScraper):
  m_db(db),
  m_TVDBScraper(TVDBScraper)
{
}

cTVDBSeries::~cTVDBSeries() {
}

void cTVDBSeries::ParseXML_search(xmlDoc *doc, vector<searchResultTvMovie> &resultSet, const string &SearchString) {
    if (doc == NULL) return;
    //Root Element has to be <Data>
    xmlNode *node = NULL;
    node = xmlDocGetRootElement(doc);
    if (!(node && !xmlStrcmp(node->name, (const xmlChar *)"Data")))
        return;
    //Searching for  <Series>
    node = node->children;
    xmlNode *cur_node = NULL;
    std::string SearchStringStripExtraUTF8 = stripExtraUTF8(SearchString.c_str() );
    for (cur_node = node; cur_node; cur_node = cur_node->next) {
        if ((cur_node->type == XML_ELEMENT_NODE) && !xmlStrcmp(cur_node->name, (const xmlChar *)"Series")) {
            ParseXML_searchSeries(doc, cur_node, resultSet, SearchStringStripExtraUTF8);
        }
    }
}
void cTVDBSeries::ParseXML_searchSeries(xmlDoc *doc, xmlNode *node, vector<searchResultTvMovie> &resultSet, const string &SearchStringStripExtraUTF8) {
    if (!node) return;
// read the series
    node = node->children;
    xmlChar *node_content;
    xmlNode *cur_node = NULL;
    string aliasNames;
    for (cur_node = node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            node_content = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
            if (!node_content)
                continue;
            if (!xmlStrcmp(cur_node->name, (const xmlChar *)"seriesid")) {
                seriesID = atoi((const char *)node_content);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"SeriesName")) {
                name = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"AliasNames")) {
                aliasNames = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"FirstAired")) {
                firstAired = (const char *)node_content;
            }
            xmlFree(node_content);
        }
    }
    if(seriesID == 0) return;
// is this series already in the list?
    for (const searchResultTvMovie &sRes: resultSet ) if (sRes.id() == seriesID * (-1) ) return;
    bool debug = false;
//    debug = SearchString == "cars toons";
//    debug = seriesID == 250498;
    transform(name.begin(), name.end(), name.begin(), ::tolower);
    transform(aliasNames.begin(), aliasNames.end(), aliasNames.begin(), ::tolower);

    searchResultTvMovie sRes(seriesID * (-1), false, firstAired);
    sRes.setPositionInExternalResult(resultSet.size() );
    int dist_a = sentence_distance_normed_strings(stripExtraUTF8(name.c_str() ), SearchStringStripExtraUTF8);
    if (debug) esyslog("tvscraper: series SearchString %s, name %s, distance %i", SearchStringStripExtraUTF8.c_str(), name.c_str(), dist_a);
// (2013) or similar at the end of a name in thetvdb indicates a year. This year is not given in the EPG. 
    if (StringRemoveLastPartWithP(name) ) dist_a = std::min(dist_a, sentence_distance_normed_strings(stripExtraUTF8(name.c_str() ), SearchStringStripExtraUTF8) );
    std::size_t lDelim = aliasNames.find('|');
    if (lDelim !=std::string::npos) {
      for (std::size_t rDelim = aliasNames.find('|', lDelim +1); rDelim != std::string::npos; rDelim = aliasNames.find('|', lDelim +1) ) {
        int dist = sentence_distance_normed_strings(stripExtraUTF8(aliasNames.substr(lDelim +1, rDelim - lDelim - 1).c_str() ), SearchStringStripExtraUTF8);
        if (debug) esyslog("tvscraper: series SearchString \"%s\", alias name \"%s\", distance %i", SearchStringStripExtraUTF8.c_str(), aliasNames.substr(lDelim +1, rDelim - lDelim - 1).c_str(), dist);
        if (dist < dist_a) dist_a = dist;
        lDelim = rDelim;
      }
    }
    sRes.setMatchText(dist_a);
    resultSet.push_back(sRes);
}

void cTVDBSeries::ParseXML_all(xmlDoc *doc) {
    if (doc == NULL) return;
//Root Element has to be <Data>
    xmlNode *node = NULL;
    node = xmlDocGetRootElement(doc);
    if (!(node && !xmlStrcmp(node->name, (const xmlChar *)"Data")))
        return;
    //Searching for  <Series>
    node = node->children;
    xmlNode *cur_node = NULL;
    for (cur_node = node; cur_node; cur_node = cur_node->next) {
        if ((cur_node->type == XML_ELEMENT_NODE) && !xmlStrcmp(cur_node->name, (const xmlChar *)"Series")) {
            ParseXML_Series(doc, cur_node); // this also reads the seriesID
            StoreDB(m_db);
        } else if ((cur_node->type == XML_ELEMENT_NODE) && !xmlStrcmp(cur_node->name, (const xmlChar *)"Episode")) {
            ParseXML_Episode(doc, cur_node);
        }
    }
    m_db->TvSetEpisodesUpdated(seriesID * (-1) );
}
void cTVDBSeries::ParseXML_Series(xmlDoc *doc, xmlNode *node) {
    if (!node) return;
//read the series
    xmlNode *cur_node = NULL;
    node = node->children;
    xmlChar *node_content;
    episodeRunTimes.clear();
    for (cur_node = node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            node_content = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
            if (!node_content)
                continue;
            if (!xmlStrcmp(cur_node->name, (const xmlChar *)"id")) {
                seriesID = atoi((const char *)node_content);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"SeriesName")) {
                name = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Overview")) {
                overview = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"banner")) {
                banner = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"poster")) {
                poster = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"fanart")) {
                fanart = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"FirstAired")) {
                firstAired = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"IMDB_ID")) {
                IMDB_ID = (const char *)node_content;
// ignore Actors here. Actors (including images) are taken from the <Actors> section
//            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Actors")) {
//                actors = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Genre")) {
                genres = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Network")) {
                networks = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Rating")) {
                rating = atof( (const char *)node_content );
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"RatingCount")) {
                ratingCount = atoi( (const char *)node_content );
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Runtime")) {
                runtime = atof( (const char *)node_content );
                if (runtime) episodeRunTimes.push_back(runtime);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Status")) {
                status = (const char *)node_content;
            }
            xmlFree(node_content);
        }
    }
}
void cTVDBSeries::ParseXML_Episode(xmlDoc *doc, xmlNode *node) {
    if (!node) return;
//read the episode
    int episodeID = 0;
    int seasonNumber = 0;
    int episodeNumber = 0;
    int episodeAbsoluteNumber = 0;
    string episodeName("");
    string episodeOverview("");
    string episodeFirstAired("");
    string episodeGuestStars("");
    string episodeFilename("");
    string episodeDirector("");
    string episodeWriter("");
    string episodeIMDB_ID("");
    float episodeRating = 0.0;
    int episodeVoteCount = 0;

    xmlNode *cur_node = NULL;
    node = node->children;
    xmlChar *node_content;
    for (cur_node = node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            node_content = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
            if (!node_content) continue;
            if (!xmlStrcmp(cur_node->name, (const xmlChar *)"id")) {
                episodeID = atoi((const char *)node_content);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"SeasonNumber")) {
                seasonNumber = atoi((const char *)node_content);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"EpisodeNumber")) {
                episodeNumber = atoi((const char *)node_content);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"absolute_number")) {
                episodeAbsoluteNumber = atoi((const char *)node_content);
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"EpisodeName")) {
                episodeName = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Overview")) {
                episodeOverview = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"FirstAired")) {
                episodeFirstAired = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"GuestStars")) {
                episodeGuestStars = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Rating")) {
                episodeRating = atof( (const char *)node_content );
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"RatingCount")) {
                episodeVoteCount = atoi( (const char *)node_content );
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Director")) {
                episodeDirector = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Writer")) {
                episodeWriter = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"IMDB_ID")) {
                episodeIMDB_ID = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"filename")) {
                episodeFilename = (const char *)node_content;
            }
            xmlFree(node_content);
        }
    }
    m_db->InsertTv_s_e(seriesID * (-1), seasonNumber, episodeNumber, episodeAbsoluteNumber, episodeID, episodeName, episodeFirstAired, episodeRating, episodeVoteCount, episodeOverview, episodeGuestStars, episodeDirector, episodeWriter, episodeIMDB_ID, episodeFilename);
}

void cTVDBSeries::StoreDB(cTVScraperDB *db) {
    db->InsertTv(seriesID * (-1), name, "", overview, firstAired, networks, genres, 0.0, rating, ratingCount, poster, fanart, IMDB_ID, status, episodeRunTimes, "");
}

void cTVDBSeries::StoreMedia(int tvID) {
    if (banner.size() > 0) m_db->insertTvMedia (tvID *-1, banner, mediaBanner);
    if (poster.size() > 0) m_db->insertTvMedia (tvID *-1, poster, mediaPoster);
    if (fanart.size() > 0) m_db->insertTvMedia (tvID *-1, fanart, mediaFanart);
}

void cTVDBSeries::Dump() {
    esyslog("tvscraper: series %s, id: %d, overview %s, imdb %s", name.c_str(), seriesID, overview.c_str(), IMDB_ID.c_str());
}

bool cTVDBSeries::AddResults(vector<searchResultTvMovie> &resultSet, const string &SearchString, const string &SearchString_ext) {
// search for tv series, add search results to resultSet
   stringstream url;
   url << m_TVDBScraper->GetMirrors()->GetMirrorXML() << "/api/GetSeries.php?seriesname=" << CurlEscape(SearchString_ext.c_str()) << "&language=" << m_TVDBScraper->GetLanguage().c_str();
    string seriesXML;
    if (config.enableDebug) esyslog("tvscraper: calling %s", url.str().c_str());
    if (!CurlGetUrl(url.str().c_str(), seriesXML)) return false;
    seriesID = 0;
    xmlInitParser();
    xmlDoc *doc = xmlReadMemory(seriesXML.c_str(), strlen(seriesXML.c_str()), "noname.xml", NULL, 0);
    ParseXML_search(doc, resultSet, SearchString);
    if(doc) xmlFreeDoc(doc);
    if(seriesID == 0) return false;
    return true;
}

bool cTVDBSeries::AddResults4(vector<searchResultTvMovie> &resultSet, const string &SearchString, const string &SearchString_ext) {
// search for tv series, add search results to resultSet
// return true if results were added
  if (!m_TVDBScraper->GetToken() ) return false;
  string out;
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "accept: application/json");
  headers = curl_slist_append(headers, m_TVDBScraper->tokenHeader.c_str() );
  headers = curl_slist_append(headers, "charset: utf-8");
  string url = m_TVDBScraper->baseURL4Search + CurlEscape(SearchString_ext.c_str());
  if (config.enableDebug) esyslog("tvscraper: calling %s", url.c_str());
  bool result =  CurlGetUrl(url.c_str(), out, headers);
  curl_slist_free_all(headers);
  if (!result) {
    esyslog("tvscraper: ERROR calling %s", url.c_str());
    return false;
  }
  json_t *root = json_loads(out.c_str(), 0, NULL);
  if (!root) {
    esyslog("tvscraper: ERROR cTVDBSeries::AddResults4, parsing %s", out.substr(0, 50).c_str());
    return false;
  }

  seriesID = 0;
  result = ParseJson_search(root, resultSet, SearchString);
  json_decref(root);
  if (!result) {
    esyslog("tvscraper: ERROR cTVDBSeries::AddResults4, json parsing %s", out.substr(0, 100).c_str());
    return false;
  }
  if(seriesID == 0) return false;
  return true;
}

bool cTVDBSeries::ParseJson_search(json_t *root, vector<searchResultTvMovie> &resultSet, const string &SearchString) {
  if (root == NULL) return false;
  if (json_string_value_validated(root, "status").compare("success") != 0) {
    esyslog("tvscraper: ERROR getting thetvdb search result, status = %s", json_string_value_validated(root, "status").c_str() );
    return false;
  }
  json_t *jData = json_object_get(root, "data");
  if(!json_is_array(jData))  {
    esyslog("tvscraper: ERROR parsing thetvdb search result, jData is not an array");
    return false;
  }
  std::string SearchStringStripExtraUTF8 = stripExtraUTF8(SearchString.c_str() );
  size_t index;
  json_t *jElement;
  json_array_foreach(jData, index, jElement) {
    ParseJson_searchSeries(jElement, resultSet, SearchStringStripExtraUTF8);
  }
  return true;
}

int minDist(int dist, const json_t *jString, const string &SearchStringStripExtraUTF8) {
// compare string in jString with SearchStringStripExtraUTF8
// make sanity chaecks first
  if (!jString || !json_is_string(jString)) return dist;
  const char *name = json_string_value(jString);
  if (!name || !*name) return dist;

  dist = std::min(dist, sentence_distance_normed_strings(stripExtraUTF8(name), SearchStringStripExtraUTF8) );
  int len = StringRemoveLastPartWithP(name, (int)strlen(name) );
  if (len != -1) dist = std::min(dist, sentence_distance_normed_strings(stripExtraUTF8(name, len), SearchStringStripExtraUTF8) );
  return dist;
}


void cTVDBSeries::ParseJson_searchSeries(json_t *data, vector<searchResultTvMovie> &resultSet, const string &SearchStringStripExtraUTF8) {
// add search results to resultSet
  if (!data) return;
  std::string objectID = json_string_value_validated(data, "objectID");
  if (objectID.length() < 8) {
    esyslog("tvscraper: ERROR cTVDBSeries::ParseJson_searchSeries, objectID.length() < 8, %s", objectID.c_str() );
    return;
  }
  if (objectID.compare(0, 7, "series-") != 0) {
    esyslog("tvscraper: ERROR cTVDBSeries::ParseJson_searchSeries, objectID does not start with series-, %s", objectID.c_str() );
    return;
  }
  seriesID = atoi(objectID.c_str() + 7);
  if (seriesID == 0) {
    esyslog("tvscraper: ERROR cTVDBSeries::ParseJson_searchSeries, seriesID = 0, %s", objectID.c_str() );
    return;
  }
// is this series already in the list?
  for (const searchResultTvMovie &sRes: resultSet ) if (sRes.id() == seriesID * (-1) ) return;
  bool debug = false;
//    debug = SearchString == "cars toons";
//    debug = seriesID == 250498;

  searchResultTvMovie sRes(seriesID * (-1), false, json_string_value_validated(data, "year"));
  sRes.setPositionInExternalResult(resultSet.size() );
  int dist_a = minDist(1000, json_object_get(data, "name"), SearchStringStripExtraUTF8);
  if (debug) esyslog("tvscraper: ParseJson_searchSeries SearchString %s, name %s, distance %i", SearchStringStripExtraUTF8.c_str(), json_string_value_validated(data, "name").c_str() , dist_a);
  json_t *jAliases = json_object_get(data, "aliases");
  if (json_is_array(jAliases) ) {
    size_t index;
    json_t *jElement;
    json_array_foreach(jAliases, index, jElement) {
      dist_a = minDist(dist_a, jElement, SearchStringStripExtraUTF8);
    }
  }
  json_t *jTranslations = json_object_get(data, "translations");
  if (json_is_object(jTranslations) ) {
    const char *key;
    json_t *value;
    json_object_foreach(jTranslations, key, value) {
      dist_a = minDist(dist_a, value, SearchStringStripExtraUTF8);
    }
  }
  sRes.setMatchText(dist_a);
  resultSet.push_back(sRes);
}


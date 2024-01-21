#include <map>
#include "overrides.h"

using namespace std;

cOverRides::cOverRides(void) {
}

cOverRides::~cOverRides() {
}

void cOverRides::ReadConfig(cSv confDir) {
  cToSvConcat confFile(confDir, "/override.conf");
  dsyslog("tvscraper: reading overrides from %s", confFile.c_str());
  if (access(confFile.c_str(), F_OK) == 0) {
    FILE *f = fopen(confFile.c_str(), "r");
    if (f) {
      char *s;
      cReadLine ReadLine;
      while ((s = ReadLine.Read(f)) != NULL) {
        char *p = strchr(s, '#');
        if (p) *p = 0;
        stripspace(s);
        if (!isempty(s)) ReadConfigLine(s);
      }
      fclose(f);
    }
  }
}

bool cRegexAction::set_dbid(cSv edb, cSv id, bool seriesRequired) {
// true on success
  if (edb == "TheTVDB_Series") {
    m_movie = false;
    m_dbid = -parse_int<int>(id);
    return true;
  }
  m_dbid = parse_int<int>(id);
  if (edb == "TheMovieDB_Series") {
    m_movie = false;
    return true;
  }
  if (!seriesRequired && edb == "TheMovieDB_Movie") {
    m_movie = true;
    return true;
  }
  m_regexTitle = std::regex();
  esyslog("tvscraper, ERROR in override.conf, %s", cToSvConcat(edb, " given, expected TheTVDB_Series or TheMovieDB_Series", !seriesRequired?" or TheMovieDB_Movie":"").c_str());
  return false;
}
std::regex getRegex(cSv sv) {
  try {
    std::regex result;
    result.imbue(g_locale);
    result.assign(sv.data(), sv.length(), std::regex_constants::ECMAScript | std::regex_constants::optimize | std::regex_constants::collate);
    return result;
  } catch (const std::regex_error& e)
  {
    esyslog("%s", cToSvConcat("tvscraper, ERROR ", e.what(), " in regex ", sv).c_str() );
    return std::regex();
  }
}

void cOverRides::ReadConfigLine(const char *line) {
  vector<cSv> flds = getSetFromString<cSv, vector<cSv>>(line);
  if (flds.size() > 0) {
    if (!flds[0].compare("ignore")) {
      if (flds.size() == 2) {
        ignores.push_back(string(flds[1]));
      }
    } else if (!flds[0].compare("settype")) {
      if (flds.size() == 3) {
        if (!flds[2].compare("series")) {
          searchTypes.insert(pair<string, scrapType>(flds[1], scrapSeries));
        } else if (!flds[2].compare("movie")) {
          searchTypes.insert(pair<string, scrapType>(flds[1], scrapMovie));
        }
      }
    } else if (!flds[0].compare("substitute")) {
      if (flds.size() == 3) {
        substitutes.insert(pair<string, string>(flds[1], flds[2]));
      }
    } else if (!flds[0].compare("ignorePath")) {
      if (flds.size() == 2) {
        if (flds[1].find("/", flds[1].length()-1) != std::string::npos)
          ignorePath.push_back(string(flds[1]));
        else
          ignorePath.push_back(string(flds[1]) + "/");
      }
    } else if (!flds[0].compare("removePrefix")) {
        removePrefixes.push_back(string(flds[1]));
    } else if (!flds[0].compare("TheTVDB_SeriesID")) {
      if (flds.size() == 3) {
        m_TheTVDB_SeriesID.insert(pair<std::string, int>(std::string(flds[1]), parse_int<int>(flds[2])));
      }
    } else if (!flds[0].compare("TheMovieDB_SeriesID")) {
      if (flds.size() == 3) {
        m_TheMovieDB_SeriesID.insert(pair<std::string, int>(std::string(flds[1]), parse_int<int>(flds[2])));
      }
    } else if (!flds[0].compare("TheMovieDB_MovieID")) {
      if (flds.size() == 3) {
        m_TheMovieDB_MovieID.insert(pair<std::string, int>(std::string(flds[1]), parse_int<int>(flds[2])));
      }
    } else if (!flds[0].compare("regexTitleChannel->id")) {
      if (flds.size() == 5) {
        cRegexAction regexAction;
        regexAction.m_matchShortText = false;
        regexAction.m_regexTitle = getRegex(flds[1]);
        regexAction.m_matchChannel = !flds[2].empty();
        if (regexAction.m_matchChannel) regexAction.m_regexChannel = getRegex(flds[2]);
        regexAction.set_dbid(flds[3], flds[4], false);
        regexAction.m_matchPurpouse = eMatchPurpouse::regexTitleChannel_id;
        m_regex.push_back(regexAction);
      }
    } else if (!flds[0].compare("regexTitleChannel->idEpisodeName")) {
      if (flds.size() == 5) {
        cRegexAction regexAction;
        regexAction.m_matchShortText = false;
        regexAction.m_regexTitle = getRegex(flds[1]);
        if (regexAction.m_regexTitle.mark_count() != 1) {
          esyslog("tvscraper, ERROR in regex %s", cToSvConcat(flds[1], ", expected 1 capture group, found ", regexAction.m_regexTitle.mark_count(), " capture groups").c_str());
          regexAction.m_regexTitle = std::regex();
        }
        regexAction.m_matchChannel = !flds[2].empty();
        if (regexAction.m_matchChannel) regexAction.m_regexChannel = getRegex(flds[2]);
        regexAction.m_dbid = parse_int<int>(flds[4]);
        if (flds[3] == "TheTVDB_Series") regexAction.m_dbid = -regexAction.m_dbid;
        regexAction.m_movie = false;
        regexAction.m_matchPurpouse = eMatchPurpouse::regexTitleChannel_idEpisodeName;
        m_regex.push_back(regexAction);
      }
    } else if (!flds[0].compare("regexTitleShortTextChannel->idEpisodeName")) {
      if (flds.size() == 6) {
        cRegexAction regexAction;
        regexAction.m_regexTitle = getRegex(flds[1]);
        regexAction.m_matchShortText = true;
        regexAction.m_regexShortText = getRegex(flds[2]);
        if ((regexAction.m_regexTitle.mark_count() + regexAction.m_regexShortText.mark_count()) != 1) {
          esyslog("tvscraper, ERROR in regex %s", cToSvConcat(flds[1], ", st regex ", flds[2], ", expected 1 capture group, found ", regexAction.m_regexTitle.mark_count(), " capture groups in title and ", regexAction.m_regexShortText.mark_count(), " capture groups in short text").c_str());
          regexAction.m_regexTitle = std::regex();
        }
        regexAction.m_matchChannel = !flds[3].empty();
        if (regexAction.m_matchChannel) regexAction.m_regexChannel = getRegex(flds[3]);
        regexAction.m_dbid = parse_int<int>(flds[5]);
        if (flds[4] == "TheTVDB_Series") regexAction.m_dbid = -regexAction.m_dbid;
        regexAction.m_movie = false;
        regexAction.m_matchPurpouse = eMatchPurpouse::regexTitleChannel_idEpisodeName;
        m_regex.push_back(regexAction);
      }
    } else if (!flds[0].compare("regexTitleShortTextChannel->idSeasonNumberEpisodeNumber")) {
      if (flds.size() == 6) {
        cRegexAction regexAction;
        regexAction.m_regexTitle = getRegex(flds[1]);
        regexAction.m_matchShortText = true;
        regexAction.m_regexShortText = getRegex(flds[2]);
        regexAction.m_matchChannel = !flds[3].empty();
        if (regexAction.m_matchChannel) regexAction.m_regexChannel = getRegex(flds[3]);
        regexAction.m_dbid = parse_int<int>(flds[5]);
        if (flds[4] == "TheTVDB_Series") regexAction.m_dbid = -regexAction.m_dbid;
        regexAction.m_movie = false;
        regexAction.m_matchPurpouse = eMatchPurpouse::regexTitleShortTextChannel_idSeasonNumberEpisodeNumber;
        m_regex.push_back(regexAction);
      }
    }
  }
}

bool cOverRides::Ignore(cSv title) {
  for (const string &pos: ignores) {
    if (title == pos) {
//      if (config.enableDebug) esyslog("tvscraper: ignoring \"%.*s\" because of override.conf", (int)title.length(), title.data());
      return true;
    }
  }
  return false;
}

bool cOverRides::Substitute(string &title) {
    map<string,string>::iterator hit = substitutes.find(title);
    if (hit != substitutes.end()) {
        if (config.enableDebug)
            esyslog("tvscraper: substitute \"%s\" with \"%s\" because of override.conf", title.c_str(), ((string)hit->second).c_str());
        title = (string)hit->second;
        return true;
    }
    return false;
}

int cOverRides::TheTVDB_SeriesID(cSv title) {
  auto hit = m_TheTVDB_SeriesID.find(title);
  if (hit == m_TheTVDB_SeriesID.end()) return 0;
//  if (config.enableDebug)
//    esyslog("tvscraper: title \"%.*s\", use TheTVDB_SeriesID %i override.conf", (int)title.length(), title.data(), (int)hit->second);
  return hit->second;
}

int cOverRides::TheMovieDB_SeriesID(cSv title) {
  auto hit = m_TheMovieDB_SeriesID.find(title);
  if (hit == m_TheMovieDB_SeriesID.end()) return 0;
//  if (config.enableDebug)
//    esyslog("tvscraper: title \"%.*s\", use TheMovieDB_SeriesID %i override.conf", (int)title.length(), title.data(), (int)hit->second);
  return hit->second;
}

int cOverRides::TheMovieDB_MovieID(cSv title) {
  auto hit = m_TheMovieDB_MovieID.find(title);
  if (hit == m_TheMovieDB_MovieID.end()) return 0;
//  if (config.enableDebug)
//    esyslog("tvscraper: title \"%.*s\", use TheMovieDB_MovieID %i override.conf", (int)title.length(), title.data(), (int)hit->second);
  return hit->second;
}

bool regexGetSeasonEpisode(cSv text, const std::regex &regexShortText, int &season, int &episode) {
  std::cmatch sub_match;
  if (text.empty() ) return false;
  if (!std::regex_match(text.data(), text.data()+text.length(), sub_match, regexShortText)) return false;
  if (sub_match.size() != 3) {
    esyslog("tvscraper, ERROR in cRegexAction::matches, matchPurpouse::regexTitleShortTextChannel_idSeasonNumberEpisodeNumber, sub_match.size() %zu", sub_match.size());
    return false;
  }
  season = parse_int<int>(sub_match[1].str());
  episode = parse_int<int>(sub_match[2].str());
  return true;
}
bool cRegexAction::matches(cSv title, cSv shortText, cSv description, cSv channel, int &season, int &episode, std::string &episodeName) const {
  std::cmatch title_match, shortText_match;
  if (!std::regex_match(title.data(), title.data()+title.length(), title_match, m_regexTitle)) return false;
  if (m_matchChannel && !std::regex_match(channel.data(), channel.data()+channel.length(), m_regexChannel)) return false;
  switch (m_matchPurpouse) {
    case eMatchPurpouse::regexTitleChannel_id:
      return true;
    case eMatchPurpouse::regexTitleChannel_idEpisodeName:
      if (m_matchShortText && !std::regex_match(shortText.data(), shortText.data()+shortText.length(), shortText_match, m_regexShortText)) return false;
      if (title_match.size() == 2) {
        episodeName = title_match[1].str();
        return true;
      }
      if (shortText_match.size() == 2) {
        episodeName = shortText_match[1].str();
        return true;
      }
      esyslog("tvscraper, ERROR in cRegexAction::matches, matchPurpouse::regexTitleChannel_idEpisodeName, title_match.size() %zu, shortText_match.size() %zu", title_match.size(), shortText_match.size() );
      return true;
    case eMatchPurpouse::regexTitleShortTextChannel_idSeasonNumberEpisodeNumber:
      if (regexGetSeasonEpisode(shortText, m_regexShortText, season, episode)) return true;
      if (regexGetSeasonEpisode(description, m_regexShortText, season, episode)) return true;
      return false;
// TODO case eMatchPurpouse::regexTitleChannel_idEpisodeAbsolutNumber
// # regexTitleChannel->idEpisodeAbsolutNumber;regexTitle;regexChannel or empty;TheMovieDB_Series or TheTVDB_Series;id  // first match of regexTitle will be used as episode absolut number
    default:
      esyslog("tvscraper, ERROR in cRegexAction::matches, matchPurpouse %d unknown", (int)m_matchPurpouse);
  }
  return false;
}
bool cOverRides::regex(cSv title, cSv shortText, cSv description, cSv channel, eMatchPurpouse &matchPurpouse, int &dbid, bool &movie, int &season, int &episode, std::string &episodeName) {
  for (const cRegexAction &r: m_regex) if (r.matches(title, shortText, description, channel, season, episode, episodeName)) {
    matchPurpouse = r.m_matchPurpouse;
    dbid = r.m_dbid;
    movie = r.m_movie;
//    if (config.enableDebug)
//      esyslog("tvscraper: title \"%.*s\", use dbid %i override.conf", (int)title.length(), title.data(), dbid);
    return true;
  }
  return false;
}

void cOverRides::RemovePrefix(string &title) {
  size_t nTitle = title.length();
  for (const string &prefix: removePrefixes) {
    size_t nPrefix = prefix.length();
    if (nPrefix >= nTitle - 3) continue;
    if (!title.compare(0, nPrefix, prefix) ) {
      for (; nPrefix < nTitle - 3 && title[nPrefix] == ' '; nPrefix++);
      if (nPrefix < nTitle - 3) title.erase(0, nPrefix);
      return;
    }
  }
}

scrapType cOverRides::Type(cSv title) {
    map<string,scrapType, less<>>::iterator hit = searchTypes.find(title);
    if (hit != searchTypes.end()) {
        if (config.enableDebug)
            esyslog("tvscraper: using type %d for \"%.*s\" because of override.conf", (int)hit->second, (int)title.length(), title.data());
        return (scrapType)hit->second;
    }
    return scrapNone;
}

bool cOverRides::IgnorePath(cSv path) {
  for (const string &pos: ignorePath) {
    if (path.find(pos) != string::npos) {
      if (config.enableDebug) esyslog("tvscraper: ignoring path \"%.*s\" because of override.conf", (int)path.length(), path.data());
      return true;
    }
  }
  return false;
}

void cOverRides::Dump(void) {
    esyslog("tvscraper: ignoring the following titles:");
    vector<string>::iterator pos;
    for ( pos = ignores.begin(); pos != ignores.end(); ++pos) {
        esyslog("tvscraper: ignore \"%s\"", pos->c_str());
    }
    
    esyslog("tvscraper: fixed scrap types for the following titles:");
    map<string,scrapType>::iterator pos2;
    for( pos2 = searchTypes.begin(); pos2 != searchTypes.end(); ++pos2) {
        esyslog("tvscraper: title \"%s\" has Type %d", ((string)pos2->first).c_str(), (int)pos2->second);
    }

    esyslog("tvscraper: substitutions for the following titles:");
    map<string,string>::iterator pos3;
    for( pos3 = substitutes.begin(); pos3 != substitutes.end(); ++pos3) {
        esyslog("tvscraper: substitute \"%s\" with \"%s\"", ((string)pos3->first).c_str(), ((string)pos3->second).c_str());
    }
    
    esyslog("tvscraper: ignoring the following paths:");
    vector<string>::iterator pos4;
    for ( pos4 = ignorePath.begin(); pos4 != ignorePath.end(); ++pos4) {
        esyslog("tvscraper: ignore path \"%s\"", pos4->c_str());
    }
}

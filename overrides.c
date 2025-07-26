#include <map>
#include "overrides.h"

using namespace std;

cOverRides::cOverRides(void) {
}

cOverRides::~cOverRides() {
}

void cOverRides::ReadConfig(cSv dir, cSv file) {
  cToSvConcat confFile(dir, "/", file);
  dsyslog("tvscraper: reading overrides from %s", confFile.c_str());
  if (access(confFile.c_str(), F_OK) == 0) {
    FILE *f = fopen(confFile.c_str(), "r");
    if (f) {
      char *s;
      cReadLine ReadLine;
      while ((s = ReadLine.Read(f)) != NULL) {
        if (*s == '#') *s = 0;
        stripspace(s);
        if (!isempty(s)) ReadConfigLine(s);
      }
      fclose(f);
    }
  }
}

std::regex getRegex(cSv sv) {
  try {
    std::regex result;
    result.imbue(g_locale);
    result.assign(sv.data(), sv.length(), std::regex_constants::ECMAScript | std::regex_constants::optimize | std::regex_constants::collate);
    return result;
  } catch (const std::regex_error& e)
  {
    esyslog2(e.what(), " in regex ", sv);
    return std::regex();
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
  m_matchTitle = true;
  m_regexTitle = std::regex();
  esyslog2("In override.conf ", edb, " given, expected TheTVDB_Series or TheMovieDB_Series", !seriesRequired?" or TheMovieDB_Movie":"");
  return false;
}
void cRegexAction::set_title(cSv title) {
  m_matchTitle = !title.empty();
  if (m_matchTitle) m_regexTitle = getRegex(title);
}
void cRegexAction::set_channel(cSv channel) {
  m_matchChannel = !channel.empty();
  if (m_matchChannel) m_regexChannel = getRegex(channel);
}

bool assertFldsSize(const char *line, const vector<cSv> &flds, size_t requiredFields) {
  if (flds.size() == requiredFields) return true;
  esyslog("tvscraper, ERROR in override.conf, line %s, fields %zu, requiredFields %zu", line, flds.size(), requiredFields);
  return false;
}
void cOverRides::ReadConfigLine(const char *line) {
  vector<cSv> flds = getSetFromString<cSv, vector<cSv>>(line);
  if (flds.size() > 0) {
    if (!flds[0].compare("ignore")) {
      if (assertFldsSize(line, flds, 2)) {
        ignores.push_back(std::string(flds[1]));
      }
    } else if (!flds[0].compare("regexIgnore")) {
      if (assertFldsSize(line, flds, 2)) {
        regexIgnores.push_back(getRegex(flds[1]));
      }
    } else if (!flds[0].compare("settype")) {
      if (assertFldsSize(line, flds, 3)) {
        if (!flds[2].compare("series")) {
          searchTypes.insert(pair<string, scrapType>(flds[1], scrapSeries));
        } else if (!flds[2].compare("movie")) {
          searchTypes.insert(pair<string, scrapType>(flds[1], scrapMovie));
        }
      }
    } else if (!flds[0].compare("substitute")) {
      if (assertFldsSize(line, flds, 3)) {
        substitutes.insert(pair<string, string>(flds[1], flds[2]));
      }
    } else if (!flds[0].compare("ignorePath")) {
      if (assertFldsSize(line, flds, 2)) {
        if (flds[1].find("/", flds[1].length()-1) != std::string::npos)
          ignorePath.push_back(string(flds[1]));
        else
          ignorePath.push_back(string(flds[1]) + "/");
      }
    } else if (!flds[0].compare("removePrefix")) {
      if (assertFldsSize(line, flds, 2)) {
        removePrefixes.push_back(string(flds[1]));
      }
    } else if (!flds[0].compare("TheTVDB_SeriesID")) {
      if (assertFldsSize(line, flds, 3)) {
        m_TheTVDB_SeriesID.insert(pair<std::string, int>(std::string(flds[1]), parse_int<int>(flds[2])));
      }
    } else if (!flds[0].compare("TheMovieDB_SeriesID")) {
      if (assertFldsSize(line, flds, 3)) {
        m_TheMovieDB_SeriesID.insert(pair<std::string, int>(std::string(flds[1]), parse_int<int>(flds[2])));
      }
    } else if (!flds[0].compare("TheMovieDB_MovieID")) {
      if (assertFldsSize(line, flds, 3)) {
        m_TheMovieDB_MovieID.insert(pair<std::string, int>(std::string(flds[1]), parse_int<int>(flds[2])));
      }
    } else if (!flds[0].compare("regexTitleChannel->id")) {
      if (assertFldsSize(line, flds, 5)) {
        cRegexAction regexAction;
        regexAction.m_matchShortText = false;
        regexAction.set_title(flds[1]);
        regexAction.set_channel(flds[2]);
        regexAction.set_dbid(flds[3], flds[4], false);
        regexAction.m_matchPurpose = eMatchPurpose::regexTitleChannel_id;
        m_regex.push_back(regexAction);
      }
    } else if (!flds[0].compare("regexTitleChannel->idEpisodeName")) {
      if (assertFldsSize(line, flds, 5)) {
        cRegexAction regexAction;
        regexAction.m_matchShortText = false;
        regexAction.set_title(flds[1]);
        int num_cg = regexAction.m_matchTitle?regexAction.m_regexTitle.mark_count():0;
        if (num_cg != 1) {
          esyslog("tvscraper, ERROR in regex %s", cToSvConcat(flds[1], ", expected 1 capture group, found ", num_cg, " capture groups").c_str());
          regexAction.m_matchTitle = true;
          regexAction.m_regexTitle = std::regex();
        }
        regexAction.set_channel(flds[2]);
        regexAction.set_dbid(flds[3], flds[4], true);
        regexAction.m_matchPurpose = eMatchPurpose::regexTitleChannel_idEpisodeName;
        m_regex.push_back(regexAction);
      }
    } else if (!flds[0].compare("regexTitleShortTextChannel->idEpisodeName")) {
      if (assertFldsSize(line, flds, 6)) {
        cRegexAction regexAction;
        regexAction.set_title(flds[1]);
        regexAction.m_matchShortText = true;
        regexAction.m_regexShortText = getRegex(flds[2]);
        int num_title_cg = regexAction.m_matchTitle?regexAction.m_regexTitle.mark_count():0;
        if ((num_title_cg + regexAction.m_regexShortText.mark_count()) != 1) {
          esyslog("tvscraper, ERROR in regex %s", cToSvConcat(flds[1], ", st regex ", flds[2], ", expected 1 capture group, found ", num_title_cg, " capture groups in title and ", regexAction.m_regexShortText.mark_count(), " capture groups in short text").c_str());
          regexAction.m_matchTitle = true;
          regexAction.m_regexTitle = std::regex();
        }
        regexAction.set_channel(flds[3]);
        regexAction.set_dbid(flds[4], flds[5], true);
        regexAction.m_matchPurpose = eMatchPurpose::regexTitleChannel_idEpisodeName;
        m_regex.push_back(regexAction);
      }
    } else if (!flds[0].compare("regexTitleShortTextChannel->idSeasonNumberEpisodeNumber")) {
      if (assertFldsSize(line, flds, 6)) {
        cRegexAction regexAction;
        regexAction.set_title(flds[1]);
        regexAction.m_matchShortText = true;
        regexAction.m_regexShortText = getRegex(flds[2]);
        regexAction.set_channel(flds[3]);
        regexAction.set_dbid(flds[4], flds[5], true);
        regexAction.m_matchPurpose = eMatchPurpose::regexTitleShortTextChannel_idSeasonNumberEpisodeNumber;
        m_regex.push_back(regexAction);
      }
    } else if (!flds[0].compare("regexTitleShortTextChannel->seasonNumberEpisodeNumber")) {
      if (flds.size() != 3 && flds.size() != 4) assertFldsSize(line, flds, 4);
      else {
        cRegexAction regexAction;
        regexAction.set_title(flds[1]);
        regexAction.m_matchShortText = !flds[2].empty();
        if (regexAction.m_matchShortText) regexAction.m_regexShortText = getRegex(flds[2]);
        int num_title_cg = regexAction.m_matchTitle?regexAction.m_regexTitle.mark_count():0;
        int num_shortText_cg = regexAction.m_matchShortText?regexAction.m_regexShortText.mark_count():0;
        if (num_title_cg + num_shortText_cg != 2) {
          esyslog("tvscraper, ERROR in regex %s", cToSvConcat(flds[1], ", st regex ", flds[2], ", expected 2 capture group, found ", num_title_cg, " capture groups in title and ", num_shortText_cg, " capture groups in short text").c_str());
          regexAction.m_matchTitle = true;
          regexAction.m_regexTitle = std::regex();
        }
        if (flds.size() == 4) regexAction.set_channel(flds[3]);
        else regexAction.m_matchChannel = false;
        regexAction.m_matchPurpose = eMatchPurpose::regexTitleShortTextChannel_seasonNumberEpisodeNumber;
        regexAction.m_dbid = 0;
        regexAction.m_movie = false;
        m_regex.push_back(regexAction);
      }
    }
  }
}

bool cOverRides::Ignore(cSv title) {
  for (const std::string &str: ignores) {
    if (title == str) {
//    if (config.enableDebug) dsyslog2("ignoring \"", title, "\" because of override.conf");
      return true;
    }
  }
  for (const std::regex &reg: regexIgnores) {
    if (std::regex_match(title.data(), title.data()+title.length(), reg)) {
      dsyslog2("ignoring \"", title, "\" because of regexIgnore in override.conf");
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
    esyslog("tvscraper, ERROR in cRegexAction::matches, regex for season/episode, sub_match.size() %zu", sub_match.size());
    return false;
  }
  season = parse_int<int>(sub_match[1].str());
  episode = parse_int<int>(sub_match[2].str());
  return true;
}
bool cRegexAction::matches(cSv title, cSv shortText, cSv description, cSv channel, int &season, int &episode, std::string &episodeName) const {
  std::cmatch title_match, shortText_match;
  if (m_matchTitle && !std::regex_match(title.data(), title.data()+title.length(), title_match, m_regexTitle)) return false;
  if (m_matchChannel && !std::regex_match(channel.data(), channel.data()+channel.length(), m_regexChannel)) return false;
  switch (m_matchPurpose) {
    case eMatchPurpose::regexTitleChannel_id:
      return true;
    case eMatchPurpose::regexTitleChannel_idEpisodeName:
      if (shortText.empty() ) shortText = description;
      if (m_matchShortText && !std::regex_match(shortText.data(), shortText.data()+shortText.length(), shortText_match, m_regexShortText)) return false;
      if (title_match.size() == 2) {
        episodeName = title_match[1].str();
        return true;
      }
      if (shortText_match.size() == 2) {
        episodeName = shortText_match[1].str();
        return true;
      }
      esyslog("tvscraper, ERROR in cRegexAction::matches, matchPurpose::regexTitleChannel_idEpisodeName, title_match.size() %zu, shortText_match.size() %zu, title %.*s, shortText %.*s", title_match.size(), shortText_match.size(), (int)title.length(), title.data(), (int)shortText.length(), shortText.data() );
      return true;
    case eMatchPurpose::regexTitleShortTextChannel_idSeasonNumberEpisodeNumber:
      if (regexGetSeasonEpisode(shortText, m_regexShortText, season, episode)) return true;
      if (regexGetSeasonEpisode(description, m_regexShortText, season, episode)) return true;
      return false;
    case eMatchPurpose::regexTitleShortTextChannel_seasonNumberEpisodeNumber:
      if (shortText.empty() ) shortText = description;
      if (m_matchShortText && !std::regex_match(shortText.data(), shortText.data()+shortText.length(), shortText_match, m_regexShortText)) return false;
      if (m_matchTitle && title_match.size() == 3) {
        season = parse_int<int>(title_match[1].str());
        episode = parse_int<int>(title_match[2].str());
        return true;
      }
      if (m_matchShortText && shortText_match.size() == 3) {
        season = parse_int<int>(shortText_match[1].str());
        episode = parse_int<int>(shortText_match[2].str());
        return true;
      }
      esyslog("tvscraper, ERROR in cRegexAction::matches, matchPurpose::regexTitleShortTextChannel_seasonNumberEpisodeNumber, title_match.size() %zu, shortText_match.size() %zu, title %.*s, shortText %.*s", title_match.size(), shortText_match.size(), (int)title.length(), title.data(), (int)shortText.length(), shortText.data() );
      return false;

// TODO case eMatchPurpose::regexTitleChannel_idEpisodeAbsolutNumber
// # regexTitleChannel->idEpisodeAbsolutNumber;regexTitle;regexChannel or empty;TheMovieDB_Series or TheTVDB_Series;id  // first match of regexTitle will be used as episode absolut number
    default:
      esyslog("tvscraper, ERROR in cRegexAction::matches, matchPurpose %d unknown", (int)m_matchPurpose);
  }
  return false;
}
bool cOverRides::regex(cSv title, cSv shortText, cSv description, cSv channel, eMatchPurpose &matchPurpose, int &dbid, bool &movie, int &season, int &episode, std::string &episodeName) {
  for (const cRegexAction &r: m_regex) if (r.matches(title, shortText, description, channel, season, episode, episodeName)) {
    matchPurpose = r.m_matchPurpose;
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
//      if (config.enableDebug) dsyslog("tvscraper: ignoring path \"%.*s\" because of override.conf", (int)path.length(), path.data());
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

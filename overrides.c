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
  if (config.enableDebug)
    esyslog("tvscraper: title \"%.*s\", use TheTVDB_SeriesID %i override.conf", (int)title.length(), title.data(), (int)hit->second);
  return hit->second;
}

int cOverRides::TheMovieDB_SeriesID(cSv title) {
  auto hit = m_TheMovieDB_SeriesID.find(title);
  if (hit == m_TheMovieDB_SeriesID.end()) return 0;
  if (config.enableDebug)
    esyslog("tvscraper: title \"%.*s\", use TheMovieDB_SeriesID %i override.conf", (int)title.length(), title.data(), (int)hit->second);
  return hit->second;
}

int cOverRides::TheMovieDB_MovieID(cSv title) {
  auto hit = m_TheMovieDB_MovieID.find(title);
  if (hit == m_TheMovieDB_MovieID.end()) return 0;
  if (config.enableDebug)
    esyslog("tvscraper: title \"%.*s\", use TheMovieDB_MovieID %i override.conf", (int)title.length(), title.data(), (int)hit->second);
  return hit->second;
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

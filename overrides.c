#include <map>
#include "overrides.h"

using namespace std;

cOverRides::cOverRides(void) {
}

cOverRides::~cOverRides() {
}

void cOverRides::ReadConfig(string confDir) {
    stringstream sstrConfFile;
    sstrConfFile << confDir << "/override.conf";
    string confFile = sstrConfFile.str();
    dsyslog("tvscraper: reading overrides from %s", confFile.c_str());
    if (access(confFile.c_str(), F_OK) == 0) {
        FILE *f = fopen(confFile.c_str(), "r");
        if (f) {
            char *s;
            cReadLine ReadLine;
            while ((s = ReadLine.Read(f)) != NULL) {
                char *p = strchr(s, '#');
                if (p)
                    *p = 0;
                stripspace(s);
                if (!isempty(s)) {
                    ReadConfigLine(s);
                }
            }
            fclose(f);
        }
    }
}

void cOverRides::ReadConfigLine(string line) {
    splitstring s(line.c_str());
    vector<string> flds = s.split(';');
    if (flds.size() > 0) {
        if (!flds[0].compare("ignore")) {
            if (flds.size() == 2) {
                ignores.push_back(flds[1]);
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
        }
    }
}

bool cOverRides::Ignore(string title) {
    vector<string>::iterator pos;
    for ( pos = ignores.begin(); pos != ignores.end(); ++pos) {
        if (!title.compare(pos->c_str())) {
            if (config.enableDebug)
                esyslog("tvscraper: ignoring \"%s\" because of override.conf", title.c_str());
            return true;
        }
    }
    return false;
}

string cOverRides::Substitute(string title) {
    map<string,string>::iterator hit = substitutes.find(title);
    if (hit != substitutes.end()) {
        if (config.enableDebug)
            esyslog("tvscraper: substitute \"%s\" with \"%s\" because of override.conf", title.c_str(), ((string)hit->second).c_str());
        return (string)hit->second;
    }
    return title;
}

scrapType cOverRides::Type(string title) {
    map<string,scrapType>::iterator hit = searchTypes.find(title);
    if (hit != searchTypes.end()) {
        if (config.enableDebug)
            esyslog("tvscraper: using type %d for \"%s\" because of override.conf", (int)hit->second, title.c_str());
        return (scrapType)hit->second;
    }
    return scrapNone;
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
}

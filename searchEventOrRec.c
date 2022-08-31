#include "searchEventOrRec.h"

cSearchEventOrRec::cSearchEventOrRec(csEventOrRecording *sEventOrRecording, cOverRides *overrides, cMovieDBScraper *moviedbScraper, cTVDBScraper *tvdbScraper, cTVScraperDB *db):
  m_sEventOrRecording(sEventOrRecording),
  m_overrides(overrides),
  m_moviedbScraper(moviedbScraper),
  m_tvdbScraper(tvdbScraper),
  m_db(db),
  m_TVtv(db, tvdbScraper),
  m_tv(db, moviedbScraper),
  m_movie(db, moviedbScraper),
  m_searchResult_Movie(0, true, "")
  {
    if (m_sEventOrRecording->Recording() && !m_sEventOrRecording->Recording()->Info() ) {
      esyslog("tvscraper: ERROR in cSearchEventOrRec: recording->Info() == NULL, Name = %s", m_sEventOrRecording->Recording()->Name() );
      return;
    }
    initBaseNameOrTitile();
    initSearchString();
    m_sEventOrRecording->AddYears(m_years);
    ::AddYears(m_years, m_baseNameOrTitle.c_str() );
    ::AddYears(m_years, m_searchString.c_str() );
  }
void cSearchEventOrRec::initBaseNameOrTitile(void) {
// initialize m_baseNameOrTitle
  const cRecording *recording = m_sEventOrRecording->Recording();
  if (recording) {
// Note: recording->Name():     This is the complete file path, /video prefix removed, and characters exchanged (like #3A -> :)
//                              In other words: You can display it on the UI
// Note: recording->BaseName(): Last part of recording->Name()  (after last '~')
//  bool debug = false;
//  debug = recording->Info() && recording->Info()->Title() && strcmp(recording->Info()->Title(), "Star Trek: Picard") == 0;
    cString baseName = recording->BaseName();
    if ((const char *)baseName == NULL) esyslog("tvscraper: ERROR in cSearchEventOrRec::initBaseNameOrTitile: baseName == NULL");
    size_t baseNameLen = strlen(baseName);
    if (baseNameLen == 0) esyslog("tvscraper: ERROR in cSearchEventOrRec::initBaseNameOrTitile: baseNameLen == 0");
    if (baseName[0] == '%') m_baseNameOrTitle = ( (const char *)baseName ) + 1;
                       else m_baseNameOrTitle = ( (const char *)baseName );
    if (!recording->Info()->Title() ) return; // no title, go ahead with basename, best what we have (very old recording?)
    if (isVdrDate(m_baseNameOrTitle) || isVdrDate2(m_baseNameOrTitle) ) {
// this normally happens if VDR records a series, and the subtitle is empty)
      m_baseNameOrTitle = recording->Info()->Title();
      if (recording->Info()->ShortText() ) return; // strange, but go ahead with title
      const char * title_pos = strstr(recording->Name(), recording->Info()->Title() );
      if (!title_pos || (size_t(title_pos - recording->Name() ) >= strlen(recording->Name()) - baseNameLen)  ) return; // title not part of the name, or title only part of the base name-> does not match the pattern -> return
      if (recording->Info()->Description() ) {
        m_episodeName = m_sEventOrRecording->EpisodeSearchString();
        m_baseNameEquShortText = true;
      }
      return;
    }
// if the title is somewhere in the recording name (except the basename part), 
// and the basename is part of the ShortText() (or Description()), we assume that this is a series
// like Merlin~1/13. The Dragon's Call
    const char *shortText = recording->Info()->ShortText();
    if (!shortText || ! *shortText) shortText = recording->Info()->Description();
    if (!shortText || ! *shortText) return; // no short text, no description -> go ahead with base name
    if ( sentence_distance(recording->Info()->Title(), m_baseNameOrTitle) <= sentence_distance(shortText, m_baseNameOrTitle) ) return; // in this case, m_baseNameOrTitle is the title, so go ahead with m_baseNameOrTitle
// name of recording is short text -> use name of recording as episode name, and title as name of TV show
    m_episodeName = m_baseNameOrTitle;
    m_baseNameOrTitle = recording->Info()->Title();
    m_baseNameEquShortText = true;
  } else m_baseNameOrTitle = m_sEventOrRecording->Title();
}

bool cSearchEventOrRec::isVdrDate(const std::string &baseName) {
// return true if string matches the 2020.01.12-02:35 pattern.
  int start;
  for (start = 0; !isdigit(baseName[start]) && start < (int)baseName.length(); start++);
  if ((int)baseName.length() < 16 + start) return false;

  int n;
  for (n = start; n < 4 + start; n ++) if (!isdigit(baseName[n]) ) return false;
  if (baseName[n] != '.') return false;
  for (n++ ; n <  7 + start; n ++) if (!isdigit(baseName[n]) ) return false;
  if (baseName[n] != '.') return false;
  for (n++ ; n < 10 + start; n ++) if (!isdigit(baseName[n]) ) return false;
  if (baseName[n] != '-') return false;
  for (n++ ; n < 13 + start; n ++) if (!isdigit(baseName[n]) ) return false;
  if (baseName[n] != ':') return false;
  for (n++ ; n < 16 + start; n ++) if (!isdigit(baseName[n]) ) return false;

  return true;
}

bool cSearchEventOrRec::isVdrDate2(const std::string &baseName) {
// return true if string matches the "Mit 01.12.2009-02:35" pattern.
  int start;
  for (start = 0; !isdigit(baseName[start]) && start < (int)baseName.length(); start++);
  if ((int)baseName.length() < 16 + start) return false;

  int n;
  for (n = start; n <  2 + start; n ++) if (!isdigit(baseName[n]) ) return false;
  if (baseName[n] != '.') return false;
  for (n++ ; n <  5 + start; n ++) if (!isdigit(baseName[n]) ) return false;
  if (baseName[n] != '.') return false;
  for (n++ ; n < 10 + start; n ++) if (!isdigit(baseName[n]) ) return false;
  if (baseName[n] != '-') return false;
  for (n++ ; n < 13 + start; n ++) if (!isdigit(baseName[n]) ) return false;
  if (baseName[n] != ':') return false;
  for (n++ ; n < 16 + start; n ++) if (!isdigit(baseName[n]) ) return false;

  return true;
}

void cSearchEventOrRec::initSearchString(void) {
  m_searchString = m_baseNameOrTitle;
  m_searchStringSubstituted = m_overrides->Substitute(m_searchString);
  if (!m_searchStringSubstituted) {
// some more, but only if no explicit substitution
    m_searchString = m_overrides->RemovePrefix(m_searchString);
    StringRemoveLastPartWithP(m_searchString); // always remove this: Year, number of season/episode, ... but not a part of the name
  }
  transform(m_searchString.begin(), m_searchString.end(), m_searchString.begin(), ::tolower);
}

cMovieOrTv *cSearchEventOrRec::Scrape(void) {
// extDbConnected: true, if request to rate limited internet db was required. Otherwise, false
  if (config.enableDebug) {
    char buff[20];
    time_t event_time = m_sEventOrRecording->StartTime();
    strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&event_time));
    esyslog("tvscraper: Scrap event: search string \"%s\", title \"%s\", start time: %s", m_searchString.c_str(), m_sEventOrRecording->Title(), buff);
  }
  if (m_overrides->Ignore(m_baseNameOrTitle)) return NULL;
  sMovieOrTv movieOrTv;
  ScrapFindAndStore(movieOrTv);
  if(movieOrTv.type != scrapMovie && movieOrTv.type != scrapSeries) {
// nothing found, try again with title
    m_baseNameOrTitle = m_sEventOrRecording->Title();
    m_baseNameEquShortText = false;
    m_episodeName = "";
    initSearchString();
    ScrapFindAndStore(movieOrTv);
  }
  ScrapAssign(movieOrTv);
  return cMovieOrTv::getMovieOrTv(m_db, movieOrTv);
}

void cSearchEventOrRec::ScrapFindAndStore(sMovieOrTv &movieOrTv) {
  if (CheckCache(movieOrTv) ) return;
  if (config.enableDebug) esyslog("tvscraper: scraping \"%s\"", m_searchString.c_str());
  vector<searchResultTvMovie> searchResults;
  string episodeSearchString;
  string movieName;
  scrapType sType = ScrapFind(searchResults, movieName, episodeSearchString);
  if (searchResults.size() == 0) sType = scrapNone;
  movieOrTv.type = sType;
  if (sType == scrapNone) {
// nothing found
    movieOrTv.id = 0;
    if (config.enableDebug) esyslog("tvscraper: nothing found for \"%s\" ", movieName.c_str());
  } else {
// something found
    movieOrTv.id = searchResults[0].id();
    movieOrTv.year = searchResults[0].m_yearMatch?searchResults[0].year()*searchResults[0].m_yearMatch:0;
    Store(movieOrTv);
    if(sType == scrapMovie) {
      movieOrTv.season = -100;
      movieOrTv.episode = 0;
      if (config.enableDebug) esyslog("tvscraper: movie \"%s\" successfully scraped, id %i", movieName.c_str(), searchResults[0].id() );
    } else if(sType == scrapSeries) {
// search episode
      movieOrTv.episodeSearchWithShorttext = episodeSearchString.empty()?1:0;
      if (movieOrTv.episodeSearchWithShorttext == 1) {
        episodeSearchString = m_baseNameEquShortText?m_episodeName:m_sEventOrRecording->EpisodeSearchString();
        if (searchResults[0].getMatchEpisode() ) {
          movieOrTv.episodeSearchWithShorttext = 3;
          float bestMatchText = searchResults[0].getMatchText();
          for (const searchResultTvMovie &searchResult: searchResults) {
            if (searchResult.id() == searchResults[0].id() ) continue;
            if ((searchResult.id()^searchResults[0].id()) < 0) continue; // we look for IDs in same database-> same sign
            if (searchResult.movie()) continue;  // we only look for TV shows
            if (abs(bestMatchText - searchResult.getMatchText()) > 0.001) continue;   // we only look for matches similar near
            m_db->setSimilar(searchResults[0].id(), searchResult.id() );
            if (config.enableDebug) esyslog("tvscraper: setSimilar %i and %i", searchResults[0].id(), searchResult.id());
          }
        }
      }
      UpdateEpisodeListIfRequired(movieOrTv.id);
      cMovieOrTv::searchEpisode(m_db, movieOrTv, episodeSearchString, m_baseNameOrTitle);
      if (config.enableDebug) esyslog("tvscraper: %stv \"%s\", episode \"%s\" successfully scraped, id %i season %i episode %i", searchResults[0].id() > 0?"":"TV", movieName.c_str(), episodeSearchString.c_str(), movieOrTv.id, movieOrTv.season, movieOrTv.episode);
    }
    if (config.enableDebug) {
      searchResults[0].log(m_searchString.c_str() );
      if (searchResults.size() > 1 ) searchResults[1].log(m_searchString.c_str() );
    }
  }
  m_db->InsertCache(m_searchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText);
  return;
}

void cSearchEventOrRec::Store(const sMovieOrTv &movieOrTv) {
  switch (movieOrTv.type) {
    case scrapMovie:
      m_movie.SetID(movieOrTv.id);
      m_moviedbScraper->StoreMovie(m_movie);
      return;
    case scrapSeries:
      if (movieOrTv.id > 0) {
        m_tv.SetTvID(movieOrTv.id);
        m_tv.UpdateDb();
      } else {
        m_tvdbScraper->StoreSeries(movieOrTv.id * (-1), false);
      }
      return;
    case scrapNone: ; // do nothing, nothing to store
  }
}

// required for older vdr versions, to make swap used by sort unambigous
inline void swap(searchResultTvMovie &a, searchResultTvMovie &b) { std::swap(a, b); }

scrapType cSearchEventOrRec::ScrapFind(vector<searchResultTvMovie> &searchResults, string &movieName, string &episodeSearchString) {
//  bool debug = m_searchString == "james cameron's dark angel";
  bool debug = false;
  movieName = m_searchString;
  episodeSearchString = "";
  scrapType type_override = m_overrides->Type(m_baseNameOrTitle);
// check for movie: save best movie in m_searchResult_Movie
  if (type_override != scrapSeries) SearchMovie(searchResults);
// check for series
  if (type_override != scrapMovie) SearchTvAll(searchResults);
  if (searchResults.size() == 0) return scrapNone; // nothing found
//    if (!titleSep && m_sEventOrRecording->DurationInSec() > 80*60)
// something was found. Add all information which is available for free
  if (m_baseNameEquShortText) for (searchResultTvMovie &searchResult: searchResults) if (!searchResult.movie() && searchResult.delim() == 0) searchResult.setBaseNameEquShortText();
  for (searchResultTvMovie &searchResult: searchResults) searchResult.setMatchYear(m_years, m_sEventOrRecording->DurationInSec() );
  std::sort(searchResults.begin(), searchResults.end() );
  if (debug) for (searchResultTvMovie &searchResult: searchResults) searchResult.log(m_searchString.c_str() );
  m_episodeFound = false;
  std::vector<searchResultTvMovie>::iterator new_end;
// in case of results which are "similar" good, add more information to find the best one
  if (selectBestAndEnhanvceIfRequired(searchResults.begin(), searchResults.end(), new_end, 0.2,  &enhance1) ) {
//      esyslog("tvscraper: ScrapFind (5), about to call enhance2 search string = %s", m_searchString.c_str()  );
// if we still have results which are "similar" good, add even more (and more expensive) information
      selectBestAndEnhanvceIfRequired(searchResults.begin(), new_end            , new_end, 0.15, &enhance2);
  }
// no more inforamtion can be added. Best result is in searchResults[0]
  if (debug) esyslog("tvscraper: ScrapFind, found: %i, title: \"%s\"", searchResults[0].id(), m_searchString.c_str() );
  if (searchResults[0].movie() ) return scrapMovie;
  if (searchResults[0].delim() ) splitString(m_searchString, searchResults[0].delim(), 4, movieName, episodeSearchString);
  return scrapSeries;
}

int cSearchEventOrRec::GetTvDurationDistance(int tvID) {
  int finalDurationDistance = -1; // default, no data available
  for (vector<string> &duration_v: tvID>0?m_moviedbScraper->GetTvRuntimes(tvID):m_tvdbScraper->GetTvRuntimes(tvID * -1) ) if ( duration_v.size() > 0) {
    int durationDistance = m_sEventOrRecording->DurationDistance(atoi(duration_v[0].c_str() ) );
    if (finalDurationDistance == -1 || durationDistance < finalDurationDistance) finalDurationDistance = durationDistance;
  }
  return finalDurationDistance;
}

void cSearchEventOrRec::SearchTvAll(vector<searchResultTvMovie> &searchResults) {
// search for TV shows, in all formats (episode in short text, or in title)
  SearchTv(searchResults, m_searchString);
  SearchTvEpisTitle(searchResults, ':');
  SearchTvEpisTitle(searchResults, '-');
}

void cSearchEventOrRec::SearchTv(vector<searchResultTvMovie> &resultSet, const string &searchString) {
  extDbConnected = true;
  string searchString1 = SecondPart(searchString, ":");
  string searchString2 = SecondPart(searchString, "'s");
  size_t oldSize = resultSet.size();
  m_TVtv.AddResults(resultSet, searchString, searchString);
  if (searchString1.length() > 4 ) m_TVtv.AddResults(resultSet, searchString, searchString1);
  if (searchString2.length() > 4 ) m_TVtv.AddResults(resultSet, searchString, searchString2);
  if (resultSet.size() == oldSize) {
    m_tv.AddTvResults(resultSet, searchString, searchString);
    if (searchString1.length() > 4 ) m_tv.AddTvResults(resultSet, searchString, searchString1);
    if (searchString2.length() > 4 ) m_tv.AddTvResults(resultSet, searchString, searchString2);
  }
}

void cSearchEventOrRec::SearchTvEpisTitle(vector<searchResultTvMovie> &resultSet, char delimiter) {
  string movieName;
  string episodeSearchString;
  if (!splitString(m_searchString, delimiter, 4, movieName, episodeSearchString) ) return;
//  bool debug = movieName == "rose" && delimiter == '-';
  bool debug = false;
  size_t oldSize = resultSet.size();
  if (debug) esyslog("tvscraper: cSearchEventOrRec::SearchTvEpisTitle (1), oldSize %lu", oldSize);
  SearchTv(resultSet, movieName);
  if (debug) esyslog("tvscraper: cSearchEventOrRec::SearchTvEpisTitle (2), oldSize %lu, size %lu", oldSize, resultSet.size() );
  if (resultSet.size() > oldSize)
    for (vector<searchResultTvMovie>::iterator i = resultSet.begin() + oldSize; i != resultSet.end(); i++) {
//      if (debug) esyslog("tvscraper: cSearchEventOrRec::SearchTvEpisTitle set delim for %i", i->id() );
      i->setDelim(delimiter);
    }
  if (debug) esyslog("tvscraper: cSearchEventOrRec::SearchTvEpisTitle (3), oldSize %lu, size %lu", oldSize, resultSet.size() );
}

void cSearchEventOrRec::SearchMovie(vector<searchResultTvMovie> &resultSet) {
  extDbConnected = true;

  string searchString1 = SecondPart(m_searchString, ":");
  string searchString2 = SecondPart(m_searchString, "'s");
  string searchString3, searchString4;
  m_movie.AddMovieResults(resultSet, m_searchString, m_searchString, m_years);
  if (searchString1.length() > 4 ) m_movie.AddMovieResults(resultSet, m_searchString, searchString1, m_years);
  if (searchString2.length() > 4 ) m_movie.AddMovieResults(resultSet, m_searchString, searchString2, m_years);
  if (splitString(m_searchString, '-', 4, searchString3, searchString4) ) {
    if (searchString3.length() > 4 ) m_movie.AddMovieResults(resultSet, m_searchString, searchString3, m_years);
    if (searchString4.length() > 4 ) m_movie.AddMovieResults(resultSet, m_searchString, searchString4, m_years);
  }
}

bool cSearchEventOrRec::CheckCache(sMovieOrTv &movieOrTv) {
  if (!m_db->GetFromCache(m_searchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText) ) return false;
// cache found
  string episodeSearchString;
  if (movieOrTv.type != scrapNone && movieOrTv.id != 0) {
// something "real" was found
    Store(movieOrTv);
// do we have to search the episode?
    if (movieOrTv.season != -100 && movieOrTv.episodeSearchWithShorttext) {
// a TV show was found, and we have to use the subtitle to find the episode
      episodeSearchString = m_baseNameEquShortText?m_episodeName:m_sEventOrRecording->EpisodeSearchString();
      int min_distance = 2000;
      sMovieOrTv movieOrTv_best;
      for (const int &id: m_db->getSimilarTvShows(movieOrTv.id) ) {
        sMovieOrTv movieOrTv2 = movieOrTv;
        movieOrTv2.id = id;
        if (id != movieOrTv.id) Store(movieOrTv2);
        UpdateEpisodeListIfRequired(id);
        int distance = cMovieOrTv::searchEpisode(m_db, movieOrTv2, episodeSearchString, m_baseNameOrTitle);
        if (distance < min_distance) {
          min_distance = distance;
          movieOrTv_best = movieOrTv2;
        }
      }
      if (movieOrTv.episodeSearchWithShorttext == 3 && min_distance > 600) return false;
// episode match required for cache, but not found (ignore coincidence matches with distance > 600)
      if (min_distance < 900) movieOrTv = movieOrTv_best; // otherwise, there was no significant episode match
    }
  }

  if (!config.enableDebug) return true;
  switch (movieOrTv.type) {
    case scrapNone:
      esyslog("tvscraper: found cache %s => scrapNone", m_searchString.c_str());
      return true;
    case scrapMovie:
      esyslog("tvscraper: found movie cache %s => %i", m_searchString.c_str(), movieOrTv.id);
      return true;
    case scrapSeries:
      esyslog("tvscraper: found %stv cache tv \"%s\", episode \"%s\" , id %i season %i episode %i", movieOrTv.id < 0?"TV":"", m_searchString.c_str(), episodeSearchString.c_str(), movieOrTv.id, movieOrTv.season, movieOrTv.episode);
      return true;
    default:
      esyslog("tvscraper: ERROR found cache %s => unknown scrap type %i", m_searchString.c_str(), movieOrTv.type);
      return true;
  }
}

void cSearchEventOrRec::ScrapAssign(const sMovieOrTv &movieOrTv) {
// assign found movieOrTv to event/recording in db
// and download images
  if(movieOrTv.type != scrapMovie && movieOrTv.type != scrapSeries) return; // if nothing was found, there is nothing todo
  if (!m_sEventOrRecording->Recording() )
          m_db->InsertEvent     (m_sEventOrRecording, movieOrTv.id, movieOrTv.season, movieOrTv.episode);
     else m_db->InsertRecording2(m_sEventOrRecording, movieOrTv.id, movieOrTv.season, movieOrTv.episode);
}

void cSearchEventOrRec::UpdateEpisodeListIfRequired(int tvID) {
// check: is update required?
  string status;
  time_t lastUpdated = 0;
  time_t now = time(0);

  m_db->GetTv(tvID, lastUpdated, status);
  if (config.enableDebug && tvID == -232731) esyslog("tvscraper: cSearchEventOrRec::UpdateEpisodeListIfRequired, 1, tvID %i, now %li, lastUpdated %li, difftime %f, status %s", tvID, now, lastUpdated, difftime(now, lastUpdated), status.c_str() );

  if (lastUpdated) {
    if (difftime(now, lastUpdated) < 7.*24*60*60 ) return; // min one week between updates
    if (status.compare("Ended") == 0) return; // see https://thetvdb-api.readthedocs.io/api/series.html
    if (status.compare("Canceled") == 0) return;
// not documented for themoviedb, see https://developers.themoviedb.org/3/tv/get-tv-details . But test indicates the same values ("Ended" & "Canceled")...
  }
// update is required
  if (tvID > 0) {
    cMovieDbTv tv(m_db, m_moviedbScraper);
    tv.SetTvID(tvID);
    tv.UpdateDb();
  } else {
    m_tvdbScraper->StoreSeries(tvID * (-1), true);
  }

}

void cSearchEventOrRec::getActorMatches(const std::string &actor, int &numMatchesAll, int &numMatchesFirst, int &numMatchesSure, vector<string> &alreadyFound) {
// numMatchesAll:   complete actor match, and actor must contain a blank
// numMatchesSure:  match of last part of actor (blank delimited)
// numMatchesFirst: match of other parts of actor (blank delimited)
// note: too short parts of actor are ignored, can be articles, ...
  const char *description = m_sEventOrRecording->Description();
  if (!description) return;
  size_t pos_blank = actor.rfind(' ');
  if (pos_blank != std::string::npos && addActor(description, actor, numMatchesAll,  alreadyFound)) return;
  if (pos_blank == std::string::npos && addActor(description, actor, numMatchesSure, alreadyFound)) return;
  if (pos_blank == std::string::npos || pos_blank == 0) return;

// look for matches of part of the actor
  const char *lPos = actor.c_str();
  for (const char *rDelimPos; (rDelimPos = strchr(lPos, ' ')); lPos = rDelimPos + 1)
    addActor(description, lPos, rDelimPos - lPos, numMatchesFirst, alreadyFound);
  addActor(description, lPos, 0, numMatchesSure, alreadyFound);
}

void cSearchEventOrRec::getDirectorWriterMatches(const std::string &directorWriter, int &numMatchesAll, int &numMatchesSure, vector<string> &alreadyFound) {
  if (directorWriter.length() < 3) return;
  const char *description = m_sEventOrRecording->Description();
  if (!description) return;
  if (addActor(description, directorWriter, numMatchesAll, alreadyFound)) return;
// complete name not found. Try last name
  size_t pos_blank = directorWriter.rfind(' ');
  if (pos_blank == std::string::npos || pos_blank == 0) return;
  addActor(description, directorWriter.c_str() + pos_blank + 1, 0, numMatchesSure, alreadyFound);
}

bool cSearchEventOrRec::addActor(const char *description, const char *name, size_t len, int &numMatches, vector<string> &alreadyFound) {
// search name in description, if found, increase numMatches. But only once for each name found
// return true if found. Always. Even if numMatches is not increased
  size_t len2 = (len == 0)?strlen(name):len;
  if (len2 < 3) return false; // ignore if name is too short
  if (strstr_word(description, name, len) == NULL) return false;   // name not found in description
  string name_s(name, len2);
  if (find (alreadyFound.begin(), alreadyFound.end(), name_s) != alreadyFound.end() ) return true;
  alreadyFound.push_back(name_s);
  numMatches++;
  return true;
}
bool cSearchEventOrRec::addActor(const char *description, const string &name, int &numMatches, vector<string> &alreadyFound) {
// search name in description, if found, increase numMatches. But only once for each name found
// return true if found. Always. Even if numMatches is not increased
  if (name.length() < 3) return false; // ignore if name is too short
  if (strstr_word(description, name.c_str() ) == NULL) return false;   // name not found in description
  if (find (alreadyFound.begin(), alreadyFound.end(), name) != alreadyFound.end() ) return true;
  alreadyFound.push_back(name);
  numMatches++;
  return true;
}

void cSearchEventOrRec::getActorMatches(searchResultTvMovie &sR, const std::vector<std::vector<std::string>> &actors) {
  int numMatchesAll = 0;
  int numMatchesFirst = 0;
  int numMatchesSureRole = 0;
  int numMatchesSureName = 0;
  vector<string> alreadyFound;
  bool debug = false;
//  debug = (sR.id() == 557) || (sR.id() == 558) || (sR.id() == 559);
//  debug = debug || (sR.id() == 5548) || (sR.id() == 5549) || (sR.id() == 5550); // RoboCop / Officer Alex J. Murphy or just RoboCop ...)
//  debug = debug || (sR.id() == -77973) || (sR.id() == 60293);
//  debug = false;
  if (debug) esyslog("tvscraper: getActorMatches, id: %i", sR.id() );
  for(const std::vector<std::string> &actor : actors) if (actor.size() == 3) {
// actor[1]: actor_name
    getActorMatches(actor[1], numMatchesAll, numMatchesFirst, numMatchesSureName, alreadyFound);
    if (debug) esyslog("tvscraper: getActorMatches, Actor: %s, numMatchesAll %i, numMatchesFirst %i, numMatchesSureRole %i, numMatchesSureName %i", actor[1].c_str(), numMatchesAll, numMatchesFirst, numMatchesSureRole, numMatchesSureName);
// actor[2]: actor_role
    getActorMatches(actor[2], numMatchesAll, numMatchesFirst, numMatchesSureRole, alreadyFound);
    if (debug) esyslog("tvscraper: getActorMatches, Actor: %s, numMatchesAll %i, numMatchesFirst %i, numMatchesSureRole %i, numMatchesSureName %i", actor[2].c_str(), numMatchesAll, numMatchesFirst, numMatchesSureRole, numMatchesSureName);
  }
  int sum = numMatchesFirst + numMatchesSureRole + 4*numMatchesSureName + 16*numMatchesAll;
  sR.setActors(sum);
  if (debug) esyslog("tvscraper: getActorMatches, sum %i, normMatch %f", sum, searchResultTvMovie::normMatch(sum/16.));
}

void cSearchEventOrRec::getDirectorWriterMatches(searchResultTvMovie &sR, const std::vector<std::string> &directorWriters) {
  int numMatchesAll = 0;
  int numMatchesSure = 0;
  vector<string> alreadyFound;
  for(const std::string &directorWriter : directorWriters) {
    getDirectorWriterMatches(directorWriter, numMatchesAll, numMatchesSure, alreadyFound);
  }
  sR.setDirectorWriter(numMatchesSure + 2*numMatchesAll);
}

bool cSearchEventOrRec::selectBestAndEnhanvceIfRequired(std::vector<searchResultTvMovie>::iterator begin, std::vector<searchResultTvMovie>::iterator end, std::vector<searchResultTvMovie>::iterator &new_end, float minDiff, void (*func)(searchResultTvMovie &sR, cSearchEventOrRec &searchEventOrRec)) {
// return true if enhancement was required
// in this case, return the end of the enhance list in new_end
// minDiff must be > 0, otherwise an empty list my be returned
//  bool debug = m_searchString == "james cameron's dark angel";
  bool debug = false;
  float minDiffSame = std::max(minDiff - 0.05, 0.01);
  float minDiffOther = minDiff;
  new_end = end;
  if (begin == end) return false; // empty list
  if (begin + 1 == end) return false; // list with one element
  std::sort(begin, end);
  float bestMatch = begin->getMatch();
  bool bestMatchMovie = begin->movie();
  std::vector<searchResultTvMovie>::iterator i;
  float diffSame = 2.;
  float diffALt  = 2.;
  for (i = begin + 1; i != end; i++) if (i->movie() != bestMatchMovie) { diffALt  = bestMatch - i->getMatch(); break; }
  for (i = begin + 1; i != end; i++) if (i->movie() == bestMatchMovie) { diffSame = bestMatch - i->getMatch(); break; }
  if (debug) esyslog("tvscraper: selectBestAndEnhanvceIfRequired, minDiffSame %f, minDiffOther %f, diffALt %f, diffSame %f", minDiffSame, minDiffOther, diffALt, diffSame);
  if (diffSame > minDiffSame && diffALt > minDiffOther) return false;
  bool matchAlt = !(diffALt <= diffALt); // set this to false, if still a match to an "alternative" finding is required
  for (i = begin; i != end; i++) {
//    debug = debug || i->id() == -353808 || i->id() == -250822 || i->id() == 440757;
    float match = i->getMatch();
    if (debug) esyslog("tvscraper: selectBestAndEnhanvceIfRequired(1), id = %i match = %f", i->id(), match );
    if (match < 0.3) break; // most likely not the match you are looking for, so no more work on this ...
    if (matchAlt && (bestMatch - match) > minDiffSame) break;   // best match is minDiffSame better compared to this
    if (debug) esyslog("tvscraper: selectBestAndEnhanvceIfRequired(2), id = %i", i->id() );
    if (i->movie() != bestMatchMovie) matchAlt = true;
    func(*i, *this);
    if (debug) esyslog("tvscraper: selectBestAndEnhanvceIfRequired(3), id = %i", i->id() );
  }
  if (debug) esyslog("tvscraper: selectBestAndEnhanvceIfRequired(4), searchstring = %s", m_searchString.c_str() );
  std::sort(begin, i);
  if (debug) esyslog("tvscraper: selectBestAndEnhanvceIfRequired(5)" );
  new_end = i;
  return true;
}

void cSearchEventOrRec::enhance1(searchResultTvMovie &sR, cSearchEventOrRec &searchEventOrRec) {
// add all information which is in database cache
// this is all except the episode list
/*
  bool debug = sR.id() == -353808 || sR.id() == -250822 || sR.id() == 440757 || sR.id() == -258828 || sR.id() == -412485 || sR.id() == -295683;
  debug = debug || searchEventOrRec.m_searchString == "die göttliche sophie";
  debug = debug || searchEventOrRec.m_searchString == "rose - königin der blumen";
  debug = debug || searchEventOrRec.m_searchString == "sendung vom 19.02. 09:30 uhr";
  debug = debug || searchEventOrRec.m_searchString == "sendung vom 19.02. 09";
  debug = debug || searchEventOrRec.m_searchString == "exploding sun 1";
  debug = debug || searchEventOrRec.m_searchString == "exploding sun 2";
  debug = debug || searchEventOrRec.m_searchString == "stargate";
*/
  bool debug = false;
  if (debug) sR.log(searchEventOrRec.m_searchString.c_str() );
  std::vector<std::vector<std::string>> actors;
  if (sR.movie() ) {
// movie
    sR.setDuration(searchEventOrRec.m_sEventOrRecording->DurationDistance(searchEventOrRec.m_moviedbScraper->GetMovieRuntime(sR.id() )) );
    sR.updateMatchText(sentence_distance(searchEventOrRec.m_db->GetMovieTagline(sR.id() ), searchEventOrRec.m_searchString));
    actors = searchEventOrRec.m_db->GetActorsMovie(sR.id() );
    std::string director;
    std::string writer;
    const char sql[] = "select movie_director, movie_writer from movie_runtime2 where movie_id = ?";
    if (searchEventOrRec.m_db->QueryLine(sql, "i", "SS", sR.id(), &director, &writer)) {
      std::vector<std::string> directors;
      stringToVector(directors, director.c_str() );
      stringToVector(directors, writer.c_str() );
      searchEventOrRec.getDirectorWriterMatches(sR, directors);
    }
  } else {
// tv show
    if (debug) esyslog("tvscraper: enhance1 (1)" );
    sR.setDuration(searchEventOrRec.GetTvDurationDistance(sR.id() ) );
    if (debug) esyslog("tvscraper: enhance1 (2)" );
    sR.setDirectorWriter(0);
    if (sR.id() < 0) {
// tv show from thetvdb
      float voteAverage;
      int voteCount;
      searchEventOrRec.m_tvdbScraper->GetTvVote(sR.id() * -1, voteAverage, voteCount);
    if (debug) esyslog("tvscraper: enhance1 (3), voteAverage = %f, voteCount = %i", voteAverage, voteCount);
      sR.setPopularity(voteAverage, voteCount);
    if (debug) esyslog("tvscraper: enhance1 (4)" );
  if (debug) sR.log(searchEventOrRec.m_searchString.c_str() );
      actors = searchEventOrRec.m_db->GetActorsSeries(sR.id() * (-1));
    if (debug) esyslog("tvscraper: enhance1 (5)" );
  if (debug) sR.log(searchEventOrRec.m_searchString.c_str() );
    } else {
// tv show from themoviedb
      actors = searchEventOrRec.m_db->GetActorsTv(sR.id() );
    }
  }
    if (debug) esyslog("tvscraper: enhance1 (6)" );
  if (debug) sR.log(searchEventOrRec.m_searchString.c_str() );
  searchEventOrRec.getActorMatches(sR, actors);
  if (debug) sR.log(searchEventOrRec.m_searchString.c_str() );
}
void cSearchEventOrRec::enhance2(searchResultTvMovie &searchResult, cSearchEventOrRec &searchEventOrRec) {
  bool debug = searchResult.id() == -353808 || searchResult.id() == -250822 || searchResult.id() == 440757;
  debug = false;
  if (debug) esyslog("tvscraper: enhance2 (1)" );

  if (searchResult.movie() ) { searchResult.setMatchEpisode(1000); return; }
//  if (searchEventOrRec.m_episodeFound || searchResult.movie() ) { searchResult.setMatchEpisode(0); return; }
  string movieName;
  string episodeSearchString;
  sMovieOrTv movieOrTv;
  movieOrTv.id = searchResult.id();
  movieOrTv.type = scrapSeries;
  searchEventOrRec.Store(movieOrTv);
  if (debug) esyslog("tvscraper: enhance2 (3)" );
// search episode
  movieOrTv.episodeSearchWithShorttext = searchResult.delim()?0:1;
  if (movieOrTv.episodeSearchWithShorttext) episodeSearchString = searchEventOrRec.m_baseNameEquShortText?searchEventOrRec.m_episodeName:searchEventOrRec.m_sEventOrRecording->EpisodeSearchString();
    else splitString(searchEventOrRec.m_searchString, searchResult.delim(), 4, movieName, episodeSearchString);
  searchEventOrRec.UpdateEpisodeListIfRequired(movieOrTv.id);
  int distance = cMovieOrTv::searchEpisode(searchEventOrRec.m_db, movieOrTv, episodeSearchString, "");
  searchEventOrRec.m_episodeFound = distance != 1000;
  searchResult.setMatchEpisode(distance);
}

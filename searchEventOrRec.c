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
//    bool debug = strcmp(recording->Info()->Title(), "Hawaii Five-0") == 0;
    bool debug = false;
    debug = false;
    cString baseName = recording->BaseName();
    size_t baseNameLen = strlen(baseName);
    if (baseName[0] == '%') m_baseNameOrTitle = ( (const char *)baseName ) + 1;
                       else m_baseNameOrTitle = ( (const char *)baseName );
    if (!recording->Info()->Title() ) return; // no title, go ahead with basename, best what we have (should not happen)
    if (isVdrDate(m_baseNameOrTitle) ) {
// this normally happens if VDR records a series, and the subtitle is empty)
      m_baseNameOrTitle = recording->Info()->Title();
      if (recording->Info()->ShortText() ) return; // strange, but go ahead with title
      const char * title_pos = strstr(recording->Name(), recording->Info()->Title() );
      if (!title_pos || (size_t(title_pos - recording->Name() ) >= strlen(recording->Name()) - baseNameLen)  ) return; // title not part of the name, or title only part of the base name-> does not match the pattern -> return
      m_baseNameEquShortText = true;
      return;
    }
// if the title is somewhere in the recording name (except the basename part), 
// and the basename is part of the ShortText() (or Description()), we assume that this is a series
// like Merlin~1/13. The Dragon's Call
    if (debug) esyslog("tvscraper: TEST recording->Info()->Title() ) \"%s\", recording->Name() \"%s\", recording->Info()->ShortText() \"%s\"", recording->Info()->Title(), recording->Name(), recording->Info()->ShortText() );

// check: title part of the first part of the name, excluding the basename
    if (strstr(recording->Info()->Title(), m_baseNameOrTitle.c_str() ) != NULL) return; // basename is part of title, seems not to be the subtitle
    const char * title_pos = strstr(recording->Name(), recording->Info()->Title() );
    if (debug) esyslog("tvscraper: TEST (1.1) size_t(title_pos - recording->Name() ) %lu, strlen(recording->Name() ) - baseNameLen %lu", size_t(title_pos - recording->Name() ), strlen(recording->Name() ) - baseNameLen);
    if (!title_pos || (size_t(title_pos - recording->Name() ) >= strlen(recording->Name()) - baseNameLen)  ) return; // title not part of the name, or title only part of the base name-> does not match the pattern -> return
    if (debug) esyslog("tvscraper: TEST (2) recording->Info()->Title() ) \"%s\", recording->Name() \"%s\", recording->Info()->ShortText() \"%s\"", recording->Info()->Title(), recording->Name(), recording->Info()->ShortText() );
// check: basename is part of the ShortText() (or Description())
    if (( recording->Info()->ShortText() && strstr(recording->Info()->ShortText(), m_baseNameOrTitle.c_str() ) ) ||
        (!recording->Info()->ShortText() && recording->Info()->Description() && strstr(recording->Info()->Description(), m_baseNameOrTitle.c_str() ) ) ) {
// this is a series, BaseName == ShortText() == episode name
    if (debug) esyslog("tvscraper: TEST (3) recording->Info()->Title() ) \"%s\", recording->Name() \"%s\", recording->Info()->ShortText() \"%s\"", recording->Info()->Title(), recording->Name(), recording->Info()->ShortText() );
      m_baseNameEquShortText = true;
      m_baseNameOrTitle = recording->Info()->Title();
    }
  } else m_baseNameOrTitle = m_sEventOrRecording->Title();
}

bool cSearchEventOrRec::isVdrDate(const std::string &baseName) {
// return true if string matches the 2020.01.12-02:35 pattern.
  if (baseName.length() < 16) return false;

  int n;
  for (n = 0; n < 4; n ++) if (!isdigit(baseName[n]) ) return false;
  if (baseName[n] != '.') return false;
  for (n++ ; n < 7; n ++) if (!isdigit(baseName[n]) ) return false;
  if (baseName[n] != '.') return false;
  for (n++ ; n < 10; n ++) if (!isdigit(baseName[n]) ) return false;
  if (baseName[n] != '-') return false;
  for (n++ ; n < 13; n ++) if (!isdigit(baseName[n]) ) return false;
  if (baseName[n] != ':') return false;
  for (n++ ; n < 16; n ++) if (!isdigit(baseName[n]) ) return false;

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

bool cSearchEventOrRec::Scrap(void) {
// return true, if request to rate limited internet db was required. Otherwise, false
  if (config.enableDebug) esyslog("tvscraper: Scrap Event ID , search string \"%s\", title \"%s\"", m_searchString.c_str(), m_sEventOrRecording->Title());
  if (m_overrides->Ignore(m_baseNameOrTitle)) return extDbConnected;
  sMovieOrTv movieOrTv;
  ScrapFindAndStore(movieOrTv);
  if(movieOrTv.type != scrapMovie && movieOrTv.type != scrapSeries) {
// nothing found, try again with title
    m_baseNameOrTitle = m_sEventOrRecording->Title();
    m_baseNameEquShortText = false;
    initSearchString();
    ScrapFindAndStore(movieOrTv);
  }
  ScrapAssign(movieOrTv);
  return extDbConnected;
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
        episodeSearchString = m_sEventOrRecording->EpisodeSearchString();
        if (searchResults[0].getMatchEpisode() ) movieOrTv.episodeSearchWithShorttext = 3;
      }
      UpdateEpisodeListIfRequired(movieOrTv.id);
      cMovieOrTv::searchEpisode(m_db, movieOrTv, episodeSearchString);
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

scrapType cSearchEventOrRec::ScrapFind(vector<searchResultTvMovie> &searchResults, string &movieName, string &episodeSearchString) {
//  bool debug = m_searchString == "james cameron's dark angel";
//  bool debug = m_searchString == "mercenario - der gefürchtete";
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
  sort(searchResults.begin(), searchResults.end() );
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
  bool debug = movieName == "rose" && delimiter == '-';
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
      UpdateEpisodeListIfRequired(movieOrTv.id);
      episodeSearchString = m_sEventOrRecording->EpisodeSearchString();
      cMovieOrTv::searchEpisode(m_db, movieOrTv, episodeSearchString);
      if (movieOrTv.episodeSearchWithShorttext == 3 &&
        movieOrTv.season == 0 && movieOrTv.episode == 0) return false;
// episode match required for cache, but not found
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
// and download episode still
  if(movieOrTv.type != scrapMovie && movieOrTv.type != scrapSeries) return; // if nothing was found, there is nothing todo
  if (!m_sEventOrRecording->Recording() )
          m_db->InsertEvent     (m_sEventOrRecording, movieOrTv.id, movieOrTv.season, movieOrTv.episode);
     else m_db->InsertRecording2(m_sEventOrRecording, movieOrTv.id, movieOrTv.season, movieOrTv.episode);
  if(movieOrTv.type == scrapMovie) {
    m_moviedbScraper->DownloadMedia(movieOrTv.id);
    m_moviedbScraper->DownloadActors(movieOrTv.id, true);
  }
  if(movieOrTv.type == scrapSeries) {
    if (movieOrTv.id > 0) {
      m_moviedbScraper->DownloadMediaTv(movieOrTv.id);
      m_moviedbScraper->DownloadActors(movieOrTv.id, false);
    } else {
      m_tvdbScraper->StoreActors(movieOrTv.id * -1);
      m_tvdbScraper->DownloadMedia(movieOrTv.id * -1);
    }
    string episodeStillPath = m_db->GetEpisodeStillPath(movieOrTv.id, movieOrTv.season, movieOrTv.episode);
    if (!episodeStillPath.empty() ) {
      if (movieOrTv.id > 0)
        m_moviedbScraper->StoreStill (movieOrTv.id     , movieOrTv.season, movieOrTv.episode, episodeStillPath);
      else m_tvdbScraper->StoreStill (movieOrTv.id * -1, movieOrTv.season, movieOrTv.episode, episodeStillPath);
    }
  }
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
  if (actor.length() < 3) return;
  const char *description = m_sEventOrRecording->Description();
  if (!description) return;
  if (strstr_word(description, actor.c_str() ) != 0) { numMatchesAll++; return; }
  size_t pos_blank = actor.rfind(' ');
  if (pos_blank == std::string::npos || pos_blank == 0) return;
  size_t pos_start = actor.rfind(' ', pos_blank-1);
  if (pos_start == std::string::npos) pos_start = 0;
    else pos_start++; // first character after the blank
  if ((actor.length() - pos_blank > 3) ) addActor(description, actor.c_str() + pos_blank + 1, numMatchesSure, alreadyFound);
  if (pos_blank - pos_start > 2) {
    addActor(description, string(actor, pos_start, pos_blank - pos_start), numMatchesFirst, alreadyFound);
  } else {
// word (first name) too short, could be an article, try word before this
    pos_blank = pos_start - 1;
    if (pos_blank <= 0) return; // no previous word
    pos_start = actor.rfind(' ', pos_blank-1);
    if (pos_start == std::string::npos) pos_start = 0;
      else pos_start++; // first character after the blank
    addActor(description, string(actor, pos_start, pos_blank - pos_start), numMatchesFirst, alreadyFound);
  }
}
void cSearchEventOrRec::addActor(const char *description, const string &name, int &numMatches, vector<string> &alreadyFound) {
// search name in description, if found, increase &numMatches. But only once for each name found
  if (name.length() < 3) return; // ignore if name is too short
  if (strstr_word(description, name.c_str() ) == NULL) return;   // name not found in description
  if (find (alreadyFound.begin(), alreadyFound.end(), name) != alreadyFound.end() ) return;
  alreadyFound.push_back(name);
  numMatches++;
}


void cSearchEventOrRec::getActorMatches(searchResultTvMovie &sR, const std::vector<std::vector<std::string>> &actors) {
  int numMatchesAll = 0;
  int numMatchesFirst = 0;
  int numMatchesSure = 0;
  vector<string> alreadyFound;
  bool debug = false;
//  bool debug = (sR.id() == -83269) || (sR.id() == 689390);
//  debug = sR.id() == -353808;
  if (debug) esyslog("tvscraper: getActorMatches, id: %i", sR.id() );
  for(const std::vector<std::string> &actor : actors) if (actor.size() == 3) {
    if (debug) esyslog("tvscraper: getActorMatches, Actor: %s, numMatchesAll %i, numMatchesFirst %i, numMatchesSure %i", actor[1].c_str(), numMatchesAll, numMatchesFirst, numMatchesSure);
    getActorMatches(actor[1], numMatchesAll, numMatchesFirst, numMatchesSure, alreadyFound);
    if (debug) esyslog("tvscraper: getActorMatches, Actor: %s, numMatchesAll %i, numMatchesFirst %i, numMatchesSure %i", actor[2].c_str(), numMatchesAll, numMatchesFirst, numMatchesSure);
    getActorMatches(actor[2], numMatchesAll, numMatchesFirst, numMatchesSure, alreadyFound);
  }
  int sum = numMatchesFirst + numMatchesSure + 2*numMatchesAll;
  sR.setActors(numMatchesFirst + numMatchesSure + 2*numMatchesAll);
  // if (debug) esyslog("tvscraper: getActorMatches, numMatchesAll %i, numMatchesFirst %i, numMatchesSure %i, sum %i, weight", numMatchesAll, numMatchesFirst, numMatchesSure, sum);
  if (debug) esyslog("tvscraper: getActorMatches, numMatchesAll %i, numMatchesFirst %i, numMatchesSure %i, sum %i, weight %f", numMatchesAll, numMatchesFirst, numMatchesSure, sum, searchResultTvMovie::normMatch(sum/4.));
}

bool cSearchEventOrRec::selectBestAndEnhanvceIfRequired(std::vector<searchResultTvMovie>::iterator begin, std::vector<searchResultTvMovie>::iterator end, std::vector<searchResultTvMovie>::iterator &new_end, float minDiff, void (*func)(searchResultTvMovie &sR, cSearchEventOrRec &searchEventOrRec)) {
// return true if enhancement was required
// in this case, return the end of the enhance list in new_end
// minDiff must be > 0, otherwise an empty list my be returned
  bool debug = m_searchString == "james cameron's dark angel";
  float minDiffSame = max (minDiff - 0.05, 0.01);
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
//  if (sR.id() == 689390 || sR.id() == -83269)
  bool debug = sR.id() == -353808 || sR.id() == -250822 || sR.id() == 440757 || sR.id() == -258828 || sR.id() == -412485 || sR.id() == -295683;
  debug = debug || searchEventOrRec.m_searchString == "die göttliche sophie";
  debug = debug || searchEventOrRec.m_searchString == "rose - königin der blumen";
  debug = debug || searchEventOrRec.m_searchString == "sendung vom 19.02. 09:30 uhr";
  debug = debug || searchEventOrRec.m_searchString == "sendung vom 19.02. 09";
  debug = debug || searchEventOrRec.m_searchString == "exploding sun 1";
  debug = debug || searchEventOrRec.m_searchString == "exploding sun 2";
  debug = debug || searchEventOrRec.m_searchString == "stargate";
  debug = false;
  if (debug) sR.log(searchEventOrRec.m_searchString.c_str() );
  std::vector<std::vector<std::string>> actors;
  if (sR.movie() ) {
// movie
    sR.setDuration(searchEventOrRec.m_sEventOrRecording->DurationDistance(searchEventOrRec.m_moviedbScraper->GetMovieRuntime(sR.id() )) );
    sR.updateMatchText(sentence_distance(searchEventOrRec.m_db->GetMovieTagline(sR.id() ), searchEventOrRec.m_searchString));
    actors = searchEventOrRec.m_db->GetActorsMovie(sR.id() );
  } else {
// tv show
    if (debug) esyslog("tvscraper: enhance1 (1)" );
    if (!debug) sR.setDuration(searchEventOrRec.GetTvDurationDistance(sR.id() ) );
    if (debug) esyslog("tvscraper: enhance1 (2)" );
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

  if (searchEventOrRec.m_episodeFound || searchResult.movie() ) { searchResult.setMatchEpisode(0); return; }
  string movieName;
  string episodeSearchString;
  sMovieOrTv movieOrTv;
  movieOrTv.id = searchResult.id();
  movieOrTv.type = scrapSeries;
  searchEventOrRec.Store(movieOrTv);
  if (debug) esyslog("tvscraper: enhance2 (3)" );
// search episode
  movieOrTv.episodeSearchWithShorttext = searchResult.delim()?0:1;
  if (movieOrTv.episodeSearchWithShorttext) episodeSearchString = searchEventOrRec.m_sEventOrRecording->EpisodeSearchString();
    else splitString(searchEventOrRec.m_searchString, searchResult.delim(), 4, movieName, episodeSearchString);
  searchEventOrRec.UpdateEpisodeListIfRequired(movieOrTv.id);
  std::size_t nmatch = cMovieOrTv::searchEpisode(searchEventOrRec.m_db, movieOrTv, episodeSearchString);
  searchEventOrRec.m_episodeFound = nmatch > 0;
  searchResult.setMatchEpisode(nmatch);
}

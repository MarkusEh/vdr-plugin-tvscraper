#include "searchEventOrRec.h"

cSearchEventOrRec::cSearchEventOrRec(csEventOrRecording *sEventOrRecording, cOverRides *overrides, cMovieDBScraper *moviedbScraper, cTVDBScraper *tvdbScraper, cTVScraperDB *db):
  m_sEventOrRecording(sEventOrRecording),
  m_overrides(overrides),
  m_moviedbScraper(moviedbScraper),
  m_tvdbScraper(tvdbScraper),
  m_db(db),
  m_TVtv(db, tvdbScraper),
  m_tv(db, moviedbScraper),
  m_movie(db, moviedbScraper)
  {
  m_searchResult_TvEpisShortText.distance = -1;
  m_searchResult_Movie.distance = -1;
  initBaseNameOrTitile();
  initSearchString();
  m_sEventOrRecording->AddYears(m_years);
  ::AddYears(m_years, m_baseNameOrTitile.c_str() );
  ::AddYears(m_years, m_searchString.c_str() );
  }
void cSearchEventOrRec::initBaseNameOrTitile(void) {
// initialize m_baseNameOrTitile
  const cRecording *recording = m_sEventOrRecording->Recording();
  if (recording) {
//    bool debug = strcmp(recording->Info()->Title(), "Hawaii Five-0") == 0;
    bool debug = false;
    debug = false;
    cString baseName = recording->BaseName();
    size_t baseNameLen = strlen(baseName);
    if (baseName[0] == '%') m_baseNameOrTitile = ( (const char *)baseName ) + 1;
                       else m_baseNameOrTitile = ( (const char *)baseName );
// if the title is somewhere in the recording name (except the basename part), 
// and the basename is part of the ShortText() (or Description()), we assume that this is a series
// like Merlin~1/13. The Dragon's Call
    if (debug) esyslog("tvscraper: TEST recording->Info()->Title() ) \"%s\", recording->Name() \"%s\", recording->Info()->ShortText() \"%s\"", recording->Info()->Title(), recording->Name(), recording->Info()->ShortText() );

    if (!recording->Info()->Title() ) return; // no title, test makes no sense
// check: title part of the first part of the name, excluding the basename
    if (strstr(recording->Info()->Title(), m_baseNameOrTitile.c_str() ) > 0) return; // basename is part of title, seems not to be the subtitle
    const char * title_pos = strstr(recording->Name(), recording->Info()->Title() );
    if (debug) esyslog("tvscraper: TEST (1.1) size_t(title_pos - recording->Name() ) %lu, strlen(recording->Name() ) - baseNameLen %lu", size_t(title_pos - recording->Name() ), strlen(recording->Name() ) - baseNameLen);
    if (!title_pos || (size_t(title_pos - recording->Name() ) >= strlen(recording->Name()) - baseNameLen)  ) return; // title not part of the name, or title only part of the base name-> does not match the pattern -> return
    if (debug) esyslog("tvscraper: TEST (2) recording->Info()->Title() ) \"%s\", recording->Name() \"%s\", recording->Info()->ShortText() \"%s\"", recording->Info()->Title(), recording->Name(), recording->Info()->ShortText() );
// check: basename is part of the ShortText() (or Description())
    if (( recording->Info()->ShortText() && strstr(recording->Info()->ShortText(), m_baseNameOrTitile.c_str() ) ) ||
        (!recording->Info()->ShortText() && recording->Info()->Description() && strstr(recording->Info()->Description(), m_baseNameOrTitile.c_str() ) ) ) {
// this is a series, BaseName == ShortText() == episode name
    if (debug) esyslog("tvscraper: TEST (3) recording->Info()->Title() ) \"%s\", recording->Name() \"%s\", recording->Info()->ShortText() \"%s\"", recording->Info()->Title(), recording->Name(), recording->Info()->ShortText() );
      m_baseNameEquShortText = true;
      m_baseNameOrTitile = recording->Info()->Title();
    }
  } else m_baseNameOrTitile = m_sEventOrRecording->Title();
}
void cSearchEventOrRec::initSearchString(void) {
  m_searchString = m_baseNameOrTitile;
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
  if (m_overrides->Ignore(m_baseNameOrTitile)) return extDbConnected;
  sMovieOrTv movieOrTv;
  ScrapFindAndStore(movieOrTv);
  if(movieOrTv.type != scrapMovie && movieOrTv.type != scrapSeries) {
// nothing found, try again with title
    m_baseNameOrTitile = m_sEventOrRecording->Title();
    m_baseNameEquShortText = false;
    m_searchResult_Movie.distance = -1; // clear cache
    m_searchResult_TvEpisShortText.distance = -1;
    initSearchString();
    ScrapFindAndStore(movieOrTv);
  }
  ScrapAssign(movieOrTv);
  return extDbConnected;
}
void cSearchEventOrRec::ScrapFindAndStore(sMovieOrTv &movieOrTv) {
  if (CheckCache(movieOrTv) ) return;
  if (config.enableDebug) esyslog("tvscraper: scraping \"%s\"", m_searchString.c_str());
  searchResultTvMovie searchResult;
  string episodeSearchString;
  string movieName;
  scrapType sType = ScrapFind(searchResult, movieName, episodeSearchString);

  if (searchResult.id == 0) sType = scrapNone;
  movieOrTv.id = searchResult.id;
  movieOrTv.type = sType;
  movieOrTv.year = searchResult.year;
  Store(movieOrTv);
  if(sType == scrapMovie) {
    movieOrTv.season = -100;
    movieOrTv.episode = 0;
    if (config.enableDebug) esyslog("tvscraper: movie \"%s\" successfully scraped, id %i", movieName.c_str(), searchResult.id);
  } else if(sType == scrapSeries) {
// search episode
    movieOrTv.episodeSearchWithShorttext = episodeSearchString.empty();
    if (movieOrTv.episodeSearchWithShorttext) episodeSearchString = m_sEventOrRecording->EpisodeSearchString();
    UpdateEpisodeListIfRequired(movieOrTv.id);
    m_db->SearchEpisode(movieOrTv, episodeSearchString);
    if (config.enableDebug) esyslog("tvscraper: %stv \"%s\", episode \"%s\" successfully scraped, id %i season %i episode %i", searchResult.id > 0?"":"TV", movieName.c_str(), episodeSearchString.c_str(), movieOrTv.id, movieOrTv.season, movieOrTv.episode);
  } else if (config.enableDebug) esyslog("tvscraper: nothing found for \"%s\" ", movieName.c_str());
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

scrapType cSearchEventOrRec::ScrapFind(searchResultTvMovie &searchResult, string &movieName, string &episodeSearchString) {
  movieName = m_searchString;
  episodeSearchString = "";
  scrapType type_override = m_overrides->Type(m_baseNameOrTitile);
// check for movie: save best movie in m_searchResult_Movie
  if (type_override != scrapSeries) SearchMovie();
     else m_searchResult_Movie.id = 0;
// check for series
  if (type_override != scrapMovie) SearchTvEpisShortText(searchResult);
     else searchResult.id = 0;
  if (m_searchResult_Movie.id == 0 && searchResult.id != 0) return scrapSeries;
  if (m_searchResult_Movie.id != 0 && searchResult.id == 0) { searchResult = m_searchResult_Movie; return scrapMovie; }
  if (m_searchResult_Movie.id != 0 && searchResult.id != 0) {
// found a movie & a series. Which one is better?
    if (m_sEventOrRecording->DurationInSec() > 80*60) { searchResult = m_searchResult_Movie; return scrapMovie; }
    if (m_baseNameEquShortText) return scrapSeries;
// check series: do we have the episodes?
    sMovieOrTv movieOrTv;
    movieOrTv.id = searchResult.id;
    movieOrTv.type = scrapSeries;
    Store(movieOrTv);
// search episode
    movieOrTv.episodeSearchWithShorttext = true;
    string episodeSearchString = m_sEventOrRecording->EpisodeSearchString();
    UpdateEpisodeListIfRequired(movieOrTv.id);
    if (m_db->SearchEpisode(movieOrTv, episodeSearchString) ) return scrapSeries;  // episode was found
    if (m_searchResult_Movie.durationDistance != 0) return scrapSeries;
    if (m_searchResult_Movie.fastMatch > 0.5) { searchResult = m_searchResult_Movie; return scrapMovie; }
    return scrapSeries;
  }
// nothing found so far
// check: series, where name & episode name is in the title
  if (type_override != scrapMovie) {
    if (SearchTvEpisTitle(searchResult, movieName, episodeSearchString, ':') < 250) return scrapSeries;
    if (SearchTvEpisTitle(searchResult, movieName, episodeSearchString, '-') < 250) return scrapSeries;
  }
// nothing found
  return scrapNone;
}


bool CompareSearchResult2 (searchResultTvMovie i, searchResultTvMovie j) {
// used for tv.
  if (i.distance == j.distance) {
    if (i.id > 0) return i.popularity > j.popularity;
    return i.positionInExternalResult < j.positionInExternalResult;
  }
  return i.distance < j.distance;  // 0-1000, lower values are better
}

int cSearchEventOrRec::SearchTv(searchResultTvMovie &searchResult, const string &searchString) {
  vector<searchResultTvMovie> resultSet;
  extDbConnected = true;
  string searchString1 = SecondPart(searchString, ":");
  string searchString2 = SecondPart(searchString, "'s");
  m_tv.AddTvResults(resultSet, searchString, searchString);
  if (searchString1.length() > 4 ) m_tv.AddTvResults(resultSet, searchString, searchString1);
  if (searchString2.length() > 4 ) m_tv.AddTvResults(resultSet, searchString, searchString2);
  m_TVtv.AddResults(resultSet, searchString, searchString);
  if (searchString1.length() > 4 ) m_TVtv.AddResults(resultSet, searchString, searchString1);
  if (searchString2.length() > 4 ) m_TVtv.AddResults(resultSet, searchString, searchString2);
  if (resultSet.size() == 0) {
    searchResult.id = 0;          // nothing found
    searchResult.distance = 1000; // nothing found
  } else {
    std::sort (resultSet.begin(), resultSet.end(), CompareSearchResult2);
    searchResult= resultSet[0];
  }
  return searchResult.distance;
}
int cSearchEventOrRec::SearchTvEpisShortText(searchResultTvMovie &searchResult) {
  if (m_searchResult_TvEpisShortText.distance != -1) {
    searchResult = m_searchResult_TvEpisShortText;
    return m_searchResult_TvEpisShortText.distance;
  }
  SearchTv(searchResult, m_searchString);
  m_searchResult_TvEpisShortText = searchResult;
  return m_searchResult_TvEpisShortText.distance;
}
int cSearchEventOrRec::SearchTvCacheSearchString(searchResultTvMovie &searchResult, const string &searchString) {
  map<string,searchResultTvMovie>::iterator cacheHit = m_cacheTv.find(searchString);
  if (cacheHit != m_cacheTv.end() ) {
    if (config.enableDebug) esyslog("tvscraper: found tv cache %s => %i", ((string)cacheHit->first).c_str(), (int)cacheHit->second.id);
    searchResult = cacheHit->second;
    return searchResult.distance;
  }
  SearchTv(searchResult, searchString);
  m_cacheTv.insert(pair<string, searchResultTvMovie>(searchString, searchResult));
  return searchResult.distance;
}

int cSearchEventOrRec::SearchTvEpisTitle(searchResultTvMovie &searchResult, string &movieName, string &episodeSearchString, char delimiter) {
  std::size_t found = m_searchString.find(delimiter);
  searchResult.distance = 1000; // default, nothing found
  if(found == std::string::npos || found <= 4) return searchResult.distance; // nothing found, or first part to short
  std::size_t ssnd;
  for(ssnd = found + 1; ssnd < m_searchString.length() && m_searchString[ssnd] == ' '; ssnd++);
  if(m_searchString.length() - ssnd <= 4) return searchResult.distance; // nothing found, second part to short

  episodeSearchString = m_searchString.substr(ssnd);
  movieName = m_searchString.substr(0, found);
  StringRemoveTrailingWhitespace(movieName);
  return SearchTvCacheSearchString(searchResult, movieName);
}

float popularityFactor(float popularity) {
// checks for themoviedb
// series Andromeda 	64.052
// Andromeda - Tödlicher Staub aus dem All 20.489
// "Der Junge von Andromeda" 2.693
// "A come Andromeda" 1.634
// "The Andromeda Strain" 17.38
// "A for Andromeda" 4.687
// "Andrômeda" 1.482
// Star Trek Evolutions 6.088
// Die Heiland: Wir sind Anwalt 18
// Die Sehnsucht der Schwestern Gusma 9.189
  if (popularity >= 10.) return 0.95 + min((popularity-10.0), 100.0) /10000. * 5.; // a number betwenn 0.95 and 1.00
  if (popularity >= 05.) return 0.50 + (popularity-5.0)/100.*9.; // a number betwenn 0.50 and 0.95
  if (popularity >= 02.) return 0.10 + (popularity-2.0)/3.*4./10.; // a number betwenn 0.10 and 0.50
  if (popularity <  .00001) return 0.3; // no data available
  return popularity/2./10.; // a number betwenn 0.00 and 0.10
}
float runtimeFactor(int durationDistance) {
// durationDistance in mins, < 0: no data
  if (durationDistance <  0) return 0.5; // default, if no data available
  if (durationDistance == 0) return 1.0;
  if (durationDistance < 10) return 0.8 + (10. - durationDistance)/10.*0.2; // between 0.8 and 1.0
  return 0.1 + max((100. - durationDistance), 0.) / 90.*.7; // between 0.1 and 0.8
}
void cSearchEventOrRec::setFastMatch(searchResultTvMovie &searchResult) {
// we have: distance 0-1000
// year (0/1)
// popularity
  if (m_searchStringSubstituted && searchResult.distance != 0) {
// for substituted strings, we expect an exact match
    searchResult.fastMatch = 0;
    return;
  }
  int match_max = 1000 + 150 + 150 + 500;
  int match0 = 1000 - searchResult.distance + (searchResult.year>0?150:searchResult.year<0?100:0) + popularityFactor(searchResult.popularity) * 150.; // higher number is better match
  float match1 = (match0  + 500.)/ (float)match_max; // even if match0==0, this will give 0.33: This acconts for the search algorithm in themoviedb, which found this item
  searchResult.fastMatch = match1;
}
bool CompareSearchResultMovie0 (searchResultTvMovie i, searchResultTvMovie j) {
// for first sort, without durationDistance
  return i.fastMatch > j.fastMatch;
}
bool CompareSearchResultMovie1 (searchResultTvMovie i, searchResultTvMovie j) {
// for second sort, with durationDistance
  return (i.fastMatch + runtimeFactor(i.durationDistance) * 0.2 ) > (j.fastMatch + runtimeFactor(j.durationDistance) * 0.2);
}

int cSearchEventOrRec::SearchMovie(void) {
  if (m_searchResult_Movie.distance != -1) return m_searchResult_Movie.id;
//  bool debug = strcmp(m_sEventOrRecording->Title(), "Der kleine Lord") == 0;
  bool debug = false;
  vector<searchResultTvMovie> resultSet;
  extDbConnected = true;
  string searchString1 = SecondPart(m_searchString, ":");
  string searchString2 = SecondPart(m_searchString, "'s");
  m_movie.AddMovieResults(resultSet, m_searchString, m_searchString, m_sEventOrRecording);
  if (searchString1.length() > 4 ) m_movie.AddMovieResults(resultSet, m_searchString, searchString1, m_sEventOrRecording);
  if (searchString2.length() > 4 ) m_movie.AddMovieResults(resultSet, m_searchString, searchString2, m_sEventOrRecording);
  if (resultSet.size() == 0) {
    m_searchResult_Movie.id = 0       ; // nothing found
    m_searchResult_Movie.distance = 1000;
  } else {
    for (searchResultTvMovie &sR: resultSet) { setFastMatch(sR); sR.durationDistance = -1; }
    std::sort (resultSet.begin(), resultSet.end(), CompareSearchResultMovie0);
// pre-sort done. Now, calculate durationDistance for most important results
    for (searchResultTvMovie &sR: resultSet) {
      sR.durationDistance = m_sEventOrRecording->DurationDistance(m_moviedbScraper->GetMovieRuntime(sR.id));
      if (debug) {
        int durationInMinLow, durationInMinHigh;
        m_sEventOrRecording->DurationRange(durationInMinLow, durationInMinHigh);
        esyslog("tvscraper: SearchMovie: Id %i, durationDistance %i, movieRuntime %i, durationInMinLow %i, durationInMinHigh %i, fastMatch %f, popularity %f, popularity_factor %f, runtimeFactor %f", sR.id, sR.durationDistance, m_moviedbScraper->GetMovieRuntime(sR.id), durationInMinLow, durationInMinHigh, sR.fastMatch, sR.popularity, popularityFactor(sR.popularity), runtimeFactor(sR.durationDistance) );
      }
      if (sR.durationDistance == 0) break;   // movie with matching duration found, no need to calculate futher durations
    }
    std::sort (resultSet.begin(), resultSet.end(), CompareSearchResultMovie1);
    m_searchResult_Movie = resultSet[0];
/*
       else if (config.enableDebug) 
        int durationInMinLow, durationInMinHigh;
        m_sEventOrRecording->DurationRange(durationInMinLow, durationInMinHigh);
        esyslog("tvscraper: movie not selected, no duration match. Id %i, durationDistance %i, movieRuntime %i, durationInMinLow %i, durationInMinHigh %i", sR.id, sR.durationDistance, m_moviedbScraper->GetMovieRuntime(sR.id), durationInMinLow, durationInMinHigh);
*/
  }
  return m_searchResult_Movie.id;
}

bool cSearchEventOrRec::CheckCache(sMovieOrTv &movieOrTv) {
  if (!m_db->GetFromCache(m_searchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText) ) return false;
// cache found
   Store(movieOrTv);
// do we have to search the episode?
  string episodeSearchString;
  if (movieOrTv.season != -100 && movieOrTv.episodeSearchWithShorttext) {
    UpdateEpisodeListIfRequired(movieOrTv.id);
    episodeSearchString = m_sEventOrRecording->EpisodeSearchString();
    m_db->SearchEpisode(movieOrTv, episodeSearchString);
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
  if(movieOrTv.type != scrapMovie && movieOrTv.type != scrapSeries) return; // if noting was found, there is nothing todo
  if (!m_sEventOrRecording->Recording() )
          m_db->InsertEvent     (m_sEventOrRecording, movieOrTv.id, movieOrTv.season, movieOrTv.episode);
     else m_db->InsertRecording2(m_sEventOrRecording, movieOrTv.id, movieOrTv.season, movieOrTv.episode);
  if(movieOrTv.type == scrapSeries) {
    string episodeStillPath = m_db->GetEpisodeStillPath(movieOrTv.id, movieOrTv.season, movieOrTv.episode);
    if (!episodeStillPath.empty() ) {
      if (movieOrTv.id > 0)
            m_moviedbScraper->StoreStill(movieOrTv.id     , movieOrTv.season, movieOrTv.episode, episodeStillPath);
      else {
        m_tvdbScraper->StoreStill (movieOrTv.id * -1, movieOrTv.season, movieOrTv.episode, episodeStillPath);
        m_tvdbScraper->StoreActors(movieOrTv.id * -1);
      }
    }
  }
}

void cSearchEventOrRec::UpdateEpisodeListIfRequired(int tvID) {
// check: is update required?
  string status;
  time_t lastUpdated = 0;
  time_t now = time(0);

  m_db->GetTv(tvID, lastUpdated, status);
  if (lastUpdated) {
    if (difftime(now, lastUpdated) < 7*24*60*60 ) return; // min one week between updates
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


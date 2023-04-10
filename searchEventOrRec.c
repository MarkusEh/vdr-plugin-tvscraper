#include "searchEventOrRec.h"

cSearchEventOrRec::cSearchEventOrRec(csEventOrRecording *sEventOrRecording, cOverRides *overrides, cMovieDBScraper *moviedbScraper, cTVDBScraper *tvdbScraper, cTVScraperDB *db):
  m_sEventOrRecording(sEventOrRecording),
  m_overrides(overrides),
  m_moviedbScraper(moviedbScraper),
  m_tvdbScraper(tvdbScraper),
  m_db(db),
  m_tv(db, moviedbScraper),
  m_movie(db, moviedbScraper),
  m_searchResult_Movie(0, true, "")
  {
    if (m_sEventOrRecording->Recording() && !m_sEventOrRecording->Recording()->Info() ) {
      esyslog("tvscraper: ERROR in cSearchEventOrRec: recording->Info() == NULL, Name = %s", m_sEventOrRecording->Recording()->Name() );
      return;
    }
    initOriginalTitle();
    initBaseNameOrTitle();
    m_TVshowSearchString = m_baseNameOrTitle;
    initSearchString3dots(m_TVshowSearchString);
    initSearchString3dots(m_movieSearchString);
    initSearchString(m_TVshowSearchString);
    initSearchString(m_movieSearchString);
    m_sEventOrRecording->AddYears(m_years);
    m_years.addYears((const char *)m_baseName);
    m_years.finalize();
  }

void cSearchEventOrRec::initOriginalTitle() {
  m_originalTitle = textAttributeValue(m_sEventOrRecording->Description(), "Originaltitel: ");
  if (m_originalTitle.length() > 0 && config.enableDebug) esyslog("tvscraper, original title = \"%.*s\"", static_cast<int>(m_originalTitle.length()), m_originalTitle.data() );
}

bool cSearchEventOrRec::isTitlePartOfPathName(size_t baseNameLen) {
  return minDistanceStrings(m_sEventOrRecording->Recording()->Name(), m_sEventOrRecording->Recording()->Info()->Title() ) < 600;
}

void cSearchEventOrRec::initSearchString3dots(std::string &searchString) {
  size_t len = searchString.length();
  if (len < 3) return;
  if (searchString.compare(len - 3, 3, "...") != 0) return;
// "title" ends with ...
  const char *shortText = m_sEventOrRecording->ShortText();
  if (!shortText || ! *shortText) shortText = m_sEventOrRecording->Description();
  if (!shortText || ! *shortText) return; // no short text, no description -> cannot continue ...
  for (int i = 0; i < 3 ; i++) if (shortText[i] != '.') return;
  shortText += 3;
// pattern match found. Now check how much of the short text we add
  const char *end = strpbrk (shortText, ".,;!?:(");
  size_t num_added = 0;
  if (end) num_added = end - shortText;
  else num_added = strlen(shortText);
  searchString.erase(len - 3);
  searchString.append(" ");
  searchString.append(shortText, num_added);
}

void cSearchEventOrRec::initBaseNameOrTitle(void) {
// initialize m_baseNameOrTitle
  const cRecording *recording = m_sEventOrRecording->Recording();
  if (!recording) {
// EPG event, we only have the title
    m_baseNameOrTitle = m_sEventOrRecording->Title();
    m_movieSearchString = m_baseNameOrTitle;
    return;
  }
// recording: we have the file name of the recording, the event title, ...

// Note: recording->Name():     This is the complete file path, /video prefix removed, and characters exchanged (like #3A -> :)
//                              In other words: You can display it on the UI
// Note: recording->BaseName(): Last part of recording->Name()  (after last '~')
  bool debug = false;
//  debug =  recording->Info() && recording->Info()->Title() && strcmp(recording->Info()->Title(), "Die Geburtstagssendung mit der Maus") == 0;
//  debug |= recording->Info() && recording->Info()->Title() && strcmp(recording->Info()->Title(), "Die Sendung mit der Maus: 25 Jahre nach dem Mauerfall (2/4)") == 0;

// set m_baseNameOrTitle (the db search string) to the recording file name, as default
  m_baseName = recording->BaseName();
  if ((const char *)m_baseName == NULL) esyslog("tvscraper: ERROR in cSearchEventOrRec::initBaseNameOrTitle: baseName == NULL");
  size_t baseNameLen = strlen(m_baseName);
  if (baseNameLen == 0) esyslog("tvscraper: ERROR in cSearchEventOrRec::initBaseNameOrTitle: baseNameLen == 0");
  if (m_baseName[0] == '%') m_baseNameOrTitle = ( (const char *)m_baseName ) + 1;
                       else m_baseNameOrTitle = ( (const char *)m_baseName );
// check: do we have something better? Note: for TV shows, the recording file name is often the name of the episode, and the title must be used as db search string
  m_movieSearchString = m_baseNameOrTitle;
  if (!recording->Info()->Title() ) return; // no title, go ahead with basename, best what we have (very old recording?)
  if (isVdrDate(m_baseNameOrTitle) || isVdrDate2(m_baseNameOrTitle) ) {
// this normally happens if VDR records a series, and the subtitle is empty
    m_baseNameOrTitle = recording->Info()->Title();
    m_movieSearchString = m_baseNameOrTitle;
    if (recording->Info()->ShortText() ) return; // strange, but go ahead with title
    if (!isTitlePartOfPathName(baseNameLen) ) return;
    if (recording->Info()->Description() ) {
      m_episodeName = m_sEventOrRecording->EpisodeSearchString();
      m_baseNameEquShortText = true;
    }
    return;
  }
// get the short text. If not available: Use the description instead
  const char *shortText = recording->Info()->ShortText();
  if (!shortText || ! *shortText) shortText = recording->Info()->Description();
  if (!shortText || ! *shortText) return; // no short text, no description -> go ahead with base name
  int distTitle     = sentence_distance(recording->Info()->Title(), m_baseNameOrTitle);
  int distShortText = sentence_distance(shortText, m_baseNameOrTitle);
  if (debug) esyslog("tvscraper: distTitle %i, distShortText %i", distTitle, distShortText);
  if (distTitle > 600 && distShortText > 600) {
// neither title nor shortText match the filename
    if (!isTitlePartOfPathName(baseNameLen) ) return;
    if (debug) esyslog("tvscraper: isTitlePartOfPathName(baseNameLen) == true");
// title is part of path name
// this indicates that we have a TV show with title=title, and episode name=filename
    m_episodeName = m_baseNameOrTitle;
    m_baseNameOrTitle = recording->Info()->Title();
    m_baseNameEquShortText = true;
    return;
  }
  if (distTitle <= distShortText) return; // in this case, m_baseNameOrTitle is the title, so go ahead with m_baseNameOrTitle
// name of recording is short text -> use name of recording as episode name, and title as name of TV show
  m_episodeName = m_baseNameOrTitle;
  m_baseNameOrTitle = recording->Info()->Title();
  m_baseNameEquShortText = true;
}

bool cSearchEventOrRec::isVdrDate(std::string_view baseName) {
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

bool cSearchEventOrRec::isVdrDate2(std::string_view baseName) {
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

void cSearchEventOrRec::initSearchString(std::string &searchString) {
  bool searchStringSubstituted = m_overrides->Substitute(searchString);
  if (!searchStringSubstituted) {
// some more, but only if no explicit substitution
    m_overrides->RemovePrefix(searchString);
    StringRemoveLastPartWithP(searchString); // always remove this: Year, number of season/episode, ... but not a part of the name
  }
  transform(searchString.begin(), searchString.end(), searchString.begin(), ::tolower);
}

cMovieOrTv *cSearchEventOrRec::Scrape(void) {
// return NULL if no movie/tv was assigned to this event/recording
// extDbConnected: true, if request to rate limited internet db was required. Otherwise, false
  if (config.enableDebug) {
    char buff[20];
    time_t event_time = m_sEventOrRecording->StartTime();
    strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&event_time));
    esyslog("tvscraper: Scrape: search string movie \"%s\", tv \"%s\", title \"%s\", start time: %s", m_movieSearchString.c_str(), m_TVshowSearchString.c_str(), m_sEventOrRecording->Title(), buff);
  }
  if (m_overrides->Ignore(m_baseNameOrTitle)) return NULL;
  sMovieOrTv movieOrTv;
  ScrapFindAndStore(movieOrTv);
  if(movieOrTv.type != scrapMovie && movieOrTv.type != scrapSeries) {
// nothing found, try again with title
    m_baseNameOrTitle = m_sEventOrRecording->Title();
    m_TVshowSearchString = m_baseNameOrTitle;
    m_movieSearchString = m_baseNameOrTitle;
    m_baseNameEquShortText = false;
    m_episodeName = "";
    initSearchString(m_TVshowSearchString);
    initSearchString(m_movieSearchString);
    ScrapFindAndStore(movieOrTv);
  }
  ScrapAssign(movieOrTv);
  return cMovieOrTv::getMovieOrTv(m_db, movieOrTv);
}

void cSearchEventOrRec::ScrapFindAndStore(sMovieOrTv &movieOrTv) {
  if (CheckCache(movieOrTv) ) return;
  if (config.enableDebug) esyslog("tvscraper: scraping movie \"%s\", TV \"%s\"", m_movieSearchString.c_str(), m_TVshowSearchString.c_str());
  vector<searchResultTvMovie> searchResults;
  string_view episodeSearchString;
  string_view foundName;
  scrapType sType = ScrapFind(searchResults, foundName, episodeSearchString);
  if (searchResults.size() == 0) sType = scrapNone;
  movieOrTv.type = sType;
  if (sType == scrapNone) {
// nothing found
    movieOrTv.id = 0;
    if (config.enableDebug) {
      esyslog("tvscraper: nothing found for movie \"%s\" TV \"%s\"", m_movieSearchString.c_str(), m_TVshowSearchString.c_str() );
      if (searchResults.size() > 0 ) searchResults[0].log(m_TVshowSearchString.c_str() );
    }
    m_db->InsertCache(m_movieSearchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText);
    m_db->InsertCache(m_TVshowSearchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText);
  } else {
// something found
    movieOrTv.id = searchResults[0].id();
    movieOrTv.year = searchResults[0].m_yearMatch?searchResults[0].year()*searchResults[0].m_yearMatch:0;
    Store(movieOrTv);
    if (sType == scrapMovie) {
      movieOrTv.season = -100;
      movieOrTv.episode = 0;
      if (config.enableDebug) esyslog("tvscraper: movie \"%.*s\" successfully scraped, id %i", static_cast<int>(foundName.length()), foundName.data(), searchResults[0].id() );
      m_db->InsertCache(m_movieSearchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText);
    } else if (sType == scrapSeries) {
// search episode
      const cLanguage *lang = m_sEventOrRecording->GetLanguage();
      UpdateEpisodeListIfRequired(movieOrTv.id, lang);
      movieOrTv.episodeSearchWithShorttext = episodeSearchString.empty()?1:0;
      if (movieOrTv.episodeSearchWithShorttext == 1) {
// pattern in title: TV show name
//    in short text: episode name
        episodeSearchString = m_baseNameEquShortText?m_episodeName:m_sEventOrRecording->EpisodeSearchString();
        if (searchResults[0].getMatchEpisode() ) {
          movieOrTv.episodeSearchWithShorttext = 3;
          float bestMatchText = searchResults[0].getMatchText();
          std::string tv_name;
          for (const searchResultTvMovie &searchResult: searchResults) {
            if (searchResult.id() == searchResults[0].id() ) continue;
            if ((searchResult.id()^searchResults[0].id()) < 0) continue; // we look for IDs in same database-> same sign
            if (searchResult.movie()) continue;  // we only look for TV shows
            if (abs(bestMatchText - searchResult.getMatchText()) > 0.001) continue;   // we only look for matches similar near
            if (sentence_distance_normed_strings(searchResults[0].normedName, searchResult.normedName) > 200) continue;
            m_db->setSimilar(searchResults[0].id(), searchResult.id() );
            if (config.enableDebug) esyslog("tvscraper: setSimilar %i and %i", searchResults[0].id(), searchResult.id());
          }
        }
        cMovieOrTv::searchEpisode(m_db, movieOrTv, episodeSearchString, m_baseNameOrTitle, m_years, lang);
        m_db->InsertCache(m_TVshowSearchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText);
      } else {
// pattern in title: TV show name - episode name
        cMovieOrTv::searchEpisode(m_db, movieOrTv, episodeSearchString, "", m_years, lang);
        m_db->InsertCache(m_movieSearchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText);
      }
      if (config.enableDebug) esyslog("tvscraper: %stv \"%.*s\", episode \"%.*s\" successfully scraped, id %i season %i episode %i", searchResults[0].id() > 0?"":"TV", static_cast<int>(foundName.length()), foundName.data(), static_cast<int>(episodeSearchString.length()), episodeSearchString.data(), movieOrTv.id, movieOrTv.season, movieOrTv.episode);
    }
    if (config.enableDebug) {
      searchResults[0].log(foundName);
      if (searchResults.size() > 1 ) {
        if (searchResults[1].movie() ) foundName = m_movieSearchString;
        else if (searchResults[1].delim() ) splitString(m_movieSearchString, searchResults[1].delim(), 4, foundName, episodeSearchString);
        else foundName = m_TVshowSearchString;
        searchResults[1].log(foundName);
      }
    }
  }
  return;
}

int cSearchEventOrRec::Store(const sMovieOrTv &movieOrTv) {
// return -1 if object does not exist in external db
// check if already in internal DB. If not: Download from external db
  switch (movieOrTv.type) {
    case scrapMovie:
      m_movie.SetID(movieOrTv.id);
      m_moviedbScraper->StoreMovie(m_movie);
      return 0;
    case scrapSeries:
      if (movieOrTv.id > 0) {
        m_tv.SetTvID(movieOrTv.id);
        m_tv.UpdateDb(false);
      } else {
        if (m_tvdbScraper->StoreSeriesJson(movieOrTv.id * (-1), false) == -1) return -1;
      }
      return 0;
    case scrapNone: ; // do nothing, nothing to store
  }
  return 0;
}

// required for older vdr versions, to make swap used by sort unambigous
inline void swap(searchResultTvMovie &a, searchResultTvMovie &b) { std::swap(a, b); }

scrapType cSearchEventOrRec::ScrapFind(vector<searchResultTvMovie> &searchResults, string_view &foundName, string_view &episodeSearchString) {
//  bool debug = m_TVshowSearchString == "james cameron's dark angel";
  bool debug = false;
  foundName = "";
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
  if (debug) for (searchResultTvMovie &searchResult: searchResults) searchResult.log(m_TVshowSearchString.c_str() );
  const float minMatchPre = 0.5;
  const float minMatchFinal = 0.5;
  std::vector<searchResultTvMovie>::iterator end = searchResults.begin();
  for (; end != searchResults.end(); end++) if (end->getMatch() < minMatchPre) break;
  if (searchResults.begin() == end) return scrapNone; // nothing good enough found
  m_episodeFound = false;
  std::vector<searchResultTvMovie>::iterator new_end;
// in case of results which are "similar" good, add more information to find the best one
  if (selectBestAndEnhanceIfRequired(searchResults.begin(), end,     new_end, 0.2,  &enhance1) ) {
//      esyslog("tvscraper: ScrapFind (5), about to call enhance2 search string = %s", m_TVshowSearchString.c_str()  );
// if we still have results which are "similar" good, add even more (and more expensive) information
      selectBestAndEnhanceIfRequired(searchResults.begin(), new_end, new_end, 0.15, &enhance2);
  }
// no more information can be added. Best result is in searchResults[0]
  if (debug) esyslog("tvscraper: ScrapFind, found: %i, title: \"%s\"", searchResults[0].id(), m_TVshowSearchString.c_str() );
  if (searchResults[0].getMatch() < minMatchFinal) return scrapNone; // nothing good enough found
  if (searchResults[0].movie() ) { foundName = m_movieSearchString; return scrapMovie;}
  if (searchResults[0].delim() ) splitString(m_movieSearchString, searchResults[0].delim(), 4, foundName, episodeSearchString);
  else foundName = m_TVshowSearchString;
  return scrapSeries;
}

int cSearchEventOrRec::GetTvDurationDistance(int tvID) {
  int finalDurationDistance = -1; // default, no data available
  const char *sql = "select episode_run_time from tv_episode_run_time where tv_id = ?";
  cSql stmt(m_db, sql, tvID);
  if (!stmt.readRow() ) {
    if (tvID>0) m_moviedbScraper->UpdateTvRuntimes(tvID);
    else m_tvdbScraper->UpdateTvRuntimes(tvID * -1);
    stmt.resetStep();
  }
// Done / checked: also write episode runtimes to episode_run_time, for each episode
  int n_col = 0;
  for (cSql &stm: stmt) {
    n_col++;
    int rt = stm.getInt(0);
    if (rt < 1) continue; // ignore 0 and -1: -1-> no value in ext. db. 0-> value 0 in ext. db
    int durationDistance = m_sEventOrRecording->DurationDistance(rt);
    if (finalDurationDistance == -1 || durationDistance < finalDurationDistance) finalDurationDistance = durationDistance;
  }
  if (n_col == 0) esyslog("tvscraper: ERROR, no runtime in cSearchEventOrRec::GetTvDurationDistance, tvID = %d", tvID);
  return finalDurationDistance;
}

void cSearchEventOrRec::SearchTvAll(vector<searchResultTvMovie> &searchResults) {
// search for TV shows, in all formats (episode in short text, or in title)
  if (m_originalTitle.length() > 0 && SearchTv(searchResults, m_originalTitle, true)) return;
  if (SearchTv(searchResults, m_TVshowSearchString)) return;
  SearchTvEpisTitle(searchResults, ':');
  SearchTvEpisTitle(searchResults, '-');
}

bool cSearchEventOrRec::SearchTv(vector<searchResultTvMovie> &resultSet, string_view searchString, bool originalTitle) {
// return true if a result matching searchString was found, and no splitting of searchString was required
// otherwise, return false. Note: also in this case some results might have been added
  extDbConnected = true;
  const cLanguage *lang = originalTitle?NULL:m_sEventOrRecording->GetLanguage();
  size_t oldSize = resultSet.size();
  if (m_tvdbScraper->AddResults4(resultSet, searchString, lang)) return true;
  if (resultSet.size() == oldSize) {
    m_moviedbScraper->AddTvResults(resultSet, searchString, lang);
  }
  return false;
}

void cSearchEventOrRec::SearchTvEpisTitle(vector<searchResultTvMovie> &resultSet, char delimiter) {
  string_view TVshowName;
  string_view episodeSearchString;
  if (!splitString(m_movieSearchString, delimiter, 4, TVshowName, episodeSearchString) ) return;
//  bool debug = TVshowName == "rose" && delimiter == '-';
  bool debug = false;
  size_t oldSize = resultSet.size();
  if (debug) esyslog("tvscraper: cSearchEventOrRec::SearchTvEpisTitle (1), oldSize %zu", oldSize);
  SearchTv(resultSet, TVshowName);
  if (debug) esyslog("tvscraper: cSearchEventOrRec::SearchTvEpisTitle (2), oldSize %zu, size %zu", oldSize, resultSet.size() );
  if (resultSet.size() > oldSize)
    for (vector<searchResultTvMovie>::iterator i = resultSet.begin() + oldSize; i != resultSet.end(); i++) {
//      if (debug) esyslog("tvscraper: cSearchEventOrRec::SearchTvEpisTitle set delim for %i", i->id() );
      i->setDelim(delimiter);
    }
  if (debug) esyslog("tvscraper: cSearchEventOrRec::SearchTvEpisTitle (3), oldSize %zu, size %zu", oldSize, resultSet.size() );
}

void cSearchEventOrRec::SearchMovie(vector<searchResultTvMovie> &resultSet) {
  extDbConnected = true;
  if (m_originalTitle.length() > 0) {
    m_moviedbScraper->AddMovieResults(resultSet, m_originalTitle, m_sEventOrRecording->Description(), m_years, NULL);
  } else {
    m_moviedbScraper->AddMovieResults(resultSet, m_movieSearchString, m_sEventOrRecording->Description(), m_years, m_sEventOrRecording->GetLanguage() );
  }
}

bool cSearchEventOrRec::CheckCache(sMovieOrTv &movieOrTv) {
  if (m_movieSearchString == m_TVshowSearchString) {
    if (!m_db->GetFromCache(m_TVshowSearchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText) ) return false;
  } else {
    bool movieFound = m_db->GetFromCache(m_movieSearchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText);
    movieFound = movieFound && (movieOrTv.type != scrapSeries || !movieOrTv.episodeSearchWithShorttext);
    if (!movieFound) {
      if (!m_db->GetFromCache(m_TVshowSearchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText) ) return false;
      if (movieOrTv.type == scrapMovie || movieOrTv.episodeSearchWithShorttext) return false;
    }
  }
  bool debug = movieOrTv.id == -315940 || movieOrTv.id == -342196;
  if (debug) esyslog("tvscraper: ERROR, movieOrTv.id == %i, m_TVshowSearchString %s", movieOrTv.id, m_TVshowSearchString.c_str() );
// cache found
  string episodeSearchString;
  if (movieOrTv.type != scrapNone && movieOrTv.id != 0) {
// something "real" was found
    if (Store(movieOrTv) == -1) {
      m_db->exec("DELETE FROM cache where movie_tv_id = ?", movieOrTv.id);
      return false;
    }
// do we have to search the episode?
    if (movieOrTv.season != -100 && movieOrTv.episodeSearchWithShorttext) {
// a TV show was found, and we have to use the subtitle to find the episode
// delete cached season / episode information: if we cannot get this inforamtion from short text -> no episode found
      movieOrTv.season = 0;
      movieOrTv.episode = 0;
      
      episodeSearchString = m_baseNameEquShortText?m_episodeName:m_sEventOrRecording->EpisodeSearchString();
      int min_distance = 2000;
      sMovieOrTv movieOrTv_best;
      const cLanguage *lang = m_sEventOrRecording->GetLanguage();
      for (int id: cSqlGetSimilarTvShows(m_db, movieOrTv.id))
      {
        sMovieOrTv movieOrTv2 = movieOrTv;
        movieOrTv2.id = id;
        if (id != movieOrTv.id) {
          if (Store(movieOrTv2) == -1) {
// object does not exist, remove from similar
// note: if the TV show is in tv2, > 0 will be returned, even if it does not exist in external db
            if (config.enableDebug)
              esyslog("tvscraper: ERROR, movieOrTv.id == %i, does not exist, will be deleted from tv_similar. m_TVshowSearchString %s", movieOrTv2.id, m_TVshowSearchString.c_str() );
            m_db->exec("DELETE FROM tv_similar where tv_id = ?", movieOrTv2.id);
            continue;
          }
        }
        if (UpdateEpisodeListIfRequired(id, lang) == -1) {
          if (id == movieOrTv.id) m_db->exec("DELETE FROM cache where movie_tv_id = ?", id);
          else m_db->exec("DELETE FROM tv_similar where tv_id = ?", id);
          continue;
        }
        int distance = cMovieOrTv::searchEpisode(m_db, movieOrTv2, episodeSearchString, m_baseNameOrTitle, m_years, lang);
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
      esyslog("tvscraper: found cache %s => scrapNone", m_TVshowSearchString.c_str());
      return true;
    case scrapMovie:
      esyslog("tvscraper: found movie cache %s => %i", m_movieSearchString.c_str(), movieOrTv.id);
      return true;
    case scrapSeries:
      esyslog("tvscraper: found %stv cache tv \"%s\", episode \"%s\" , id %i season %i episode %i", movieOrTv.id < 0?"TV":"", m_TVshowSearchString.c_str(), episodeSearchString.c_str(), movieOrTv.id, movieOrTv.season, movieOrTv.episode);
      return true;
    default:
      esyslog("tvscraper: ERROR found cache %s => unknown scrap type %i", m_TVshowSearchString.c_str(), movieOrTv.type);
      return true;
  }
}

void cSearchEventOrRec::ScrapAssign(const sMovieOrTv &movieOrTv) {
// assign found movieOrTv to event/recording in db
// and download images
  if(movieOrTv.type != scrapMovie && movieOrTv.type != scrapSeries) {
    m_db->DeleteEventOrRec(m_sEventOrRecording); // nothing was found, delete assignment
    return;
  }
  if (!m_sEventOrRecording->Recording() )
          m_db->InsertEvent     (m_sEventOrRecording, movieOrTv.id, movieOrTv.season, movieOrTv.episode);
     else m_db->InsertRecording2(m_sEventOrRecording, movieOrTv.id, movieOrTv.season, movieOrTv.episode);
}

int cSearchEventOrRec::UpdateEpisodeListIfRequired(int tvID, const cLanguage *lang) {
// return -1 if update is required, but TV show does not exist in external db
  int r = UpdateEpisodeListIfRequired_i(tvID);
  if (tvID > 0) return r;
  if (config.isDefaultLanguage(lang)) return r;
  if (config.enableDebug) esyslog("tvscraper: cSearchEventOrRec::UpdateEpisodeListIfRequired lang %s, tvID %i", lang->getNames().c_str(), tvID);
  m_tvdbScraper->StoreSeriesJson(tvID * (-1), lang);
  return r;
}

int cSearchEventOrRec::UpdateEpisodeListIfRequired_i(int tvID) {
// check: is update required?
// return -1 if update is required, but TV show does not exist in external db
// 0 otherwise
  string status;
  time_t lastUpdated = 0;
  time_t now = time(0);

  m_db->GetTv(tvID, lastUpdated, status);
  if (config.enableDebug && tvID == -232731) esyslog("tvscraper: cSearchEventOrRec::UpdateEpisodeListIfRequired, 1, tvID %i, now %li, lastUpdated %li, difftime %f, status %s", tvID, now, lastUpdated, difftime(now, lastUpdated), status.c_str() );

  if (lastUpdated) {
    if (difftime(now, lastUpdated) < 7.*24*60*60 ) return 0; // min one week between updates
    if (status.compare("Ended") == 0) return 0; // see https://thetvdb-api.readthedocs.io/api/series.html
    if (status.compare("Canceled") == 0) return 0;
// not documented for themoviedb, see https://developers.themoviedb.org/3/tv/get-tv-details . But test indicates the same values ("Ended" & "Canceled")...
  }
// update is required
  if (tvID > 0) {
    cMovieDbTv tv(m_db, m_moviedbScraper);
    tv.SetTvID(tvID);
    tv.UpdateDb(true);
  } else {
    if (m_tvdbScraper->StoreSeriesJson(tvID * (-1), true) == -1) return -1;
  }
  return 0;
}

void cSearchEventOrRec::getActorMatches(const char* actor, int &numMatchesAll, int &numMatchesFirst, int &numMatchesSure, cContainer &alreadyFound) {
// numMatchesAll:   complete actor match, and actor must contain a blank
// numMatchesSure:  match of last part of actor (blank delimited)
// numMatchesFirst: match of other parts of actor (blank delimited)
// note: too short parts of actor are ignored, can be articles, ...
  const char *description = m_sEventOrRecording->Description();
  if (!description || !actor) return;
  const char *pos_blank = strrchr(actor, ' ');
  if ( pos_blank && addActor(description, actor, 0, numMatchesAll,  alreadyFound)) return;
  if (!pos_blank && addActor(description, actor, 0, numMatchesSure, alreadyFound)) return;
  if (!pos_blank || pos_blank == actor) return;

// look for matches of part of the actor
  const char *lPos = actor;
  for (const char *rDelimPos; (rDelimPos = strchr(lPos, ' ')); lPos = rDelimPos + 1)
    addActor(description, lPos, rDelimPos - lPos, numMatchesFirst, alreadyFound);
  addActor(description, lPos, 0, numMatchesSure, alreadyFound);
}

void cSearchEventOrRec::getDirectorWriterMatches(const std::string_view &directorWriter, int &numMatchesAll, int &numMatchesSure, cContainer &alreadyFound) {
  if (directorWriter.length() < 3) return;
  const char *description = m_sEventOrRecording->Description();
  if (!description) return;
  if (addActor(description, directorWriter, numMatchesAll, alreadyFound)) return;
// complete name not found. Try last name
  size_t pos_blank = directorWriter.rfind(' ');
  if (pos_blank == std::string::npos || pos_blank == 0) return;
  addActor(description, directorWriter.substr(pos_blank + 1), numMatchesSure, alreadyFound);
}

bool cSearchEventOrRec::addActor(const char *description, const char *name, size_t len, int &numMatches, cContainer &alreadyFound) {
// search name in description, if found, increase numMatches. But only once for each name found
// return true if found. Always. Even if numMatches is not increased
  size_t len2 = (len == 0)?strlen(name):len;
  if (len2 < 3) return false; // ignore if name is too short
  if (strstr_word(description, name, len) == NULL) return false;   // name not found in description
  if (!alreadyFound.insert(string_view(name, len2))) numMatches++;
  return true;
}
bool cSearchEventOrRec::addActor(const char *description, const string_view &name, int &numMatches, cContainer &alreadyFound) {
// search name in description, if found, increase numMatches. But only once for each name found
// return true if found. Always. Even if numMatches is not increased
  if (name.length() < 3) return false; // ignore if name is too short
  if (strstr_word(description, name.data(), name.length() ) == NULL) return false;   // name not found in description
  if (!alreadyFound.insert(name)) numMatches++;
  return true;
}

void cSearchEventOrRec::getActorMatches(searchResultTvMovie &sR, cSql &actors) {
  int numMatchesAll = 0;
  int numMatchesFirst = 0;
  int numMatchesSureRole = 0;
  int numMatchesSureName = 0;
  cContainer alreadyFound;
// note: alreadyFound contains data from actors. So, actors must not change during this method
// also, avoid sql loop to get actors (as this will result in a change for the actors strings)
  bool debug = false;
//  debug = (sR.id() == 557) || (sR.id() == 558) || (sR.id() == 559);
//  debug = debug || (sR.id() == 5548) || (sR.id() == 5549) || (sR.id() == 5550); // RoboCop / Officer Alex J. Murphy or just RoboCop ...)
//  debug = debug || (sR.id() == -77973) || (sR.id() == 60293);
//  debug = false;
  if (debug) esyslog("tvscraper: getActorMatches, id: %i", sR.id() );
  for(cSql &actor: actors) {
// first col: actor_name
    getActorMatches(actor.getCharS(0), numMatchesAll, numMatchesFirst, numMatchesSureName, alreadyFound);
//    if (debug) esyslog("tvscraper: getActorMatches, Actor: %s, numMatchesAll %i, numMatchesFirst %i, numMatchesSureRole %i, numMatchesSureName %i", actor[1].c_str(), numMatchesAll, numMatchesFirst, numMatchesSureRole, numMatchesSureName);
// second col: actor_role
    getActorMatches(actor.getCharS(1), numMatchesAll, numMatchesFirst, numMatchesSureRole, alreadyFound);
//    if (debug) esyslog("tvscraper: getActorMatches, Actor: %s, numMatchesAll %i, numMatchesFirst %i, numMatchesSureRole %i, numMatchesSureName %i", actor[2].c_str(), numMatchesAll, numMatchesFirst, numMatchesSureRole, numMatchesSureName);
  }
  int sum = numMatchesFirst + numMatchesSureRole + 4*numMatchesSureName + 16*numMatchesAll;
  sR.setActors(sum);
  if (debug) esyslog("tvscraper: getActorMatches, sum %i, normMatch %f", sum, searchResultTvMovie::normMatch(sum/16.));
}

void cSearchEventOrRec::getDirectorWriterMatches(searchResultTvMovie &sR, const char *directors, const char *writers) {
  int numMatchesAll = 0;
  int numMatchesSure = 0;
  cContainer alreadyFound;
  for(std::string_view directorWriter: cSplit(directors, '|')) {
    getDirectorWriterMatches(directorWriter, numMatchesAll, numMatchesSure, alreadyFound);
  }
  for(std::string_view directorWriter: cSplit(writers, '|')) {
    getDirectorWriterMatches(directorWriter, numMatchesAll, numMatchesSure, alreadyFound);
  }
  sR.setDirectorWriter(numMatchesSure + 2*numMatchesAll);
}

bool cSearchEventOrRec::selectBestAndEnhanceIfRequired(std::vector<searchResultTvMovie>::iterator begin, std::vector<searchResultTvMovie>::iterator end, std::vector<searchResultTvMovie>::iterator &new_end, float minDiff, void (*func)(searchResultTvMovie &sR, cSearchEventOrRec &searchEventOrRec)) {
// return true if enhancement was required
// in this case, return the end of the enhance list in new_end
// minDiff must be > 0, otherwise an empty list my be returned
//  bool debug = m_TVshowSearchString == "james cameron's dark angel";
  bool debug = false;
  float minDiffSame = std::max(minDiff - 0.05, 0.01);
  float minDiffOther = minDiff;
  new_end = end;
  if (begin == end) return false; // empty list
  if (begin + 1 == end) { func(*begin, *this); return false;} // list with one element, always enhance first (currently best) result
  std::sort(begin, end);
  float bestMatch = begin->getMatch();
  bool bestMatchMovie = begin->movie();
  std::vector<searchResultTvMovie>::iterator i;
  float diffSame = 2.;
  float diffALt  = 2.;
  for (i = begin + 1; i != end; i++) if (i->movie() != bestMatchMovie) { diffALt  = bestMatch - i->getMatch(); break; }
  for (i = begin + 1; i != end; i++) if (i->movie() == bestMatchMovie) { diffSame = bestMatch - i->getMatch(); break; }
  if (debug) esyslog("tvscraper: selectBestAndEnhanceIfRequired, minDiffSame %f, minDiffOther %f, diffALt %f, diffSame %f", minDiffSame, minDiffOther, diffALt, diffSame);
  if (diffSame > minDiffSame && diffALt > minDiffOther) { func(*begin, *this); return false;}
  bool matchAlt = !(diffALt <= diffALt); // set this to false, if still a match to an "alternative" finding is required
  for (i = begin; i != end; i++) {
//    debug = debug || i->id() == -353808 || i->id() == -250822 || i->id() == 440757;
    float match = i->getMatch();
    if (debug) esyslog("tvscraper: selectBestAndEnhanceIfRequired(1), id = %i match = %f", i->id(), match );
    if (match < 0.3) break; // most likely not the match you are looking for, so no more work on this ...
    if (matchAlt && (bestMatch - match) > minDiffSame) break;   // best match is minDiffSame better compared to this
    if (debug) esyslog("tvscraper: selectBestAndEnhanceIfRequired(2), id = %i", i->id() );
    if (i->movie() != bestMatchMovie) matchAlt = true;
    func(*i, *this);
    if (debug) esyslog("tvscraper: selectBestAndEnhanceIfRequired(3), id = %i", i->id() );
  }
  if (debug) esyslog("tvscraper: selectBestAndEnhanceIfRequired(4), searchstring = %s", m_TVshowSearchString.c_str() );
  std::sort(begin, i);
  if (debug) esyslog("tvscraper: selectBestAndEnhanceIfRequired(5)" );
  new_end = i;
  if (i == begin) func(*begin, *this);
  return true;
}

void cSearchEventOrRec::enhance1(searchResultTvMovie &sR, cSearchEventOrRec &searchEventOrRec) {
// add all information which is in database cache
// this is all except the episode list
/*
  bool debug = sR.id() == -353808 || sR.id() == -250822 || sR.id() == 440757 || sR.id() == -258828 || sR.id() == -412485 || sR.id() == -295683;
  debug = debug || searchEventOrRec.m_TVshowSearchString == "die göttliche sophie";
  debug = debug || searchEventOrRec.m_TVshowSearchString == "rose - königin der blumen";
  debug = debug || searchEventOrRec.m_TVshowSearchString == "sendung vom 19.02. 09:30 uhr";
  debug = debug || searchEventOrRec.m_TVshowSearchString == "sendung vom 19.02. 09";
  debug = debug || searchEventOrRec.m_TVshowSearchString == "exploding sun 1";
  debug = debug || searchEventOrRec.m_TVshowSearchString == "exploding sun 2";
  debug = debug || searchEventOrRec.m_TVshowSearchString == "stargate";
*/
  bool debug = false;
  if (debug) sR.log(searchEventOrRec.m_TVshowSearchString.c_str() );
  cSql actors(searchEventOrRec.m_db);
  if (sR.movie() ) {
// movie
    sR.setDuration(searchEventOrRec.m_sEventOrRecording->DurationDistance(searchEventOrRec.m_moviedbScraper->GetMovieRuntime(sR.id() )) );
    sR.updateMatchText(sentence_distance(searchEventOrRec.m_db->GetMovieTagline(sR.id() ), searchEventOrRec.m_movieSearchString));
    actors.prepareBindStep("select actor_name, actor_role from actors, actor_movie where actor_movie.actor_id = actors.actor_id and actor_movie.movie_id = ?", sR.id());
    const char *sql = "select movie_director, movie_writer from movie_runtime2 where movie_id = ?";
    cSql sqlI(searchEventOrRec.m_db, sql, sR.id());
    if (sqlI.readRow()) {
      searchEventOrRec.getDirectorWriterMatches(sR, sqlI.getCharS(0), sqlI.getCharS(1));
    }
  } else {
// tv show
    if (debug) esyslog("tvscraper: enhance1 (1)" );
    sR.setDuration(searchEventOrRec.GetTvDurationDistance(sR.id() ) );
    if (debug) esyslog("tvscraper: enhance1 (2)" );
    sR.setDirectorWriter(0);
    if (sR.id() < 0) {
// tv show from thetvdb
/*
      float voteAverage;
      int voteCount;
      searchEventOrRec.m_tvdbScraper->GetTvVote(sR.id() * -1, voteAverage, voteCount);
*/
      int score = searchEventOrRec.m_tvdbScraper->GetTvScore(sR.id() * -1);
      sR.setScore(score);
      if (debug) esyslog("tvscraper: enhance1 (4)" );
      if (debug) sR.log(searchEventOrRec.m_TVshowSearchString.c_str() );
      actors.prepareBindStep("SELECT actor_name, actor_role FROM series_actors WHERE actor_series_id = ?", sR.id() * (-1));
      if (debug) esyslog("tvscraper: enhance1 (5)" );
      if (debug) sR.log(searchEventOrRec.m_TVshowSearchString.c_str() );
    } else {
// tv show from themoviedb
      actors.prepareBindStep("select actor_name, actor_role from actors, actor_tv where actor_tv.actor_id = actors.actor_id and actor_tv.tv_id = ?", sR.id());
    }
  }
  if (debug) esyslog("tvscraper: enhance1 (6)" );
  if (debug) sR.log(searchEventOrRec.m_TVshowSearchString.c_str() );
  searchEventOrRec.getActorMatches(sR, actors);
  if (debug) sR.log(searchEventOrRec.m_TVshowSearchString.c_str() );
}
void cSearchEventOrRec::enhance2(searchResultTvMovie &searchResult, cSearchEventOrRec &searchEventOrRec) {
  bool debug = searchResult.id() == -353808 || searchResult.id() == -250822 || searchResult.id() == 440757;
  debug = false;
  if (debug) esyslog("tvscraper: enhance2 (1)" );

  if (searchResult.movie() ) return;
//  if (searchResult.movie() ) { searchResult.setMatchEpisode(1000); return; }
//  Transporter - The Mission: Is a movie, and we should not give negative point for having no episode match
  string_view foundName;
  string_view episodeSearchString;
  sMovieOrTv movieOrTv;
  movieOrTv.id = searchResult.id();
  movieOrTv.type = scrapSeries;
  searchEventOrRec.Store(movieOrTv);
  if (debug) esyslog("tvscraper: enhance2 (3)" );
// search episode
  movieOrTv.episodeSearchWithShorttext = searchResult.delim()?0:1;
  if (movieOrTv.episodeSearchWithShorttext) episodeSearchString = searchEventOrRec.m_baseNameEquShortText?searchEventOrRec.m_episodeName:searchEventOrRec.m_sEventOrRecording->EpisodeSearchString();
  else splitString(searchEventOrRec.m_movieSearchString, searchResult.delim(), 4, foundName, episodeSearchString);
  const cLanguage *lang = searchEventOrRec.m_sEventOrRecording->GetLanguage();
  searchEventOrRec.UpdateEpisodeListIfRequired(movieOrTv.id, lang);
  int distance = cMovieOrTv::searchEpisode(searchEventOrRec.m_db, movieOrTv, episodeSearchString, searchEventOrRec.m_baseNameOrTitle, searchEventOrRec.m_years, lang);
  searchEventOrRec.m_episodeFound = distance != 1000;
  if (searchEventOrRec.m_episodeFound) distance /= 2; // reduce the distance here, found episode is always positive
  searchResult.setMatchEpisode(distance);
}

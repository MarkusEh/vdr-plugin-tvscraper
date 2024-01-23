#include "searchEventOrRec.h"

cSearchEventOrRec::cSearchEventOrRec(csEventOrRecording *sEventOrRecording, cOverRides *overrides, cMovieDbMovieScraper *movieDbMovieScraper, cMovieDbTvScraper *movieDbTvScraper, cTvDbTvScraper *tvDbTvScraper, cTVScraperDB *db):
  m_sEventOrRecording(sEventOrRecording),
  m_overrides(overrides),
  m_movieDbMovieScraper(movieDbMovieScraper),
  m_movieDbTvScraper(movieDbTvScraper),
  m_tvDbTvScraper(tvDbTvScraper),
  m_db(db),
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
    m_num_parts = Number2InLastPartWithP(m_movieSearchString);
    initSearchString(m_TVshowSearchString);
    initSearchString(m_movieSearchString);
    m_sEventOrRecording->AddYears(m_years);
    m_years.addYears((const char *)m_baseName);
    m_years.finalize();
  }

void cSearchEventOrRec::initOriginalTitle() {
  m_originalTitle = textAttributeValue(m_sEventOrRecording->Description(), "Originaltitel: ");
//  if (m_originalTitle.length() > 0 && config.enableDebug) esyslog("tvscraper, original title = \"%.*s\"", static_cast<int>(m_originalTitle.length()), m_originalTitle.data() );
}

bool cSearchEventOrRec::isTitlePartOfPathName(size_t baseNameLen) {
  return cNormedString(m_sEventOrRecording->Recording()->Info()->Title()).minDistanceStrings(m_sEventOrRecording->Recording()->Name()) < 600;
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
  cNormedString nsBaseNameOrTitle(m_baseNameOrTitle);
  int distTitle     = nsBaseNameOrTitle.sentence_distance(recording->Info()->Title() );
  int distShortText = nsBaseNameOrTitle.sentence_distance(shortText);
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

bool cSearchEventOrRec::isVdrDate(cSv baseName) {
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

bool cSearchEventOrRec::isVdrDate2(cSv baseName) {
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
  searchString = std::string(cToSvToLower(searchString, g_locale));
}

cMovieOrTv *cSearchEventOrRec::Scrape(int &statistics, cSv channelName) {
// return NULL if no movie/tv was assigned to this event/recording
// extDbConnected: true, if request to rate limited internet db was required. Otherwise, false
// statistics:
// 0: did nothing, overrides
// 1: cache hit
// 11: external db
  statistics = 0;
  if (m_overrides->Ignore(m_baseNameOrTitle)) return NULL;
  sMovieOrTv movieOrTv;
  statistics = ScrapFindAndStore(movieOrTv, channelName);
  if(movieOrTv.type != scrapMovie && movieOrTv.type != scrapSeries) {
// nothing found, try again with title
    if (m_baseNameOrTitle != m_sEventOrRecording->Title() ) {
      m_baseNameOrTitle = m_sEventOrRecording->Title();
      m_TVshowSearchString = m_baseNameOrTitle;
      m_movieSearchString = m_baseNameOrTitle;
      m_baseNameEquShortText = false;
      m_episodeName = "";
      initSearchString(m_TVshowSearchString);
      initSearchString(m_movieSearchString);
      statistics = ScrapFindAndStore(movieOrTv, channelName);
    }
  }
  ScrapAssign(movieOrTv);
  return cMovieOrTv::getMovieOrTv(m_db, movieOrTv);
}

bool cSearchEventOrRec::ScrapCheckOverride(sMovieOrTv &movieOrTv, cSv channelName) {
  movieOrTv.season = 0;
  movieOrTv.year = 0;
  
  cSv title = removeLastPartWithP(m_sEventOrRecording->Title() );
  int o_id = m_overrides->TheTVDB_SeriesID(title);
  if (o_id != 0) {
    movieOrTv.type = scrapSeries;
    movieOrTv.id = -o_id;
    movieOrTv.episodeSearchWithShorttext = 1;
    cMovieOrTv::searchEpisode(m_db, movieOrTv, getExtMovieTvDb(movieOrTv), m_sEventOrRecording->ShortText(), m_baseNameOrTitle, m_years, m_sEventOrRecording->GetLanguage(), m_sEventOrRecording->ShortText(), m_sEventOrRecording->Description());
    return true;
  }
  o_id = m_overrides->TheMovieDB_SeriesID(title);
  if (o_id != 0) {
    movieOrTv.type = scrapSeries;
    movieOrTv.id = o_id;
    movieOrTv.episodeSearchWithShorttext = 1;
    cMovieOrTv::searchEpisode(m_db, movieOrTv, getExtMovieTvDb(movieOrTv), m_sEventOrRecording->ShortText(), m_baseNameOrTitle, m_years, m_sEventOrRecording->GetLanguage(), m_sEventOrRecording->ShortText(), m_sEventOrRecording->Description());
    return true;
  }
  o_id = m_overrides->TheMovieDB_MovieID(title);
  if (o_id != 0) {
    movieOrTv.type = scrapMovie;
    movieOrTv.id = o_id;
    movieOrTv.season = -100;
    return true;
  }
  int dbid, season, episode;
  bool is_movie;
  std::string episodeName;
  eMatchPurpouse matchPurpouse;
  if (m_overrides->regex(m_sEventOrRecording->Title(), m_sEventOrRecording->ShortText(), m_sEventOrRecording->Description(), channelName, matchPurpouse, dbid, is_movie, season, episode, episodeName)) {
    movieOrTv.id = dbid;
    if (is_movie) {
      movieOrTv.type = scrapMovie;
      movieOrTv.season = -100;
      return true;
    }
    movieOrTv.type = scrapSeries;
    switch (matchPurpouse) {
      case eMatchPurpouse::regexTitleChannel_id:
        movieOrTv.episodeSearchWithShorttext = 1;
        cMovieOrTv::searchEpisode(m_db, movieOrTv, getExtMovieTvDb(movieOrTv), m_sEventOrRecording->ShortText(), m_baseNameOrTitle, m_years, m_sEventOrRecording->GetLanguage(), m_sEventOrRecording->ShortText(), m_sEventOrRecording->Description());
        if (config.enableDebug)
          esyslog("tvscraper: overrides: title \"%s\", dbid %d", m_sEventOrRecording->Title(), dbid);
        return true;
      case eMatchPurpouse::regexTitleChannel_idEpisodeName:
        movieOrTv.episodeSearchWithShorttext = 0;
        cMovieOrTv::searchEpisode(m_db, movieOrTv, getExtMovieTvDb(movieOrTv), episodeName, "", m_years, m_sEventOrRecording->GetLanguage(), nullptr, nullptr);
        if (config.enableDebug)
          esyslog("tvscraper: overrides: title \"%s\", episode \"%s\", dbid %d", m_sEventOrRecording->Title(), episodeName.c_str(), dbid);
        return true;
      case eMatchPurpouse::regexTitleShortTextChannel_idSeasonNumberEpisodeNumber:
        movieOrTv.episodeSearchWithShorttext = 0;
        movieOrTv.season = season;
        movieOrTv.episode = episode;
        if (config.enableDebug)
          esyslog("tvscraper: overrides: title \"%s\", dbid %d, season %i, episode %i", m_sEventOrRecording->Title(), dbid, season, episode);
        return true;
      case eMatchPurpouse::regexTitleShortTextChannel_seasonNumberEpisodeNumber:
        m_matchPurpouse = matchPurpouse;
        m_season = season;
        m_episode = episode;
        if (config.enableDebug) {
          const char *st = m_sEventOrRecording->ShortText();
          if (!st) st = m_sEventOrRecording->Description();
          if (!st) st = "ERROR: no short text / description";
          esyslog("tvscraper: overrides: title \"%s\", short text \"%s\", season %i, episode %i", m_sEventOrRecording->Title(), st, season, episode);
        }
        return false;  // we still need to search for the series ID
// TODO case eMatchPurpouse::regexTitleChannel_idEpisodeAbsolutNumber
      default:
        esyslog("tvscraper, ERROR in cSearchEventOrRec::ScrapCheckOverride, matchPurpouse %d unknown", (int)matchPurpouse);
    }
  }
  return false;
}
int cSearchEventOrRec::ScrapFindAndStore(sMovieOrTv &movieOrTv, cSv channelName) {
// 1: cache hit
// 11: external db
  if (ScrapCheckOverride(movieOrTv, channelName) ) {
    if (Store(movieOrTv) != -1) return 1;
    esyslog("tvscraper: ERROR movie/tv id %d given in override.conf does not exist", movieOrTv.id);
  }
  if (CheckCache(movieOrTv) ) return 1;
  if (config.enableDebug) {
    esyslog("tvscraper: scraping movie \"%s\", TV \"%s\", title \"%s\", orig. title \"%.*s\", start time: %s", m_movieSearchString.c_str(), m_TVshowSearchString.c_str(), m_sEventOrRecording->Title(), static_cast<int>(m_originalTitle.length()), m_originalTitle.data(), cToSvDateTime("%Y-%m-%d %H:%M:%S", m_sEventOrRecording->StartTime() ).c_str() );
  }
  vector<searchResultTvMovie> searchResults;
  cSv episodeSearchString;
  cSv foundName;
  scrapType sType = ScrapFind(searchResults, foundName, episodeSearchString);
  if (searchResults.size() == 0) sType = scrapNone;
  movieOrTv.type = sType;
  if (sType == scrapNone) {
// nothing found
    movieOrTv.id = 0;
    if (config.enableDebug) {
      esyslog("tvscraper: nothing found for movie \"%s\" TV \"%s\"", m_movieSearchString.c_str(), m_TVshowSearchString.c_str() );
      if (searchResults.size() > 0 ) log(searchResults[0]);
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
      movieOrTv.episodeSearchWithShorttext = episodeSearchString.empty()?1:0;
      if (movieOrTv.episodeSearchWithShorttext == 1) {
// pattern in title: TV show name
//    in short text: episode name
        episodeSearchString = m_baseNameEquShortText?m_episodeName:m_sEventOrRecording->EpisodeSearchString();
        if (searchResults[0].getMatchEpisode() < 1000) {
          if (searchResults[0].getMatchEpisode() < 340) movieOrTv.episodeSearchWithShorttext = 3;
// we request an episode match of the cached data only if there was a text match.
// a number after the text is just too week, and if there was a year match this is reflected / checked in the cache anyway
// note: we use episiode distance / 2, so distance <= 325 (roughly) is required for text match
          float bestMatchText = searchResults[0].getMatchText();
          std::string tv_name;
          for (const searchResultTvMovie &searchResult: searchResults) {
            if (searchResult.id() == searchResults[0].id() ) continue;
            if ((searchResult.id()^searchResults[0].id()) < 0) continue; // we look for IDs in same database-> same sign
            if (searchResult.movie()) continue;  // we only look for TV shows
            if (abs(bestMatchText - searchResult.getMatchText()) > 0.001) continue;   // we only look for matches similar near
            if (searchResult.m_normedName.sentence_distance(searchResults[0].m_normedName) > 200) continue;
            m_db->setSimilar(searchResults[0].id(), searchResult.id() );
            if (config.enableDebug) esyslog("tvscraper: setSimilar %i and %i", searchResults[0].id(), searchResult.id());
          }
        }
        if (m_matchPurpouse == eMatchPurpouse::regexTitleShortTextChannel_seasonNumberEpisodeNumber) {
          movieOrTv.season = m_season;
          movieOrTv.episode = m_episode;
        } else {
          cMovieOrTv::searchEpisode(m_db, movieOrTv, getExtMovieTvDb(movieOrTv), episodeSearchString, m_baseNameOrTitle, m_years, lang, m_sEventOrRecording->ShortText(), m_sEventOrRecording->Description());
        }

        m_db->InsertCache(m_TVshowSearchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText);
      } else {
// pattern in title: TV show name - episode name
        cMovieOrTv::searchEpisode(m_db, movieOrTv, getExtMovieTvDb(movieOrTv), episodeSearchString, "", m_years, lang, nullptr, nullptr);
        m_db->InsertCache(m_movieSearchString, m_sEventOrRecording, movieOrTv, m_baseNameEquShortText);
      }
      if (config.enableDebug) esyslog("tvscraper: %stv \"%.*s\", episode \"%.*s\" successfully scraped, id %i season %i episode %i", searchResults[0].id() > 0?"":"TV", static_cast<int>(foundName.length()), foundName.data(), static_cast<int>(episodeSearchString.length()), episodeSearchString.data(), movieOrTv.id, movieOrTv.season, movieOrTv.episode);
    }
    if (config.enableDebug) {
      log(searchResults[0], foundName);
      if (searchResults.size() > 1 ) log(searchResults[1]);
    }
  }
  return 11;
}

int cSearchEventOrRec::Store(const sMovieOrTv &movieOrTv) {
// return -1 if object does not exist in external db
// check if already in internal DB. If not: Download from external db
  iExtMovieTvDb *extMovieTvDb = getExtMovieTvDb(movieOrTv);
  if (extMovieTvDb) return extMovieTvDb->download(std::abs(movieOrTv.id));
  return 0; // scrapNone, do nothing, nothing to store
}

// required for older vdr versions, to make swap used by sort unambigous
inline void swap(searchResultTvMovie &a, searchResultTvMovie &b) {
 std::swap(a, b);
}
void cSearchEventOrRec::log(const searchResultTvMovie &searchResult, cSv foundName) {
  if (foundName.empty()) {
    if (searchResult.movie() ) foundName = m_movieSearchString;
    else {
      if (searchResult.delim() == 0 || searchResult.delim() == 's') foundName = m_TVshowSearchString;
      else {
        cSv episodeSearchString;
        splitString(m_movieSearchString, searchResult.delim(), 4, foundName, episodeSearchString);
      }
    }
  }
  if (searchResult.delim() == 's') {
    searchResult.log(cToSvConcat(foundName, " ", m_sEventOrRecording->ShortText()) );
  } else {
    searchResult.log(foundName);
  }
}
scrapType cSearchEventOrRec::ScrapFind(vector<searchResultTvMovie> &searchResults, cSv &foundName, cSv &episodeSearchString) {
//  bool debug = m_TVshowSearchString == "james cameron's dark angel";
  bool debug = false;
  foundName = cSv();
  episodeSearchString = cSv();
  SearchNew(searchResults);
  if (searchResults.size() == 0) return scrapNone; // nothing found
// something was found. Add all information which is available for free
  if (m_baseNameEquShortText) for (searchResultTvMovie &searchResult: searchResults) if (!searchResult.movie() && searchResult.delim() == 0) searchResult.setBaseNameEquShortText();
  for (searchResultTvMovie &searchResult: searchResults) searchResult.setMatchYear(m_years, m_sEventOrRecording->DurationInSec() );
  std::sort(searchResults.begin(), searchResults.end() );
  if (debug) for (searchResultTvMovie &searchResult: searchResults) log(searchResult);
  const float minMatchPre = 0.4;
  std::vector<searchResultTvMovie>::iterator end = searchResults.begin();
  for (; end != searchResults.end(); ++end) if (end->getMatch() < minMatchPre) break;
  if (searchResults.begin() == end) {
    if (end->getMatch() < 0.3) return scrapNone; // nothing good enough found
    ++end; // enhance best match, so we can find at least one
  }
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
  if (searchResults[0].getMatch() < config.minMatchFinal) return scrapNone; // nothing good enough found
  if (searchResults[0].movie() ) { foundName = m_movieSearchString; return scrapMovie;}
  int n_tv_id = m_db->getNormedTvId(searchResults[0].id());
  if (searchResults[0].id() != n_tv_id) {
    for (auto &i :searchResults) {
      if (i.id() == n_tv_id) {
        if (config.enableDebug) {
          isyslog("tvscraper: use tv ID %d instead of tv ID %d", n_tv_id, searchResults[0].id() );
          log(searchResults[0]);
        }
        std::swap(searchResults[0], i);
        break;
      }
    }
  }
  if (searchResults[0].delim() ) splitString(m_movieSearchString, searchResults[0].delim(), 4, foundName, episodeSearchString);
  else foundName = m_TVshowSearchString;
  return scrapSeries;
}

int cSearchEventOrRec::GetTvDurationDistance(int tvID) {
  int finalDurationDistance = -1; // default, no data available
  cSql stmt(m_db, "SELECT episode_run_time FROM tv_episode_run_time WHERE tv_id = ?", tvID);
  if (!stmt.readRow() ) return finalDurationDistance;
// Done / checked: also write episode runtimes to episode_run_time, for each episode
  for (cSql &stm: stmt) {
    int rt = stm.getInt(0);
    if (rt < 1) continue; // ignore 0 and -1: -1-> no value in ext. db. 0-> value 0 in ext. db
    int durationDistance = m_sEventOrRecording->DurationDistance(rt);
    if (finalDurationDistance == -1 || durationDistance < finalDurationDistance) finalDurationDistance = durationDistance;
  }
  return finalDurationDistance;
}

void cSearchEventOrRec::SearchNew(vector<searchResultTvMovie> &resultSet) {
  extDbConnected = true;
  scrapType type_override = m_overrides->Type(m_baseNameOrTitle);
  if (m_matchPurpouse == eMatchPurpouse::regexTitleShortTextChannel_seasonNumberEpisodeNumber) type_override = scrapSeries;
  if (!m_originalTitle.empty() ) {
    cCompareStrings compareStrings(m_originalTitle);
    if (type_override != scrapSeries) addSearchResults(m_movieDbMovieScraper, resultSet, m_originalTitle, compareStrings, nullptr);
    if (m_originalTitle == m_movieSearchString && m_matchPurpouse != eMatchPurpouse::regexTitleShortTextChannel_seasonNumberEpisodeNumber) {
      compareStrings.add(m_originalTitle, ':');
      compareStrings.add(m_originalTitle, '-');
    }
    if (type_override != scrapMovie) addSearchResults(m_tvDbTvScraper, resultSet, m_originalTitle, compareStrings, nullptr);
    if (type_override != scrapMovie) addSearchResults(m_movieDbTvScraper, resultSet, m_originalTitle, compareStrings, nullptr);
  }
  const cLanguage *lang = m_sEventOrRecording->GetLanguage();
  if (m_TVshowSearchString != m_movieSearchString && m_TVshowSearchString != m_originalTitle) {
    cCompareStrings compareStrings(m_TVshowSearchString);
    if (type_override != scrapMovie) addSearchResults(m_tvDbTvScraper, resultSet, m_TVshowSearchString, compareStrings, lang);
    if (type_override != scrapMovie) addSearchResults(m_movieDbTvScraper, resultSet, m_TVshowSearchString, compareStrings, lang);
  }
  if (m_movieSearchString != m_originalTitle) {
    cCompareStrings compareStrings(m_movieSearchString, m_sEventOrRecording->ShortText() );
    if (type_override != scrapSeries) addSearchResults(m_movieDbMovieScraper, resultSet, m_movieSearchString, compareStrings, lang);
    if (m_matchPurpouse != eMatchPurpouse::regexTitleShortTextChannel_seasonNumberEpisodeNumber) {
      compareStrings.add(m_movieSearchString, ':');
      compareStrings.add(m_movieSearchString, '-');
    }
    if (type_override != scrapMovie) addSearchResults(m_tvDbTvScraper, resultSet, m_movieSearchString, compareStrings, lang);
    if (type_override != scrapMovie) addSearchResults(m_movieDbTvScraper, resultSet, m_movieSearchString, compareStrings, lang);
  }
}
bool cSearchEventOrRec::addSearchResults(iExtMovieTvDb *extMovieTvDb, vector<searchResultTvMovie> &resultSet, cSv searchString, const cCompareStrings &compareStrings, const cLanguage *lang) {
// return true if something was added
// modify searchString so that the external db will find something
// call the external db with the modified string
// note: compareStrings will be used on the found results, to figure out how good they match
  CONVERT(SearchString_rom, searchString, removeRomanNumC);
  cSv searchString_f = strlen(SearchString_rom) > 6?SearchString_rom:searchString;
  cSv searchString1 = SecondPart(searchString_f, "'s ", 6);
  size_t size0 = resultSet.size();
  if (!searchString1.empty()) {
    extMovieTvDb->addSearchResults(resultSet, searchString1, false, compareStrings, m_sEventOrRecording->ShortText(), m_sEventOrRecording->Description(), m_years, lang);
    if (resultSet.size() > size0) return true;
  }
  extMovieTvDb->addSearchResults(resultSet, searchString_f, true, compareStrings, m_sEventOrRecording->ShortText(), m_sEventOrRecording->Description(), m_years, lang);
  if (resultSet.size() > size0) {
    cNormedString normedSearchString(searchString);
    for (size_t i = size0; i < resultSet.size(); i++)
      if (normedSearchString.sentence_distance(resultSet[i].m_normedName) < 600) return true;
  }
  cSv searchString2;
  if (splitString(searchString_f, ": ", 3, searchString1, searchString2) ) {
    if (searchString1.length() >= 5) extMovieTvDb->addSearchResults(resultSet, searchString1, false, compareStrings, m_sEventOrRecording->ShortText(), m_sEventOrRecording->Description(), m_years, lang);
    if (searchString2.length() >= 6) extMovieTvDb->addSearchResults(resultSet, searchString2, false, compareStrings, m_sEventOrRecording->ShortText(), m_sEventOrRecording->Description(), m_years, lang);
  }
  if (splitString(searchString_f, " - ", 3, searchString1, searchString2)) {
    if (searchString1.length() >= 6) extMovieTvDb->addSearchResults(resultSet, searchString1, false, compareStrings, m_sEventOrRecording->ShortText(), m_sEventOrRecording->Description(), m_years, lang);
    if (searchString2.length() >= 6) extMovieTvDb->addSearchResults(resultSet, searchString2, false, compareStrings, m_sEventOrRecording->ShortText(), m_sEventOrRecording->Description(), m_years, lang);
  }
  return resultSet.size() > size0;
}

bool cSearchEventOrRec::CheckCache(sMovieOrTv &movieOrTv) {
// return false if no usable cache entry was found
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
  std::string episodeSearchString;
  if (movieOrTv.type != scrapNone && movieOrTv.id != 0) {
// something "real" was found
// do we have to search the episode?
    if (movieOrTv.season != -100 && movieOrTv.episodeSearchWithShorttext) {
// a TV show was found, and we have to use the subtitle to find the episode
      if (movieOrTv.episodeSearchWithShorttext != 3 && m_matchPurpouse == eMatchPurpouse::regexTitleShortTextChannel_seasonNumberEpisodeNumber) {
        movieOrTv.season = m_season;
        movieOrTv.episode = m_episode;
      } else {
      m_db->m_cache_episode_search_time.start();
// delete cached season / episode information: if we cannot get this information from short text -> no episode found
      movieOrTv.season = 0;
      movieOrTv.episode = 0;
      
      episodeSearchString = m_baseNameEquShortText?m_episodeName:m_sEventOrRecording->EpisodeSearchString();
      int min_distance = 1000;
      sMovieOrTv movieOrTv_best;
      const cLanguage *lang = m_sEventOrRecording->GetLanguage();
      for (int id: cSqlGetSimilarTvShows(m_db, movieOrTv.id))
      {
        sMovieOrTv movieOrTv2 = movieOrTv;
        movieOrTv2.id = id;
        int distance = cMovieOrTv::searchEpisode(m_db, movieOrTv2, getExtMovieTvDb(movieOrTv2), episodeSearchString, m_baseNameOrTitle, m_years, lang, m_sEventOrRecording->ShortText(), m_sEventOrRecording->Description() );
        if (distance < min_distance) {
          min_distance = distance;
          movieOrTv_best = movieOrTv2;
        }
      }
      m_db->m_cache_episode_search_time.stop();
      if (movieOrTv.episodeSearchWithShorttext == 3 && min_distance > 650) return false;
// episode match required for cache, but not found (ignore coincidence matches with distance > 650)

      if (m_matchPurpouse == eMatchPurpouse::regexTitleShortTextChannel_seasonNumberEpisodeNumber) {
        movieOrTv.season = m_season;
        movieOrTv.episode = m_episode;
      } else if (min_distance < 1000) movieOrTv = movieOrTv_best; // otherwise, there was no episode match

// was (min_distance < 950). But, this results in episode = 0 and season = 0.
// so, better to use the episode / season found, even if it is a very weak match.
// also, we must do it here the same way we do it in ScrapFindAndStore(),
// where we also have no min_distance for episode match
    }
    }
    if (Store(movieOrTv) == -1) return false;
  }
/*
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
*/
  return true;
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

void cSearchEventOrRec::getActorMatches(const char* actor, int &numMatchesAll, int &numMatchesFirst, int &numMatchesSure, cContainer &alreadyFound) {
// search name in Description & ShortText, if found, increase numMatches*. But only once for each name found
// return true if found. Always. Even if numMatches is not increased

// numMatchesAll:   complete actor match, and actor must contain a blank
// numMatchesSure:  match of last part of actor (blank delimited)
// numMatchesFirst: match of other parts of actor (blank delimited)
// note: too short parts of actor are ignored, can be articles, ...
  if (!actor) return;
  const char *pos_blank = strrchr(actor, ' ');
  if ( pos_blank && addActor(actor, numMatchesAll,  alreadyFound)) return;
  if (!pos_blank && addActor(actor, numMatchesSure, alreadyFound)) return;
  if (!pos_blank || pos_blank == actor) return;

// look for matches of part of the actor
  const char *lPos = actor;
  for (const char *rDelimPos; (rDelimPos = strchr(lPos, ' ')); lPos = rDelimPos + 1)
    addActor(cSv(lPos, rDelimPos - lPos), numMatchesFirst, alreadyFound);
  addActor(lPos, numMatchesSure, alreadyFound);
}

void cSearchEventOrRec::getDirectorWriterMatches(cSv directorWriter, int &numMatchesAll, int &numMatchesSure, cContainer &alreadyFound) {
  if (directorWriter.length() < 3) return;
  if (addActor(directorWriter, numMatchesAll, alreadyFound)) return;
// complete name not found. Try last name
  size_t pos_blank = directorWriter.rfind(' ');
  if (pos_blank == std::string::npos || pos_blank == 0) return;
  addActor(directorWriter.substr(pos_blank + 1), numMatchesSure, alreadyFound);
}

bool cSearchEventOrRec::addActor(cSv name, int &numMatches, cContainer &alreadyFound) {
// search name in Description & ShortText, if found, increase numMatches. But only once for each name found
// return true if found. Always. Even if numMatches is not increased
  if (name.length() < 3) return false; // ignore if name is too short
  if (const_ignoreWords.find(name) != const_ignoreWords.end() ) return false;
  if (strstr_word(m_sEventOrRecording->Description(), name.data(), name.length() ) == NULL &&
      strstr_word(m_sEventOrRecording->ShortText()  , name.data(), name.length() ) == NULL) return false;   // name not found in text
  if (!alreadyFound.insert(name)) ++numMatches;
  return true;
}

void cSearchEventOrRec::getActorMatches(searchResultTvMovie &sR, cSql &actors) {
  int numMatchesAll = 0;
  int numMatchesFirst = 0;
  int numMatchesSureRole = 0;
  int numMatchesSureName = 0;
  cContainer alreadyFound;
  bool debug = false;
//  debug = (sR.id() == 557) || (sR.id() == 558) || (sR.id() == 559);
//  debug = debug || (sR.id() == 5548) || (sR.id() == 5549) || (sR.id() == 5550); // RoboCop / Officer Alex J. Murphy or just RoboCop ...)
//  debug = debug || (sR.id() == 682) || (sR.id() == 37412);
  if (debug) esyslog("tvscraper: getActorMatches, id: %i", sR.id() );
  for(cSql &actor: actors) {
// first col: actor_name
    getActorMatches(actor.getCharS(0), numMatchesAll, numMatchesFirst, numMatchesSureName, alreadyFound);
    if (debug) esyslog("tvscraper: getActorMatches name, Actor: %s, numMatchesAll %i, numMatchesFirst %i, numMatchesSureRole %i, numMatchesSureName %i", actor.getCharS(0), numMatchesAll, numMatchesFirst, numMatchesSureRole, numMatchesSureName);
// second col: actor_role
    getActorMatches(actor.getCharS(1), numMatchesAll, numMatchesFirst, numMatchesSureRole, alreadyFound);
    if (debug) esyslog("tvscraper: getActorMatches role, Actor: %s, numMatchesAll %i, numMatchesFirst %i, numMatchesSureRole %i, numMatchesSureName %i", actor.getCharS(1), numMatchesAll, numMatchesFirst, numMatchesSureRole, numMatchesSureName);
  }
  int sum = numMatchesFirst + numMatchesSureRole + 4*numMatchesSureName + 16*numMatchesAll;
  sR.setActors(sum);
  if (debug) esyslog("tvscraper: getActorMatches, sum %i, normMatch %f", sum, searchResultTvMovie::normMatch(sum/16.));
}

void cSearchEventOrRec::getDirectorWriterMatches(searchResultTvMovie &sR, const char *directors, const char *writers) {
  int numMatchesAll = 0;
  int numMatchesSure = 0;
  cContainer alreadyFound;
  for(cSv directorWriter: cSplit(directors, '|')) {
    getDirectorWriterMatches(directorWriter, numMatchesAll, numMatchesSure, alreadyFound);
  }
  for(cSv directorWriter: cSplit(writers, '|')) {
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
  std::vector<searchResultTvMovie>::iterator i;
  for (i = begin; i != end; i++) {
    if (i->m_other_id != 0 || i->movie() ) continue;
    if (i->id() > 0) {
      i->m_other_id = m_db->get_theTVDB_id(i->id());
    } else {
      i->m_other_id = m_db->get_TMDb_id(i->id() );
    }
  }
  std::sort(begin, end);
  float bestMatch = begin->getMatch();
  bool bestMatchMovie = begin->movie();
  int bestMatch_other_id = begin->m_other_id;
  float diffSame = 2.;
  float diffALt  = 2.;
  for (i = begin + 1; i != end; i++) if (i->movie() != bestMatchMovie && i->id() != bestMatch_other_id) { diffALt  = bestMatch - i->getMatch(); break; }
  for (i = begin + 1; i != end; i++) if (i->movie() == bestMatchMovie && i->id() != bestMatch_other_id) { diffSame = bestMatch - i->getMatch(); break; }
  if (debug) esyslog("tvscraper: selectBestAndEnhanceIfRequired, minDiffSame %f, minDiffOther %f, diffALt %f, diffSame %f", minDiffSame, minDiffOther, diffALt, diffSame);
  if (diffSame > minDiffSame && diffALt > minDiffOther) { func(*begin, *this); return false;}
  bool matchAlt = !(diffALt <= diffALt); // set this to false, if still a match to an "alternative" finding is required
  for (i = begin; i != end; i++) {
//    debug = debug || i->id() == -353808 || i->id() == -250822 || i->id() == 440757;
//    if (i->id() == bestMatch_other_id) continue;
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

iExtMovieTvDb *cSearchEventOrRec::getExtMovieTvDb(const searchResultTvMovie &sR) const {
  if (sR.movie() ) return m_movieDbMovieScraper;
  if (sR.id() < 0) return m_tvDbTvScraper;
  return m_movieDbTvScraper;
}

iExtMovieTvDb *cSearchEventOrRec::getExtMovieTvDb(const sMovieOrTv &movieOrTv) const {
  if (movieOrTv.id == 0) return nullptr;
  switch (movieOrTv.type) {
    case scrapMovie:
      return m_movieDbMovieScraper;
    case scrapSeries:
      if (movieOrTv.id < 0) return m_tvDbTvScraper;
      return m_movieDbTvScraper;
    case scrapNone: ; // do nothing
  }
  return nullptr;
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
  if (debug) searchEventOrRec.log(sR);
  searchEventOrRec.getExtMovieTvDb(sR)->enhance1(sR, searchEventOrRec.m_sEventOrRecording->GetLanguage() );
  cSql actors(searchEventOrRec.m_db);
  cSv lang_themoviedb = cSv(searchEventOrRec.m_sEventOrRecording->GetLanguage()->m_themoviedb).substr(0, 2);
  if (sR.movie() ) {
// movie
    cNormedString normedMovieSearchString(searchEventOrRec.m_movieSearchString);
    cSql sql_movieEnhance(searchEventOrRec.m_db, "select movie_runtime, movie_tagline, movie_director, movie_writer, movie_languages from movie_runtime2 where movie_id = ?", sR.id() );
    if (!sql_movieEnhance.readRow() ) {
// we just called enhance1, so data should be available ...
      esyslog("tvscraper, ERROR in cSearchEventOrRec::enhance1, no data in movie_runtime2 for movie id %d", sR.id() );
    } else {
// entry in movie_runtime2 was found
      int movie_runtime = sql_movieEnhance.getInt(0);
      int durationDistance = searchEventOrRec.m_sEventOrRecording->DurationDistance(movie_runtime);
      if (searchEventOrRec.m_num_parts > 0) durationDistance =
        std::min(durationDistance, searchEventOrRec.m_sEventOrRecording->DurationDistance(movie_runtime/searchEventOrRec.m_num_parts));
      sR.setDuration(durationDistance);
      cSv movieTagline = sql_movieEnhance.getStringView(1);
      if (!movieTagline.empty()) sR.updateMatchText(normedMovieSearchString.sentence_distance(movieTagline));
      searchEventOrRec.getDirectorWriterMatches(sR, sql_movieEnhance.getCharS(2), sql_movieEnhance.getCharS(3));
      sR.setTranslationAvailable(sql_movieEnhance.getStringView(4).find(cToSvConcat('|', lang_themoviedb) ) != std::string_view::npos);
    }
// actors
    actors.finalizePrepareBindStep("SELECT actor_name, actor_role FROM actors, actor_movie WHERE actor_movie.actor_id = actors.actor_id AND actor_movie.movie_id = ?", sR.id());
// alternative titles
    for (cSql &sql_title: cSql(searchEventOrRec.m_db, "SELECT alternative_title FROM alternative_titles WHERE external_database = 1 AND id = ?",  sR.id()) ) {
      sR.updateMatchText(normedMovieSearchString.sentence_distance(sql_title.getStringView(0) ));
    }
  } else {
// tv show
    if (debug) esyslog("tvscraper: enhance1 (1)" );
    sR.setDuration(searchEventOrRec.GetTvDurationDistance(sR.id() ) );
    if (debug) esyslog("tvscraper: enhance1 (2)" );
    sR.setDirectorWriter(0);
    cSql stmtScore(searchEventOrRec.m_db, "SELECT tv_score, tv_languages FROM tv_score WHERE tv_id = ?", sR.id() );
    if (!stmtScore.readRow() ) {
      esyslog("tvscraper: ERROR cSearchEventOrRec::enhance1, no data in tv_score, tv_id = %i",  sR.id() );
      sR.setTranslationAvailable(false);
    }
    if (sR.id() < 0) {
// tv show from TheTVDB
      if (stmtScore.readRow() ) {
        sR.setScore(stmtScore.getInt(0) );
        cSplit transSplit(stmtScore.getCharS(1), '|');
        if (transSplit.find(searchEventOrRec.m_sEventOrRecording->GetLanguage()->m_thetvdb) == transSplit.end() ) sR.setTranslationAvailable(false);
        else sR.setTranslationAvailable(true);
      }
      if (debug) searchEventOrRec.log(sR);
      actors.finalizePrepareBindStep("SELECT actor_name, actor_role FROM series_actors WHERE actor_series_id = ?", sR.id() * (-1));
      if (debug) esyslog("tvscraper: enhance1 (5)" );
      if (debug) searchEventOrRec.log(sR);
    } else {
// tv show from The Movie Database (TMDB)
      if (stmtScore.readRow() ) {
        sR.setTranslationAvailable(stmtScore.getStringView(1).find(cToSvConcat('|', lang_themoviedb) ) != std::string_view::npos);
      }
      cNormedString normedSearchString(searchEventOrRec.m_TVshowSearchString);
// alternative titles
      for (cSql &sql_title: cSql(searchEventOrRec.m_db, "SELECT alternative_title FROM alternative_titles WHERE external_database = 2 AND id = ?",  sR.id()) ) {
        sR.updateMatchText(normedSearchString.sentence_distance(sql_title.getStringView(0) ));
      }
      actors.finalizePrepareBindStep("SELECT actor_name, actor_role FROM actors, actor_tv WHERE actor_tv.actor_id = actors.actor_id AND actor_tv.tv_id = ?", sR.id());
    }
  }
  if (debug) esyslog("tvscraper: enhance1 (6)" );
  if (debug) searchEventOrRec.log(sR);
  searchEventOrRec.getActorMatches(sR, actors);
  if (debug) searchEventOrRec.log(sR);
}
void cSearchEventOrRec::enhance2(searchResultTvMovie &searchResult, cSearchEventOrRec &searchEventOrRec) {
  bool debug = searchResult.id() == -353808 || searchResult.id() == -250822 || searchResult.id() == 440757;
  debug = false;
  if (debug) esyslog("tvscraper: enhance2 (1)" );

  if (searchResult.movie() ) return;
//  if (searchResult.movie() ) { searchResult.setMatchEpisode(1000); return; }
//  Transporter - The Mission: Is a movie, and we should not give negative point for having no episode match
  if (searchResult.simulateMatchEpisode(0) < config.minMatchFinal) return; // best possible distance is 0. If, with this distance, the result will till not be selected, we don't need to calculate the distance
  cSv foundName;
  cSv episodeSearchString;
  sMovieOrTv movieOrTv;
  movieOrTv.id = searchResult.id();
  movieOrTv.type = scrapSeries;
  if (debug) esyslog("tvscraper: enhance2 (3)" );
// search episode
  movieOrTv.episodeSearchWithShorttext = searchResult.delim()?0:1;
  if (movieOrTv.episodeSearchWithShorttext) episodeSearchString = searchEventOrRec.m_baseNameEquShortText?searchEventOrRec.m_episodeName:searchEventOrRec.m_sEventOrRecording->EpisodeSearchString();
  else splitString(searchEventOrRec.m_movieSearchString, searchResult.delim(), 4, foundName, episodeSearchString);
  const cLanguage *lang = searchEventOrRec.m_sEventOrRecording->GetLanguage();
  int distance = cMovieOrTv::searchEpisode(searchEventOrRec.m_db, movieOrTv, searchEventOrRec.getExtMovieTvDb(movieOrTv), episodeSearchString, searchEventOrRec.m_baseNameOrTitle, searchEventOrRec.m_years, lang, searchEventOrRec.m_sEventOrRecording->ShortText(), searchEventOrRec.m_sEventOrRecording->Description() );
  searchEventOrRec.m_episodeFound = distance != 1000;
  if (searchEventOrRec.m_episodeFound) distance /= 2; // reduce the distance here, found episode is always positive
  searchResult.setMatchEpisode(distance);
}

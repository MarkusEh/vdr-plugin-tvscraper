#include <string>

class cSearchEventOrRec {
public:
  cSearchEventOrRec(csEventOrRecording *sEventOrRecording, cOverRides *overrides, cMovieDbMovieScraper *movieDbMovieScraper, cMovieDbTvScraper *movieDbTvScraper, cTvDbTvScraper *tvDbTvScraper, cTVScraperDB *db);
  cMovieOrTv *Scrape(int &statistics, cSv channelName); // note: if nothing is found, NULL is returned. Otherwise, the images must be downloaded, and the returned cMovieOrTv must be deleted
private:
  bool ScrapCheckOverride(sMovieOrTv &movieOrTv, cSv channelName);
  scrapType ScrapFind(vector<searchResultTvMovie> &searchResults, cSv &movieName, cSv &episodeSearchString);
  int Store(const sMovieOrTv &movieOrTv);
  void SearchNew(vector<searchResultTvMovie> &resultSet);
  bool addSearchResults(iExtMovieTvDb *extMovieTvDb, vector<searchResultTvMovie> &resultSet, cSv searchString, const cCompareStrings &compareStrings, const cLanguage *lang);
  void initOriginalTitle();
  bool isTitlePartOfPathName(size_t baseNameLen);
  void initBaseNameOrTitle(void);
  bool isVdrDate(cSv baseName);
  bool isVdrDate2(cSv baseName);
  void initSearchString3dots(std::string &searchString);
  void initSearchString(std::string &searchString);
  void setFastMatch(searchResultTvMovie &searchResult);
  int GetTvDurationDistance(int tvID);
  int ScrapFindAndStore(sMovieOrTv &movieOrTv, cSv channelName);
  bool CheckCache(sMovieOrTv &movieOrTv);
  void ScrapAssign(const sMovieOrTv &movieOrTv);
  void getActorMatches(const char *actor, int &numMatchesAll, int &numMatchesFirst, int &numMatchesSure, cContainer &alreadyFound);
  void getDirectorWriterMatches(cSv directorWriter, int &numMatchesAll, int &numMatchesSure, cContainer &alreadyFound);
  void getDirectorWriterMatches(searchResultTvMovie &sR, const char *directors, const char *writers);
  void getActorMatches(searchResultTvMovie &sR, cSql &actors);
  bool addActor(cSv name, int &numMatches, cContainer &alreadyFound);
  bool selectBestAndEnhanceIfRequired(std::vector<searchResultTvMovie>::iterator begin, std::vector<searchResultTvMovie>::iterator end, std::vector<searchResultTvMovie>::iterator &new_end, float minDiff, void (*func)(searchResultTvMovie &sR, cSearchEventOrRec &searchEventOrRec));
  static void enhance1(searchResultTvMovie &sR, cSearchEventOrRec &searchEventOrRec);
  static void enhance2(searchResultTvMovie &sR, cSearchEventOrRec &searchEventOrRec);
  iExtMovieTvDb *getExtMovieTvDb(const searchResultTvMovie &sR) const;
  iExtMovieTvDb *getExtMovieTvDb(const sMovieOrTv &movieOrTv) const;
  void log(const searchResultTvMovie &searchResult, cSv foundName = cSv() );

// passed from constructor
  csEventOrRecording *m_sEventOrRecording;
  cOverRides *m_overrides;
  iExtMovieTvDb *m_movieDbMovieScraper;
  iExtMovieTvDb *m_movieDbTvScraper;
  iExtMovieTvDb *m_tvDbTvScraper;
  cTVScraperDB *m_db; 
// "calculated"
  cString m_baseName;
  cSv m_baseNameOrTitle;
  std::string m_episodeName;
  std::string m_movieSearchString;
  std::string m_TVshowSearchString;
  cSv m_originalTitle;
  cYears m_years;
  eMatchPurpose m_matchPurpose = eMatchPurpose::none;
  int m_season = -1;
  int m_episode = -1;
  int m_num_parts;
  bool m_baseNameEquShortText = false;
  searchResultTvMovie m_searchResult_Movie;
  bool extDbConnected = false; // true, if request to rate limited internet db was required. Otherwise, false
  bool m_episodeFound = false;
};

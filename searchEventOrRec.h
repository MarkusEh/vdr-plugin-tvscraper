#include <string>

class cSearchEventOrRec {
public:
  cSearchEventOrRec(csEventOrRecording *sEventOrRecording, cOverRides *overrides, iExtMovieTvDb *movieDbMovieScraper, cMovieDBScraper *moviedbScraper, cTVDBScraper *tvdbScraper, cTVScraperDB *db);
  cMovieOrTv *Scrape(void); // note: if nothing is found, NULL is returned. Otherwise, the images must be downloaded, and the returned cMovieOrTv must be deleted
private:
  scrapType ScrapFind(vector<searchResultTvMovie> &searchResults, std::string_view movieName, std::string_view episodeSearchString);
  int Store(const sMovieOrTv &movieOrTv);
  bool SearchTv(vector<searchResultTvMovie> &resultSet, std::string_view searchString, bool originalTitle = false);
  void SearchTvEpisTitle(vector<searchResultTvMovie> &resultSet, char delimiter); // Title: name of TV series, and episode name (with : or - between them)
  void SearchMovie(vector<searchResultTvMovie> &searchResults); // 0: no match; return movie ID, search result in m_searchResult_Movie
  void SearchTvAll(vector<searchResultTvMovie> &searchResults);
  void initOriginalTitle();
  bool isTitlePartOfPathName(size_t baseNameLen);
  void initBaseNameOrTitle(void);
  bool isVdrDate(std::string_view baseName);
  bool isVdrDate2(std::string_view baseName);
  void initSearchString3dots(std::string &searchString);
  void initSearchString(std::string &searchString);
  void setFastMatch(searchResultTvMovie &searchResult);
  int GetTvDurationDistance(int tvID);
  void ScrapFindAndStore(sMovieOrTv &movieOrTv);
  bool CheckCache(sMovieOrTv &movieOrTv);
  void ScrapAssign(const sMovieOrTv &movieOrTv);
  int UpdateEpisodeListIfRequired(int tvID, const cLanguage *lang);
  int UpdateEpisodeListIfRequired_i(int tvID);
  void getActorMatches(const char *actor, int &numMatchesAll, int &numMatchesFirst, int &numMatchesSure, cContainer &alreadyFound);
  void getDirectorWriterMatches(std::string_view directorWriter, int &numMatchesAll, int &numMatchesSure, cContainer &alreadyFound);
  void getDirectorWriterMatches(searchResultTvMovie &sR, const char *directors, const char *writers);
  void getActorMatches(searchResultTvMovie &sR, cSql &actors);
  bool addActor(const char *description, std::string_view name, int &numMatches, cContainer &alreadyFound);
  bool selectBestAndEnhanceIfRequired(std::vector<searchResultTvMovie>::iterator begin, std::vector<searchResultTvMovie>::iterator end, std::vector<searchResultTvMovie>::iterator &new_end, float minDiff, void (*func)(searchResultTvMovie &sR, cSearchEventOrRec &searchEventOrRec));
  static void enhance1(searchResultTvMovie &sR, cSearchEventOrRec &searchEventOrRec);
  static void enhance2(searchResultTvMovie &sR, cSearchEventOrRec &searchEventOrRec);
// passed from constructor
  csEventOrRecording *m_sEventOrRecording;
  cOverRides *m_overrides;
  iExtMovieTvDb *m_movieDbMovieScraper;
  cMovieDBScraper *m_moviedbScraper;
  cTVDBScraper *m_tvdbScraper;
  cTVScraperDB *m_db; 
// "calculated"
  cString m_baseName;
  std::string_view m_baseNameOrTitle;
  std::string m_episodeName;
  std::string m_movieSearchString;
  std::string m_TVshowSearchString;
  std::string_view m_originalTitle;
  cYears m_years;
  bool m_baseNameEquShortText = false;
  cMovieDbTv m_tv;
  searchResultTvMovie m_searchResult_Movie;
  bool extDbConnected = false; // true, if request to rate limited internet db was required. Otherwise, false
  bool m_episodeFound = false;
};

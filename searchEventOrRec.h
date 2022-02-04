#include <string>

class cSearchEventOrRec {
public:
  cSearchEventOrRec(csEventOrRecording *sEventOrRecording, cOverRides *overrides, cMovieDBScraper *moviedbScraper, cTVDBScraper *tvdbScraper, cTVScraperDB *db);
  bool Scrap(void); // return true, if request to rate limited internet db was required. Otherwise, false
private:
  scrapType ScrapFind(searchResultTvMovie &searchResult, string &movieName, string &episodeSearchString);
  void Store(const sMovieOrTv &movieOrTv);
  int SearchTv(searchResultTvMovie &searchResult, const string &searchString);
  int SearchTvCacheSearchString(searchResultTvMovie &searchResult, const string &searchString);
  int SearchTvEpisTitle(searchResultTvMovie &searchResult, string &movieName, string &episodeSearchString, char delimiter); // Title: name of TV series, and episode name (with : or - between them)
  int SearchMovie(void); // 0: no match; return movie ID, search result in m_searchResult_Movie
  scrapType FindMovie(const std::string &prefix_delim);
  int SearchTvEpisShortText(searchResultTvMovie &searchResult); // Title: name of TV series, ShortText: Episode
  void initBaseNameOrTitile(void);
  void initSearchString(void);
  void setFastMatch(searchResultTvMovie &searchResult);
  void ScrapFindAndStore(sMovieOrTv &movieOrTv);
  bool CheckCache(sMovieOrTv &movieOrTv);
  void ScrapAssign(const sMovieOrTv &movieOrTv);
  void UpdateEpisodeListIfRequired(int tvID);
// passed from constructor
  csEventOrRecording *m_sEventOrRecording;
  cOverRides *m_overrides;
  cMovieDBScraper *m_moviedbScraper;
  cTVDBScraper *m_tvdbScraper;
  cTVScraperDB *m_db; 
// "calculated"
  std::string m_baseNameOrTitile;
  std::string m_searchString;
  bool m_searchStringSubstituted;
  vector<int> m_years;
  bool m_baseNameEquShortText = false;
  cTVDBSeries m_TVtv;
  cMovieDbTv m_tv;
  cMovieDbMovie m_movie;
  bool extDbConnected = false; // true, if request to rate limited internet db was required. Otherwise, false
  searchResultTvMovie m_searchResult_TvEpisShortText;
  searchResultTvMovie m_searchResult_Movie;
  map<string,searchResultTvMovie> m_cacheTv;
};

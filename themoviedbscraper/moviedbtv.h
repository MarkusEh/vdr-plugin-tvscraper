#ifndef __TVSCRAPER_MOVIEDBTV_H
#define __TVSCRAPER_MOVIEDBTV_H

using namespace std;

class cMovieDBScraper;

// --- cMovieDbTv -------------------------------------------------------------

class cMovieDbTv {
private:
   cTVScraperDB *m_db;
   cMovieDBScraper *m_movieDBScraper;
   const char *m_baseURL = "api.themoviedb.org/3";
   int m_tvID = 0;
   const char *m_tvName = NULL;
   const char *m_tvOriginalName = NULL;
   const char *m_tvOverview = NULL;
   const char *m_first_air_date = NULL;
   std::string m_networks;
   std::string m_genres;
   std::string m_createdBy;
   float m_popularity = 0.0;
   float m_vote_average = 0.0;
   int m_vote_count = 0;
   const char *m_status = NULL;
   const char *m_tvBackdropPath = NULL;
   const char *m_tvPosterPath = NULL;
   int m_tvNumberOfSeasons = 0;
   int m_tvNumberOfEpisodes = 0;
   int m_seasonNumber = 0;
   int m_episodeNumber = 0;
   set<int> m_episodeRunTimes;
   bool ReadTv(bool exits_in_db, cLargeString &buffer);
   bool ReadTv(const rapidjson::Value &tv);
   bool AddOneSeason(cLargeString &buffer);
public:
   cMovieDbTv(cTVScraperDB *db, cMovieDBScraper *movieDBScraper);
   virtual ~cMovieDbTv(void);
   bool UpdateDb(bool updateEpisodes);
   int tvID(void) { return m_tvID; };
   void SetTvID(int tvID) { m_tvID = tvID; };
   int GetSeason() { return m_seasonNumber; }
   int GetEpisode() { return m_episodeNumber; }
};


#endif // __TVSCRAPER_MOVIEDBTV_H

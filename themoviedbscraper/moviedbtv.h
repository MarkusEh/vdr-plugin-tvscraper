#ifndef __TVSCRAPER_MOVIEDBTV_H
#define __TVSCRAPER_MOVIEDBTV_H

using namespace std;
#include "../tools/searchResultTvMovie.h"

class cMovieDBScraper;

// --- cMovieDbTv -------------------------------------------------------------

class cMovieDbTv {
private:
   cTVScraperDB *m_db;
   cMovieDBScraper *m_movieDBScraper;
   const string m_baseURL = "api.themoviedb.org/3";
   int m_tvID = 0;
   string m_tvName = "";
   string m_tvOriginalName = "";
   string m_tvOverview = "";
   string m_first_air_date = "";
   string m_networks = "";
   string m_genres = "";
   float m_popularity = 0.0;
   float m_vote_average = 0.0;
   string m_status = "";
   string m_tvBackdropPath = "";
   string m_tvPosterPath = "";
   int m_tvNumberOfSeasons = 0;
   int m_tvNumberOfEpisodes = 0;
   int m_seasonNumber = 0;
   int m_episodeNumber = 0;
   vector<int> m_episodeRunTimes;
   bool ReadTv(bool exits_in_db);
   bool ReadTv(json_t *tv);
   bool AddTvResults(json_t *root, vector<searchResultTvMovie> &resultSet, const string &tvSearchString);
   bool AddOneSeason();
   bool AddOneSeason(json_t *root);
   bool AddActors();
   bool AddActors(json_t *root, int episode_id);
   bool AddActorsTv(json_t *jCredits);
public:
   cMovieDbTv(cTVScraperDB *db, cMovieDBScraper *movieDBScraper);
   virtual ~cMovieDbTv(void);
   bool AddTvResults(vector<searchResultTvMovie> &resultSet, const string &tvSearchString);
   bool UpdateDb();
   bool StoreSeasonPoster(const string &SeasonPosterPath);
   int tvID(void) { return m_tvID; };
   void SetTvID(int tvID) { m_tvID = tvID; };
   void StoreMedia(void);
   void Dump();
   int GetSeason() { return m_seasonNumber; }
   int GetEpisode() { return m_episodeNumber; }
};


#endif // __TVSCRAPER_MOVIEDBTV_H

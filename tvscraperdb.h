#ifndef __TVSCRAPER_TVSCRAPERDB_H
#define __TVSCRAPER_TVSCRAPERDB_H

using namespace std; 

// --- cTVScraperDB --------------------------------------------------------

class cTVScraperDB {
private:
    sqlite3 *db;
    string dbPathPhys;
    string dbPathMem;
    bool inMem;
    int printSqlite3Errmsg(const string &query);
    int execSql(const string &sql);
    int execSqlBind(const string &sql, const string &value);
    vector<vector<string> > Query(const string &query);
    bool QueryLine(vector<string> &result, const string &query);
    bool QueryValue(string &result, const string &query);
    sqlite3_int64 QueryInt64(const string &query);
    int QueryInt(const string &query);
    vector<vector<string> > QueryEscaped(string query, string where);
    int LoadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave);
    bool CreateTables(void);
    std::size_t SearchEpisode_int(sMovieOrTv &movieOrTv, const string &tvSearchEpisodeString);
    void WriteRecordingInfo(const cRecording *recording, int movie_tv_id, int season_number, int episode_number);
public:
    cTVScraperDB(void);
    virtual ~cTVScraperDB(void);
    bool Connect(void);
    void BackupToDisc(void);
    void ClearOutdated(const string &movieDir, const string &seriesDir);
    bool CheckMovieOutdatedEvents(int movieID, int season_number, int episode_number);
    bool CheckMovieOutdatedRecordings(int movieID, int season_number, int episode_number);
    void DeleteMovie(int movieID, string movieDir);
    void DeleteSeries(int seriesID, const string &movieDir, const string &seriesDir);
    void InsertTv(int tvID, const string &name, const string &originalName, const string &overview, const string &firstAired, const string &networks, const string &genres, float popularity, float vote_average, const string &status, const vector<int> &EpisodeRunTimes);
    void InsertTv_s_e(int tvID, int season_number, int episode_number, int episode_id, const string &episode_name, const string &airDate, float vote_average, const string &episode_overview, const string &episode_guest_stars = "");
    void TvSetEpisodesUpdated(int tvID);
    void TvSetNumberOfEpisodes(int tvID, int LastSeason, int NumberOfEpisodes);
    bool TvGetNumberOfEpisodes(int tvID, int &LastSeason, int &NumberOfEpisodes);
    void InsertEvent(const cEvent *event, const cRecording *recording, int movie_tv_id, int season_number, int episode_number);
    void InsertActor(int seriesID, string name, string role, string thumb);
    void InsertMovie(int movieID, const string &title, const string &original_title, const string &tagline, const string &overview, bool adult, int collection_id, const string &collection_name, int budget, int revenue, const string &genres, const string &homepage, const string &release_date, int runtime, float popularity, float vote_average);

    void InsertMovieActor(int movieID, int actorID, string name, string role);
    void InsertTvActor(int tvID, int actorID, const string &name, const string &role);
    void InsertTvEpisodeActor(int episodeID, int actorID, const string &name, const string &role);
    bool MovieExists(int movieID);
    bool TvExists(int tvID);
    int SearchMovie(string movieTitle);
    int SearchTv(string tvTitle);
    int SearchTvEpisode(int tvID, string episode_search_name, int &season_number, int &episode_number, string &episode_name);
    string GetStillPathTvEpisode(int tvID, int season_number, int episode_number);
    void InsertRecording2(const cEvent *event, const cRecording *recording, int movie_tv_id, int season_number, int episode_number);
    bool SetRecording(const cEvent *event, const cRecording *recording);
    void ClearRecordings2(void);
    bool CheckStartScrapping(int minimumDistance);
    bool GetMovieTvID(const cEvent *event, const cRecording *recording, int &movie_tv_id, int &season_number, int &episode_number);
    vector<vector<string> > GetActorsMovie(int movieID);
    vector<vector<string> > GetActorsSeries(int seriesID);
    int GetEpisodeID(int tvID, int seasonNumber, int episodeNumber);
    vector<vector<string> > GetGuestActorsTv(int episodeID);
    vector<vector<string> > GetActorsTv(int tvID);
    string GetDescriptionTv(int tvID);
    string GetDescriptionTv(int tvID, int seasonNumber, int episodeNumber);
    string GetDescriptionMovie(int movieID);
    bool GetMovie(int movieID, string &title, string &original_title, string &tagline, string &overview, bool &adult, int &collection_id, string &collection_name, int &budget, int &revenue, string &genres, string &homepage, string &release_date, int &runtime, float &popularity, float &vote_average);
    int GetMovieRuntime(int movieID);
    bool GetTv(int tvID, string &name, string &overview, string &firstAired, string &networks, string &genres, float &popularity, float &vote_average, string &status);
    bool GetTv(int tvID, time_t &lastUpdated, string &status);
    bool GetTvEpisode(int tvID, int seasonNumber, int episodeNumber, int &episodeID, string &name, string &airDate, float &vote_average, string &overview, string &episodeGuestStars);
    bool SearchEpisode(sMovieOrTv &movieOrTv, const string &tvSearchEpisodeString);
    bool GetFromCache(const string &movieNameCache, const cEvent *event, const cRecording *recording, sMovieOrTv &movieOrTv);
    void InsertCache(const string &movieNameCache, const cEvent *event, const cRecording *recording, sMovieOrTv &movieOrTv);
    void DeleteOutdatedCache();

};

#endif //__TVSCRAPER_TVSCRAPPERDB_H

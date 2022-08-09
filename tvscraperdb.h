#ifndef __TVSCRAPER_TVSCRAPERDB_H
#define __TVSCRAPER_TVSCRAPERDB_H
#include <sqlite3.h>

using namespace std; 

// --- cTVScraperDB --------------------------------------------------------

class cTVScraperDB {
private:
    sqlite3 *db;
    string dbPathPhys;
    string dbPathMem;
    bool inMem;
// methods to execute sql statements
    int execSql(const string &sql) const;
    bool QueryValue(const char *query, const char *bind, const char *fmt_result, va_list &vl, ...) const;
    vector<vector<string> > Query(const char *query, const char *bind, ...) const;
// low level methosd for sql
    int prepare_bind(sqlite3_stmt **statement, const char *query, const char *bind, va_list &vl) const;
    int step_read_result(sqlite3_stmt *statement, const char *fmt_result, va_list &vl) const;
    int printSqlite3Errmsg(const char *query) const;
// manipulate tables
    bool TableExists(const char *table);
    bool TableColumnExists(const char *table, const char *column);
    void AddCulumnIfNotExists(const char *table, const char *column, const char *type);
// others
    int LoadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave);
    bool CreateTables(void);
    void WriteRecordingInfo(const cRecording *recording, int movie_tv_id, int season_number, int episode_number);
public:
    cTVScraperDB(void);
    virtual ~cTVScraperDB(void);
    bool Connect(void);
    void BackupToDisc(void);
// allow sql statments from outside this class
    int execSql(const char *query, const char *bind, ...) const;
    bool QueryLine(const char *query, const char *bind, const char *fmt_result, ...) const;
    int QueryInt(const char *query, const char *bind, ...) const;
    sqlite3_int64 QueryInt64(const char *query, const char *bind, ...) const;
    string QueryString(const char *query, const char *bind, ...) const;
    sqlite3_stmt *QueryPrepare(const char *query, const char *bind, ...) const;
    bool QueryStep(sqlite3_stmt *&statement, const char *fmt_result, ...) const;
    void ClearOutdated();
    bool CheckMovieOutdatedEvents(int movieID, int season_number, int episode_number);
    bool CheckMovieOutdatedRecordings(int movieID, int season_number, int episode_number);
    void DeleteMovie(int movieID, string movieDir);
    int DeleteMovie(int movieID);
    void DeleteSeries(int seriesID, const string &movieDir, const string &seriesDir);
    int DeleteSeries(int seriesID);
    void InsertTv(int tvID, const string &name, const string &originalName, const string &overview, const string &firstAired, const string &networks, const string &genres, float popularity, float vote_average, int vote_count, const string &posterUrl, const string &fanartUrl, const string &IMDB_ID, const string &status, const vector<int> &EpisodeRunTimes, const string &createdBy);
    void InsertTv_s_e(int tvID, int season_number, int episode_number, int episode_absolute_number, int episode_id, const string &episode_name, const string &airDate, float vote_average, int vote_count, const string &episode_overview, const string &episode_guest_stars, const string &episode_director, const string &episode_writer, const string &episode_IMDB_ID, const string &episode_still_path);
    string GetEpisodeStillPath(int tvID, int seasonNumber, int episodeNumber);
    void TvSetEpisodesUpdated(int tvID);
    void TvSetNumberOfEpisodes(int tvID, int LastSeason, int NumberOfEpisodes);
    bool TvGetNumberOfEpisodes(int tvID, int &LastSeason, int &NumberOfEpisodes);
    void InsertEvent(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number);
    void InsertActor(int seriesID, const string &name, const string &role, int number);
    void InsertMovie(int movieID, const string &title, const string &original_title, const string &tagline, const string &overview, bool adult, int collection_id, const string &collection_name, int budget, int revenue, const string &genres, const string &homepage, const string &release_date, int runtime, float popularity, float vote_average, int vote_count, const string &productionCountries, const string &posterUrl, const string &fanartUrl, const string &IMDB_ID);
    void InsertMovieDirectorWriter(int movieID, const string &director, const string &writer);

    void InsertMovieActor(int movieID, int actorID, const string &name, const string &role, bool hasImage);
    void InsertTvActor(int tvID, int actorID, const string &name, const string &role, bool hasImage);
    void InsertTvEpisodeActor(int episodeID, int actorID, const string &name, const string &role, bool hasImage);
    bool MovieExists(int movieID);
    bool TvExists(int tvID);
    bool SearchTvEpisode(int tvID, const string &episode_search_name, int &season_number, int &episode_number);
    string GetStillPathTvEpisode(int tvID, int season_number, int episode_number);
    void InsertRecording2(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number);
    bool SetRecording(csEventOrRecording *sEventOrRecording);
    void ClearRecordings2(void);
    bool CheckStartScrapping(int minimumDistance);
    bool GetMovieTvID(csEventOrRecording *sEventOrRecording, int &movie_tv_id, int &season_number, int &episode_number, int *runtime = NULL);
    int GetMovieCollectionID(int movieID) const;
    vector<vector<string> > GetActorsMovie(int movieID);
    vector<vector<string> > GetActorsSeries(int seriesID);
    std::string GetEpisodeName(int tvID, int seasonNumber, int episodeNumber);
    vector<vector<string> > GetGuestActorsTv(int episodeID);
    vector<vector<string> > GetActorsTv(int tvID);
    string GetDescriptionTv(int tvID);
    string GetDescriptionTv(int tvID, int seasonNumber, int episodeNumber);
    string GetDescriptionMovie(int movieID);
    bool GetMovie(int movieID, string &title, string &original_title, string &tagline, string &overview, bool &adult, int &collection_id, string &collection_name, int &budget, int &revenue, string &genres, string &homepage, string &release_date, int &runtime, float &popularity, float &vote_average);
    string GetMovieTagline(int movieID);
    int GetMovieRuntime(int movieID);
    vector<vector<string> > GetTvRuntimes(int tvID);
    bool GetTv(int tvID, string &name, string &overview, string &firstAired, string &networks, string &genres, float &popularity, float &vote_average, string &status);
    bool GetTv(int tvID, time_t &lastUpdated, string &status);
    bool GetTvVote(int tvID, float &vote_average, int &vote_count);
    bool GetTvEpisode(int tvID, int seasonNumber, int episodeNumber, int &episodeID, string &name, string &airDate, float &vote_average, string &overview, string &episodeGuestStars);
    bool GetFromCache(const string &movieNameCache, csEventOrRecording *sEventOrRecording, sMovieOrTv &movieOrTv, bool baseNameEquShortText = false);
    void InsertCache(const string &movieNameCache, csEventOrRecording *sEventOrRecording, sMovieOrTv &movieOrTv, bool baseNameEquShortText = false);
    void DeleteOutdatedCache();
    int DeleteFromCache(const char *movieNameCache); // return number of deleted entries
    vector<vector<string> > GetTvMedia(int tvID, eMediaType mediaType, bool movie = false);
    void insertTvMedia (int tvID, const string &path, eMediaType mediaType);
    void insertTvMediaSeasonPoster (int tvID, const string &path, eMediaType mediaType, int season);
    bool existsTvMedia (int tvID, const string &path);
    void deleteTvMedia (int tvID, bool movie = false, bool keepSeasonPoster = true);
    void AddActorDownload (int tvID, bool movie, int actorId, const string &actorPath);
    vector<vector<string> > GetActorDownload(int tvID, bool movie);
    void DeleteActorDownload (int tvID, bool movie);
    sqlite3_stmt *GetAllMovies();
    int GetRuntime(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number);
    vector<int> getSimilarTvShows(int tv_id);
    void setSimilar(int tv_id1, int tv_id2);
};

#endif //__TVSCRAPER_TVSCRAPPERDB_H

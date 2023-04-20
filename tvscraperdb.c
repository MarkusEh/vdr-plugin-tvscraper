#include <string>
#include <sstream>
#include <vector>
#include <sqlite3.h>
#include <stdarg.h>
#include <filesystem>
#include "tvscraperdb.h"
#include "services.h"
#include "rapidjson/pointer.h"
using namespace std;

cTVScraperDB::cTVScraperDB(void) {
    db = NULL;
    string memHD = "/dev/shm/";
    inMem = !config.GetReadOnlyClient() && CheckDirExistsRam(memHD.c_str());
    if (inMem) {
        stringstream sstrDbFileMem;
        sstrDbFileMem << memHD << "tvscraper2.db";
        dbPathMem = sstrDbFileMem.str();
    }
    stringstream sstrDbFile;
    sstrDbFile << config.GetBaseDir() << "tvscraper2.db";
    dbPathPhys = sstrDbFile.str();
}

cTVScraperDB::~cTVScraperDB() {
    sqlite3_close(db);
}

int cTVScraperDB::printSqlite3Errmsg(std::string_view query) const {
// return 0 if there was no error
// otherwise, return error code
  int errCode = sqlite3_errcode(db);
  if (errCode == SQLITE_OK || errCode == SQLITE_DONE || errCode == SQLITE_ROW) return 0;
  int extendedErrCode = sqlite3_extended_errcode(db);
  if (errCode == SQLITE_CONSTRAINT && (extendedErrCode == SQLITE_CONSTRAINT_UNIQUE || extendedErrCode == SQLITE_CONSTRAINT_PRIMARYKEY)) return 0; // not an error, just the entry already exists
  const char *err = sqlite3_errmsg(db);
  if(err) {
    if (strcmp(err, "not an error") != 0)
      esyslog("tvscraper: ERROR query failed: %.*s, error: %s, error code %i extendedErrCode %i", static_cast<int>(query.length()), query.data(), err, errCode, extendedErrCode);
    else return 0;
  } else 
    esyslog("tvscraper: ERROR query failed: %.*s, no error text, error code %i extendedErrCode %i", static_cast<int>(query.length()), query.data(), errCode, extendedErrCode);
  return errCode;
}
// helpers for updates, if db changes =======================================
bool cTVScraperDB::TableColumnExists(const char *table, const char *column) {
  bool found = false;
  stringstream sql;
  sql << "SELECT * FROM " << table << " WHERE 1 = 2;";
  sqlite3_stmt *statement;
  if(sqlite3_prepare_v2(db, sql.str().c_str(), -1, &statement, 0) == SQLITE_OK)
    for (int i=0; i< sqlite3_column_count(statement); i++) if (strcmp(sqlite3_column_name(statement, i), column) == 0) { found = true; break; }
  sqlite3_finalize(statement); 
  printSqlite3Errmsg(sql.str().c_str()  );
  return found;
}

bool cTVScraperDB::TableExists(const char *table) {
  return queryInt("select count(type) from sqlite_master where type= ? and name= ?", "table", table) == 1;
}

void cTVScraperDB::AddCulumnIfNotExists(const char *table, const char *column, const char *type) {
  if (TableColumnExists(table, column) ) return;
  stringstream sql;
  sql << "ALTER TABLE " << table << " ADD COLUMN " << column << " " << type;
  exec(sql.str().c_str() );
}

bool cTVScraperDB::Connect(void) {
    if (inMem) {
        time_t timeMem = LastModifiedTime(dbPathMem.c_str() );
        time_t timePhys = LastModifiedTime(dbPathPhys.c_str() );
        bool readFromPhys = timePhys > timeMem;
        if (sqlite3_open(dbPathMem.c_str(),&db)!=SQLITE_OK) {
            esyslog("tvscraper: failed to open or create %s", dbPathMem.c_str());
            return false;
        }
        if (readFromPhys) {
          esyslog("tvscraper: connecting to db %s", dbPathMem.c_str());
          int rc = LoadOrSaveDb(db, dbPathPhys.c_str(), false);
          if (rc != SQLITE_OK) {
              esyslog("tvscraper: error while loading data from %s, errorcode %d", dbPathPhys.c_str(), rc);
              return false;
          }
        }
    } else {
        if (sqlite3_open(dbPathPhys.c_str(),&db)!=SQLITE_OK) {
            esyslog("tvscraper: failed to open or create %s", dbPathPhys.c_str());
            return false;
        }
        esyslog("tvscraper: connecting to db %s", dbPathPhys.c_str());
    }
    if (config.GetReadOnlyClient() ) return true;
    return CreateTables();
}

void cTVScraperDB::BackupToDisc(void) {
    if (inMem) {
        LoadOrSaveDb(db, dbPathPhys.c_str(), true);
    }
}

int cTVScraperDB::LoadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave) {
    int rc;                   /* Function return code */
    sqlite3 *pFile;           /* Database connection opened on zFilename */
    sqlite3_backup *pBackup;  /* Backup object used to copy data */
    sqlite3 *pTo;             /* Database to copy to (pFile or pInMemory) */
    sqlite3 *pFrom;           /* Database to copy from (pFile or pInMemory) */


    if (isSave) esyslog("tvscraper: access %s for write", zFilename);
    else esyslog("tvscraper: access %s for read", zFilename);
    rc = sqlite3_open(zFilename, &pFile);
    if( rc==SQLITE_OK ){
        pFrom = (isSave ? pInMemory : pFile);
        pTo   = (isSave ? pFile     : pInMemory);
        pBackup = sqlite3_backup_init(pTo, "main", pFrom, "main");
        if( pBackup ){
            (void)sqlite3_backup_step(pBackup, -1);
            (void)sqlite3_backup_finish(pBackup);
        }
        rc = sqlite3_errcode(pTo);
    }

    (void)sqlite3_close(pFile);
    esyslog("tvscraper: access to %s finished", zFilename);
    return rc;
}

bool cTVScraperDB::CreateTables(void) {
    stringstream sql;
// tv2: data for a TV show.
// tv_id > 0 -> data from themoviedb
// tv_id < 0 -> data from thetvdb
    sql << "DROP TABLE IF EXISTS tv;";
    sql << "CREATE TABLE IF NOT EXISTS tv2 (";
    sql << "tv_id integer primary key, ";
    sql << "tv_name nvarchar(255), ";
    sql << "tv_original_name nvarchar(255), ";
    sql << "tv_overview text, ";
    sql << "tv_first_air_date text, ";
    sql << "tv_networks text, ";
    sql << "tv_genres text, ";
    sql << "tv_created_by text, ";
    sql << "tv_popularity real, ";
    sql << "tv_vote_average real, ";
    sql << "tv_vote_count integer, ";
    sql << "tv_posterUrl text, ";
    sql << "tv_fanartUrl text, ";
    sql << "tv_IMDB_ID text, ";
    sql << "tv_status text, ";
    sql << "tv_last_season integer, ";
    sql << "tv_number_of_episodes integer, ";
    sql << "tv_last_updated integer";
    sql << ");";

    sql << "CREATE TABLE IF NOT EXISTS tv_episode_run_time (";
    sql << "tv_id integer, ";
    sql << "episode_run_time integer);";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_tv_episode_run_time on tv_episode_run_time (tv_id, episode_run_time);";

    sql << "CREATE TABLE IF NOT EXISTS tv_vote (";
    sql << "tv_id integer primary key, ";
    sql << "tv_vote_average real, ";
    sql << "tv_vote_count integer);";

    sql << "CREATE TABLE IF NOT EXISTS tv_score (";
    sql << "tv_id integer primary key, ";
    sql << "tv_score real);";

    sql << "CREATE TABLE IF NOT EXISTS tv_media (";
    sql << "tv_id integer, ";
    sql << "media_path nvarchar, ";
    sql << "media_type integer, ";
    sql << "media_number integer);";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_tv_media on tv_media (tv_id, media_path);";
// if mediaType == season_poster, media_number is the season
// otherwise, it numbers the media (0, 1, ...)
// for movies, media_number is < 0

    sql << "DROP TABLE IF EXISTS movie_runtime;";
    sql << "CREATE TABLE IF NOT EXISTS movie_runtime2 (";
    sql << "movie_id integer primary key, ";
    sql << "movie_runtime integer, ";
    sql << "movie_tagline nvarchar(255), ";
    sql << "movie_director nvarchar, ";
    sql << "movie_writer nvarchar);";

// episode information for tv
    sql << "CREATE TABLE IF NOT EXISTS tv_s_e (";
    sql << "tv_id integer, ";
    sql << "season_number integer, ";
    sql << "episode_number integer, ";
    sql << "episode_absolute_number integer, ";
    sql << "episode_id integer, ";
    sql << "episode_name nvarchar(255), ";
    sql << "episode_air_date nvarchar(255), ";
    sql << "episode_run_time integer, ";
    sql << "episode_vote_average real, ";
    sql << "episode_vote_count integer, ";
    sql << "episode_overview nvarchar, ";
    sql << "episode_guest_stars nvarchar, ";
    sql << "episode_director nvarchar, ";
    sql << "episode_writer nvarchar, ";
    sql << "episode_IMDB_ID text, ";
    sql << "episode_still_path nvarchar";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_tv_s_e on tv_s_e (tv_id, season_number, episode_number); ";
    sql << "DROP INDEX IF EXISTS idx_tv_s_e_episode;";

// episode names, language dependent
    sql << "CREATE TABLE IF NOT EXISTS tv_s_e_name (";
    sql << "episode_id integer, ";
    sql << "language_id integer, ";
    sql << "episode_name nvarchar";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_tv_s_e_name on tv_s_e_name (episode_id, language_id); ";

    sql << "CREATE TABLE IF NOT EXISTS actor_tv (";
    sql << "tv_id integer, ";
    sql << "actor_id integer, ";
    sql << "actor_role nvarchar(255)";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_actor_tv on actor_tv (tv_id, actor_id); ";

    sql << "CREATE TABLE IF NOT EXISTS actor_tv_episode (";
    sql << "episode_id integer, ";
    sql << "actor_id integer, ";
    sql << "actor_role nvarchar(255)";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_actor_tv on actor_tv_episode (episode_id, actor_id); ";

// actors for tv shows from thetvdb
    sql << "CREATE TABLE IF NOT EXISTS series_actors (";
    sql << "actor_series_id integer, ";
    sql << "actor_name nvarchar(255), ";
    sql << "actor_role nvarchar(255), ";
    sql << "actor_number integer";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_series_actors on series_actors (actor_series_id, actor_name, actor_role); ";
    sql << "CREATE INDEX IF NOT EXISTS idx1 on series_actors (actor_series_id); ";

// very similar TV shows
    sql << "CREATE TABLE IF NOT EXISTS tv_similar (";
    sql << "tv_id integer primary key, ";
    sql << "equal_id integer);";
    sql << "CREATE INDEX IF NOT EXISTS idx1 on tv_similar (equal_id); ";

// data for movies from themoviedb
    sql << "DROP TABLE IF EXISTS movies;";
    sql << "DROP TABLE IF EXISTS movies2;";
    sql << "CREATE TABLE IF NOT EXISTS movies3 (";
    sql << "movie_id integer primary key, ";
    sql << "movie_title nvarchar(255), ";
    sql << "movie_original_title nvarchar(255), ";
    sql << "movie_tagline nvarchar(255), ";
    sql << "movie_overview text, ";
    sql << "movie_adult integer, ";
    sql << "movie_collection_id integer, ";
    sql << "movie_collection_name text, ";
    sql << "movie_budget integer, ";
    sql << "movie_revenue integer, ";
    sql << "movie_genres text, ";
    sql << "movie_homepage text, ";
    sql << "movie_release_date text, ";
    sql << "movie_runtime integer, ";
    sql << "movie_popularity real, ";
    sql << "movie_vote_average real, ";
    sql << "movie_vote_count integer, ";
    sql << "movie_production_countries nvarchar, ";
    sql << "movie_posterUrl text, ";
    sql << "movie_fanartUrl text, ";
    sql << "movie_IMDB_ID text";
    sql << ");";

// actors from themoviedb
    sql << "CREATE TABLE IF NOT EXISTS actors (";
    sql << "actor_id integer primary key, ";
    sql << "actor_has_image integer , ";
    sql << "actor_name nvarchar(255)";
    sql << ");";

    sql << "CREATE TABLE IF NOT EXISTS actor_download (";
    sql << "movie_id integer, ";
    sql << "is_movie integer, ";
    sql << "actor_id integer, ";
    sql << "actor_path nvarchar";
    sql << ");";

    sql << "CREATE TABLE IF NOT EXISTS actor_movie (";
    sql << "actor_id integer, ";
    sql << "movie_id integer, ";
    sql << "actor_role nvarchar(255)";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx2 on actor_movie (actor_id, movie_id); ";

    sql << "CREATE TABLE IF NOT EXISTS cache (";
    sql << "movie_name_cache nvarchar(255), ";
    sql << "recording integer, "; // 0: no recording. 1: recording. 3: recording, with TV show format
          // bit 0: recording
          // bit 1: TV show format (recording name = episode name)
    sql << "duration integer, ";
    sql << "year integer, ";
    sql << "episode_search_with_shorttext integer, "; // 1: season_number&episode_number must be searched with shorttext
                 // 3: there was a matching episode, and this was required to select this result
    sql << "movie_tv_id integer, ";   // movie if season_number == -100. Otherwisse, tv
    sql << "season_number integer, ";
    sql << "episode_number integer, ";
    sql << "cache_entry_created_at integer";
    sql << ");";
    sql << "DROP INDEX IF EXISTS idx_cache;";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_cache2 on cache (movie_name_cache, recording, duration, year); ";

// mapping EPG event to tv show / movie
    sql << "CREATE TABLE IF NOT EXISTS event (";
    sql << "event_id integer, ";
    sql << "channel_id nvarchar(255), ";
    sql << "valid_till integer, ";
    sql << "runtime integer, ";       // runtime of thetvdb, which does best match to runtime of event
    sql << "movie_tv_id integer, ";   // movie if season_number == -100. Otherwisse, tv
    sql << "season_number integer, ";
    sql << "episode_number integer";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_event on event (event_id, channel_id); ";

// mapping recording to tv show / movie
    sql << "CREATE TABLE IF NOT EXISTS recordings2 (";
    sql << "event_id integer, ";
    sql << "event_start_time integer, ";
    sql << "channel_id nvarchar(255), ";
    sql << "runtime integer, ";       // runtime of thetvdb, which does best match to runtime of recording
    sql << "movie_tv_id integer, ";   // movie if season_number == -100. Otherwisse, tv
    sql << "season_number integer, ";
    sql << "episode_number integer";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_recordings2 on recordings2 (event_id, event_start_time, channel_id); ";

    sql << "CREATE TABLE IF NOT EXISTS scrap_checker (";
    sql << "last_scrapped integer ";
    sql << ");";
    
    if (TableExists("tv2") && !TableColumnExists("tv2", "tv_created_by")) {
      exec("drop table tv2;");
      exec("drop table tv_s_e;");
      exec("drop table movies3;");
    }
    char *errmsg;
    if (sqlite3_exec(db,sql.str().c_str(),NULL,NULL,&errmsg)!=SQLITE_OK) {
        esyslog("tvscraper: ERROR: createdb: %s", errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return false;
    }
    AddCulumnIfNotExists("movie_runtime2", "movie_director", "nvarchar");
    AddCulumnIfNotExists("movie_runtime2", "movie_writer", "nvarchar");
    AddCulumnIfNotExists("actors", "actor_has_image", "integer");
    AddCulumnIfNotExists("event", "runtime", "integer");
    AddCulumnIfNotExists("recordings2", "runtime", "integer");

    AddCulumnIfNotExists("tv_s_e", "episode_run_time", "integer");
// move from actor_thumbnail to actor_number, and delete culumn actor_path (if exists)
    if (TableColumnExists("series_actors", "actor_thumbnail") ) {
      stringstream sql;
      sql << "CREATE TABLE IF NOT EXISTS series_actors2 (";
      sql << "actor_series_id integer, ";
      sql << "actor_name nvarchar(255), ";
      sql << "actor_role nvarchar(255), ";
      sql << "actor_number integer);";
      exec(sql.str().c_str() );
      exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_series_actors2 on series_actors2 (actor_series_id, actor_name, actor_role); ");
      exec("CREATE INDEX IF NOT EXISTS idx12 on series_actors2 (actor_series_id);");

      for (cSql &stmtActors: cSql(this, "SELECT actor_series_id, actor_name, actor_role, actor_thumbnail FROM series_actors;")) {
        stringstream sql;
        sql << "INSERT INTO series_actors2 (actor_series_id, actor_name, actor_role, actor_number) ";
        sql << "VALUES (" << stmtActors.getCharS(0) << ", ?, ?, ";
        size_t pos;
        const std::string_view actor_thumbnail = stmtActors.getStringView(3);
        if (actor_thumbnail.length() > 10 && (pos = actor_thumbnail.find(".jpg") ) !=  std::string::npos) {
          sql << actor_thumbnail.substr(6, pos - 6);
        } else {
          sql << -1;
        }
        sql << ");";
        exec(sql.str().c_str(), stmtActors.getCharS(1), stmtActors.getCharS(2));
      }
      exec("DROP TABLE series_actors;");
      exec("ALTER TABLE series_actors2 RENAME to series_actors;");
    }
    return true;
}

void cTVScraperDB::ClearOutdated() const {
// delete all invalid events pointing to movies
// and delete all invalid events pointing to series
  exec("delete from event where valid_till < ?", time(0));
  DeleteOutdatedCache();
  esyslog("tvscraper: Cleanup Done");
}

void cTVScraperDB::DeleteMovie(int movieID, string movieDir) const {
    //delete images
    stringstream backdrop;
    backdrop << movieDir << "/" << movieID << "_backdrop.jpg";
    stringstream poster;
    poster << movieDir << "/" << movieID << "_poster.jpg";
    DeleteFile(backdrop.str());
    DeleteFile(poster.str());
//delete this movie from db
    DeleteMovie(movieID);
}
int cTVScraperDB::DeleteMovie(int movieID) const {
//delete this movie from db
  deleteTvMedia (movieID, true, false);  // deletes entries in table tv_media
  DeleteActorDownload (movieID, true);  // deletes entries in actor_download
  exec("DELETE FROM movies3 WHERE movie_id = ?", movieID);
  return sqlite3_changes(db);
}

bool cTVScraperDB::CheckMovieOutdatedEvents(int movieID, int season_number, int episode_number) const {
// check if there is still an event for which movieID is required
  const char sql_m[] = "select count(event_id) from event where season_number  = ? and movie_tv_id = ? and valid_till > ?";
  const char sql_t[] = "select count(event_id) from event where season_number != ? and movie_tv_id = ? and valid_till > ?";
  if (season_number == -100) return queryInt(sql_m, -100, movieID, time(0) ) > 0;
                else         return queryInt(sql_t, -100, movieID, time(0) ) > 0;
}

bool cTVScraperDB::CheckMovieOutdatedRecordings(int movieID, int season_number, int episode_number) const {
// check if there is still a recording for which movieID is required
  const char sql_m[] = "select count(event_id) from recordings2 where season_number  = ? and movie_tv_id = ?";
  const char sql_t[] = "select count(event_id) from recordings2 where season_number != ? and movie_tv_id = ?";
  if (season_number == -100) return queryInt(sql_m, -100, movieID) > 0;
                        else return queryInt(sql_t, -100, movieID) > 0;
}

int cTVScraperDB::DeleteSeries(int seriesID) const {
// seriesID > 0 => moviedb
// seriesID < 0 => tvdb
// this one only deletes the entry in tv database. No images, ... are deleted
  if (seriesID > 0) {
// delete actor_tv_episode, data are only for moviedb
    exec("delete from actor_tv_episode where episode_id in ( select episode_id from tv_s_e where tv_s_e.tv_id = ? );",
      seriesID);
  };
// delete tv_s_e
  exec("DELETE FROM tv_s_e WHERE tv_id = ?", seriesID);
// delete tv
  exec("DELETE FROM tv2    WHERE tv_id = ?", seriesID);
  int num_del = sqlite3_changes(db);
// don't delete from tv_episode_runtime, very small, but very usefull
  deleteTvMedia (seriesID, false, false);  // deletes entries in table tv_media
  DeleteActorDownload (seriesID, false); // deletes entries in table actor_download
  return num_del;
}

void cTVScraperDB::DeleteSeries(int seriesID, const string &movieDir, const string &seriesDir) const {
// seriesID > 0 => moviedb
// seriesID < 0 => tvdb
// delete images
  stringstream folder;
  if (seriesID < 0) folder << seriesDir << "/" << seriesID * (-1);
               else folder << movieDir <<  "/tv/" << seriesID;
  DeleteAll(folder.str() );
// now the db entries
  DeleteSeries(seriesID);
}

void cTVScraperDB::InsertTv(int tvID, const char *name, const char *originalName, const char *overview, const char *firstAired, const char *networks, const string &genres, float popularity, float vote_average, int vote_count, const char *posterUrl, const char *fanartUrl, const char *IMDB_ID, const char *status, const set<int> &EpisodeRunTimes, const char *createdBy) {
  exec("INSERT OR REPLACE INTO tv2 (tv_id, tv_name, tv_original_name, tv_overview, tv_first_air_date, tv_networks, tv_genres, tv_popularity, tv_vote_average, tv_vote_count, tv_posterUrl, tv_fanartUrl, tv_IMDB_ID, tv_status, tv_created_by, tv_last_season, tv_number_of_episodes) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, 0);",
    tvID, name, originalName, overview, firstAired, networks, genres, popularity,
    vote_average, vote_count, posterUrl, fanartUrl, IMDB_ID, status, createdBy);

    InsertTvEpisodeRunTimes(tvID, EpisodeRunTimes);
    exec("INSERT OR REPLACE INTO tv_vote (tv_id, tv_vote_average, tv_vote_count) VALUES (?, ?, ?)",
      tvID, vote_average, vote_count);
    exec("INSERT OR REPLACE INTO tv_score (tv_id, tv_score) VALUES (?, ?)", tvID, popularity);
}

void cTVScraperDB::InsertTvEpisodeRunTimes(int tvID, const set<int> &EpisodeRunTimes) {
  if (EpisodeRunTimes.size() == 0) {
    esyslog("tvscraper: ERROR in InsertTvEpisodeRunTimes, EpisodeRunTimes.size() == 0, tvID = %d", tvID);
    return;
  }
  cSql stmt(this, "INSERT OR REPLACE INTO tv_episode_run_time (tv_id, episode_run_time) VALUES (?, ?);");
  for (const int &episodeRunTime: EpisodeRunTimes)
    stmt.resetBindStep(tvID, episodeRunTime);
}

void cTVScraperDB::TvSetEpisodesUpdated(int tvID) {
  exec("UPDATE tv2 set tv_last_updated = ? where tv_id= ?", time(0), tvID);
}

void cTVScraperDB::TvSetNumberOfEpisodes(int tvID, int LastSeason, int NumberOfEpisodes) {
  exec("UPDATE tv2 set tv_last_season = ? , tv_number_of_episodes = ? where tv_id= ?",
    LastSeason, NumberOfEpisodes, tvID);
}

bool cTVScraperDB::TvGetNumberOfEpisodes(int tvID, int &LastSeason, int &NumberOfEpisodes) {
  cSql sql(this, "select tv_last_season, tv_number_of_episodes from tv2 where tv_id = ?", tvID);
  return sql.readRow(LastSeason, NumberOfEpisodes);
}

bool cTVScraperDB::episodeNameUpdateRequired(int tvID, int langId) {
  const char *sqld = "select count(episode_id) from tv_s_e where tv_id = ?;";
  const char *sqll = "select count(tv_s_e_name.episode_id) from tv_s_e, tv_s_e_name where tv_s_e_name.episode_id = tv_s_e.episode_id and tv_s_e.tv_id = ? and tv_s_e_name.language_id = ?;";
  int numEpisodesD = queryInt(sqld, tvID);
  int numEpisodesL = queryInt(sqll, tvID, langId);
  if (numEpisodesD == numEpisodesL) return false;
  if (config.enableDebug) esyslog("tvscraper: episodeNameUpdateRequired tvID %i, numEpisodesD %i numEpisodesL %i langId %i",
      tvID, numEpisodesD, numEpisodesL, langId);
  if (numEpisodesD > numEpisodesL) return true;
  esyslog("tvscraper: ERROR episodeNameUpdateRequired tvID %i, numEpisodesD %i numEpisodesL %i langId %i",
      tvID, numEpisodesD, numEpisodesL, langId);
  return false;
}

void cTVScraperDB::InsertEvent(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number) {
    tEventID eventID = sEventOrRecording->EventID();
    time_t validTill = sEventOrRecording->EndTime();
    std::string channelIDs = sEventOrRecording->ChannelIDs();
    int runtime = GetRuntime(sEventOrRecording, movie_tv_id, season_number, episode_number);
    exec("INSERT OR REPLACE INTO event (event_id, channel_id, valid_till, runtime, movie_tv_id, season_number, episode_number) VALUES (?, ?, ?, ?, ?, ?, ?);",
      eventID, channelIDs, validTill, runtime, movie_tv_id, season_number, episode_number);
}
void cTVScraperDB::DeleteEventOrRec(csEventOrRecording *sEventOrRecording) {
// deletes assignment of sEventOrRecording to movie/TV show
  tEventID eventID = sEventOrRecording->EventID();
  std::string channelIDs = sEventOrRecording->ChannelIDs();
  if (!sEventOrRecording->Recording() ){
    exec("DELETE FROM event where event_id = ? and channel_id = ?;", eventID, channelIDs);
  } else {
    time_t eventStartTime = sEventOrRecording->StartTime();
    exec("DELETE FROM recordings2 where event_id = ? and channel_id = ? and event_start_time = ?;", eventID, channelIDs, eventStartTime);
  }
}

void cTVScraperDB::InsertMovie(int movieID, const char *title, const char *original_title, const char *tagline, const char *overview, bool adult, int collection_id, const char *collection_name, int budget, int revenue, const char *genres, const char *homepage, const char *release_date, int runtime, float popularity, float vote_average, int vote_count, const char *productionCountries, const char *posterUrl, const char *fanartUrl, const char *IMDB_ID){

  exec("INSERT OR REPLACE INTO movies3 (movie_id, movie_title, movie_original_title, movie_tagline, movie_overview, movie_adult, movie_collection_id, movie_collection_name, movie_budget, movie_revenue, movie_genres, movie_homepage, movie_release_date, movie_runtime, movie_popularity, movie_vote_average, movie_vote_count, movie_production_countries, movie_posterUrl, movie_fanartUrl, movie_IMDB_ID) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
    movieID, title, original_title, tagline, overview, adult, collection_id, collection_name, budget, revenue,
    genres, homepage, release_date, runtime, popularity, vote_average,
    vote_count, productionCountries, posterUrl, fanartUrl, IMDB_ID);
// extra table for runtime & tagline, will not be deleted
  if (runtime == 0) runtime = -1; // -1 : no runtime available in themoviedb
  exec("INSERT OR REPLACE INTO movie_runtime2 (movie_id, movie_runtime, movie_tagline) VALUES (?, ?, ?);",
    movieID, runtime, tagline);
}

string cTVScraperDB::GetMovieTagline(int movieID) {
  return queryString("select movie_tagline from movie_runtime2 where movie_id = ?", movieID);
}

int cTVScraperDB::GetMovieRuntime(int movieID) const {
// -1 : no runtime available in themoviedb
//  0 : themoviedb never checked for runtime
    return queryInt("select movie_runtime from movie_runtime2 where movie_id = ?", movieID);
}

void cTVScraperDB::InsertActor(int seriesID, const char *name, const char *role, const char *path) {
  bool debug = seriesID == 78804;
  debug = false;
  int actorNumber = -1;
  cSql sql(this, "SELECT actor_number FROM series_actors WHERE actor_series_id = ? AND actor_name = ? AND actor_role = ?");
  if (sql.resetBindStep(seriesID, name, role).readRow(actorNumber) ) {
// entry already in db
    if (actorNumber >= 0 && path && *path) {
// we don't check here if the file already exists. This is checked during download,
// existing files are not overwritten or downloaded again
      addActorDownload(seriesID * -1, false, actorNumber, path);
    }
    if (actorNumber == -1 && path && *path) {
// entry in db, with explicit actorNumber == -1. This is set if there was no path, last time this actor was found
// note: for entry in db, with actor_number not written: 0 will be returned
      actorNumber = findUnusedActorNumber(seriesID);
      if (debug) esyslog("tvscraper: InsertActor, update, actorNumber = %i, actor_name = %s, actor_role = %s", actorNumber, name, role);
      const char *sql_un = "UPDATE series_actors SET actor_number = ? WHERE actor_series_id = ? AND actor_name = ? AND actor_role = ?";
      exec(sql_un, actorNumber, seriesID, name, role);
      addActorDownload(seriesID * -1, false, actorNumber, path);
    }
  } else {
// no entry in db
  if (!path || !*path ) actorNumber = -1;
  else actorNumber = findUnusedActorNumber(seriesID);
  if (debug) esyslog("tvscraper: InsertActor, new, actorNumber = %i, actor_name = %s, actor_role = %s", actorNumber, name, role);
  exec("INSERT INTO series_actors (actor_series_id, actor_name, actor_role, actor_number) VALUES (?, ?, ?, ?);",
    seriesID, name, role, actorNumber);
  if (actorNumber != -1) addActorDownload(seriesID * -1, false, actorNumber, path);
  }
}

bool cTVScraperDB::MovieExists(int movieID) {
  return queryInt("select count(movie_id) as found from movies3 where movie_id = ?", movieID) == 1;
}

bool cTVScraperDB::TvExists(int tvID) {
  return queryInt("select count(tv_id) as found from tv2 where tv_id = ?", tvID) == 1;
}

int cTVScraperDB::GetRuntime(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number) {
// return runtime from internat database, which matches best the runtime of the current event
// return 0 if no runtime information is available
  if (season_number == -100) return GetMovieRuntime(movie_tv_id);
  int runtime = queryInt("select episode_run_time from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?", movie_tv_id, season_number, episode_number);
  if (runtime > 0) return runtime;
// runtime for this episode is not available. Check which runtime (in list of runtimes) fits best
  int durationInMinLow;
  int durationInMinHigh;
  int best_runtime = 0;
  const char *sql = "select episode_run_time from tv_episode_run_time where tv_id = ?";
  if (!sEventOrRecording->DurationRange(durationInMinLow, durationInMinHigh) ) {
// no information allowing us to check best fit is available
    int n_rtimes = 0;
    for (int runtime2: cSqlInt(this, cStringRef(sql), movie_tv_id)) {
//      runtime = statement.getInt(0);
      if (runtime2 > 0) { n_rtimes++; best_runtime = runtime2; }
    }
    if (n_rtimes == 1) return best_runtime;  // there is exactly one meaningfull runtime
    return 0; // no data. Better admit this fact than returning an arbitrarily choosen runtime
  }
// tv show, more than one runtime is available. Select the best fitting one
  int runtime_distance = 20000;
  for (cSql &statement: cSql(this, cStringRef(sql), movie_tv_id)) {
    runtime = statement.getInt(0);
    if (runtime <= 0) continue;
    int dist = 0;
    if (runtime > durationInMinHigh) dist = runtime - durationInMinHigh;
    if (runtime < durationInMinLow)  dist = durationInMinLow - runtime;

    if (dist < runtime_distance) {
      best_runtime = runtime;
      runtime_distance = dist;
    }
  }
  return best_runtime;
}

int cTVScraperDB::InsertRecording2(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number) {
// return 1 if the movieTv was already assigned to the recording
// return 2 if the movieTv was not yet assigned to the recording, but it is done now
  tEventID eventID = sEventOrRecording->EventID();
  time_t eventStartTime = sEventOrRecording->StartTime();
  std::string channelIDs = sEventOrRecording->ChannelIDs();
  const char *sql= "SELECT COUNT(*) FROM recordings2 WHERE event_id = ? AND event_start_time = ? AND channel_id = ? AND movie_tv_id = ? AND season_number = ? AND episode_number = ?";
  if (queryInt(sql, eventID, eventStartTime, channelIDs, movie_tv_id, season_number, episode_number) > 0) return 1;

// movieTv was not yet assigned to the recording, assign it now
  int runtime = GetRuntime(sEventOrRecording, movie_tv_id, season_number, episode_number);
  exec("INSERT or REPLACE INTO recordings2 (event_id, event_start_time, channel_id, runtime, movie_tv_id, season_number, episode_number) VALUES (?, ?, ?, ?, ?, ?, ?)",
    eventID, eventStartTime, channelIDs, runtime, movie_tv_id, season_number, episode_number);

  WriteRecordingInfo(sEventOrRecording->Recording(), movie_tv_id, season_number, episode_number);
// copy images to recording folder will be done later, with cMovieOrTv->copyImagesToRecordingFolder(
// required as InsertRecording2 is called before the images are downloaded
  return 2;
}

void cTVScraperDB::WriteRecordingInfo(const cRecording *recording, int movie_tv_id, int season_number, int episode_number) {
  if (!recording || !recording->FileName() ) return;  // no place to write the information
  std::string filename = concatenate(recording->FileName(), "/tvscrapper.json");
// open / parse existing file
  rapidjson::Document jInfo;
  cLargeString document_s(jsonReadFile(jInfo, filename.c_str() ));
  if (jInfo.HasParseError() ) {
    std::error_code ec;
    std::filesystem::copy_file(filename, filename + ".bak", ec);
    if (ec.value() != 0) esyslog("tvscraper: ERROR \"%s\", code %i  tried to copy \"%s\" to \"%s.bak\"", ec.message().c_str(), ec.value(), filename.c_str(), filename.c_str() );
    jInfo.SetObject();
  }

// set new attributes
rapidjson::Value jTvscraper;
jTvscraper.SetObject();

// set attributes
const char *sql_tv = "select tv_name, tv_first_air_date from tv2 where tv_id = ?";
const char *sql_mv = "select movie_title, movie_release_date from movies3 where movie_id = ?";
const char *sql;
cSql stmtEpisodeName(this, "select episode_name from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?"); // must exist until json is written
if (season_number != -100) {
// TV Show
  jTvscraper.AddMember("type", rapidjson::Value().SetString("tv show"), jInfo.GetAllocator() );
  sql = sql_tv;
  if( season_number != 0 || episode_number != 0) {  // season / episode was found
    jTvscraper.AddMember("season_number", rapidjson::Value().SetInt(season_number), jInfo.GetAllocator() );
    jTvscraper.AddMember("episode_number", rapidjson::Value().SetInt(episode_number), jInfo.GetAllocator() );
// get episode name
    stmtEpisodeName.resetBindStep(movie_tv_id, season_number, episode_number);
    const char *episode_name;
    if (stmtEpisodeName.readRow(episode_name) && episode_name) 
      jTvscraper.AddMember("episode_name", rapidjson::Value().SetString(rapidjson::StringRef(episode_name) ), jInfo.GetAllocator() );
}} else {
// movie
  jTvscraper.AddMember("type", rapidjson::Value().SetString("movie"), jInfo.GetAllocator() );
  sql = sql_mv;
}
cSql sqlI(this, cStringRef(sql), movie_tv_id);
if (sqlI.readRow() ) {
  const char *title = sqlI.getCharS(0);
  const char *date = sqlI.getCharS(1);
  if (title) jTvscraper.AddMember("name", rapidjson::Value().SetString(rapidjson::StringRef(title)), jInfo.GetAllocator() );
  if (date && strlen(date) >= 4)
    jTvscraper.AddMember("year", rapidjson::Value().SetInt(atoi(date)), jInfo.GetAllocator() );
}
jTvscraper.AddMember("movie_tv_id", rapidjson::Value().SetInt(abs(movie_tv_id) ), jInfo.GetAllocator() );

// "themoviedb / thetvdb" node: this is owned by "tvscraper", so we can overwrite (if it exists)
if (movie_tv_id > 0) { jInfo.AddMember("themoviedb", jTvscraper, jInfo.GetAllocator() ); rapidjson::Pointer("/thetvdb").Erase(jInfo); }
                else { jInfo.AddMember("thetvdb",    jTvscraper, jInfo.GetAllocator() ); rapidjson::Pointer("/themoviedb").Erase(jInfo); }

// write file
jsonWriteFile(jInfo, filename.c_str());
}

int cTVScraperDB::SetRecording(csEventOrRecording *sEventOrRecording) {
// return 0 if no movieTv is assigned to the event
// return 1 if the movieTv assigned to the event was already assigned to the recording
// return 2 if the movieTv assigned to the event was not yet assigned to the recording, but it is done now

// sEventOrRecording used to be an event, is now a recording
// only called in workers, if a timer is recording
  int movieTvId = 0, seasonNumber = 0, episodeNumber = 0;
  cSql sqlI(this, "select movie_tv_id, season_number, episode_number from event where event_id = ? and channel_id = ?");
  sqlI.resetBindStep(sEventOrRecording->EventID(), sEventOrRecording->ChannelIDs());
  if (!sqlI.readRow(movieTvId, seasonNumber, episodeNumber)) return 0;
  return InsertRecording2(sEventOrRecording, movieTvId, seasonNumber, episodeNumber);
}

void cTVScraperDB::ClearRecordings2(void) {
    exec("DELETE FROM recordings2 where 0 = 0");
}

bool cTVScraperDB::CheckStartScrapping(int minimumDistance) {
  bool startScrapping = false;
  time_t now = time(0);
  time_t last_scrapped = static_cast<time_t>(queryInt64("select last_scrapped from scrap_checker"));
  const char *sql =  "INSERT INTO scrap_checker (last_scrapped) VALUES (?)";
  if (last_scrapped) {
    int difference = (int)(now - last_scrapped);
    if (difference > minimumDistance) {
      startScrapping = true;
      exec("delete from scrap_checker");
      exec(sql, now);
    }
  } else {
    startScrapping = true;
    exec(sql, now);
  }
  return startScrapping;
}

bool cTVScraperDB::GetMovieTvID(const cRecording *recording, int &movie_tv_id, int &season_number, int &episode_number, int *runtime) const {
  if (!recording || !recording->Info() || !recording->Info()->GetEvent()) return false;
  csRecording sRecording(recording);
  if (runtime) {
    cSql sqlI(this, "select runtime, movie_tv_id, season_number, episode_number from recordings2 where event_id = ? and event_start_time = ? and channel_id = ?");
    sqlI.resetBindStep(sRecording.EventID(), sRecording.StartTime(), sRecording.ChannelIDs() );
    return sqlI.readRow(*runtime, movie_tv_id, season_number, episode_number);
  } else {
    cSql sqlI(this, "select movie_tv_id, season_number, episode_number from recordings2 where event_id = ? and event_start_time = ? and channel_id = ?");
    sqlI.resetBindStep(sRecording.EventID(), sRecording.StartTime(), sRecording.ChannelIDs() );
    return sqlI.readRow(movie_tv_id, season_number, episode_number);
  }
}

bool cTVScraperDB::GetMovieTvID(const cEvent *event, int &movie_tv_id, int &season_number, int &episode_number, int *runtime) const {
  if (!event) return false;
  std::string channelIDs = channelToString(event->ChannelID() );
  if (channelIDs.empty() ) esyslog("tvscraper: ERROR in cTVScraperDB::GetMovieTvID (event), !channelIDs");
  if (runtime) {
    cSql sqlI(this, "select runtime, movie_tv_id, season_number, episode_number from event where event_id = ? and channel_id = ?");
    sqlI.resetBindStep(event->EventID(), channelIDs);
    return sqlI.readRow(*runtime, movie_tv_id, season_number, episode_number);
  } else {
    cSql sqlI(this, "select movie_tv_id, season_number, episode_number from event where event_id = ? and channel_id = ?");
    sqlI.resetBindStep(event->EventID(), channelIDs);
    return sqlI.readRow(movie_tv_id, season_number, episode_number);
  }
}

std::string cTVScraperDB::GetEpisodeName(int tvID, int seasonNumber, int episodeNumber) const {
  if (seasonNumber == 0 && episodeNumber == 0) return "";
  return queryString("select episode_name from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?", tvID, seasonNumber, episodeNumber);
}

string cTVScraperDB::GetDescriptionTv(int tvID) {
  return queryString("select tv_overview from tv2 where tv_id = ?", tvID);
}

string cTVScraperDB::GetDescriptionTv(int tvID, int seasonNumber, int episodeNumber) {
  return queryString("select episode_overview from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?", tvID, seasonNumber, episodeNumber);
}

int cTVScraperDB::GetMovieCollectionID(int movieID) const {
  return queryInt("select movie_collection_id from movies3 where movie_id = ?", movieID);
}

bool cTVScraperDB::GetMovie(int movieID, string &title, string &original_title, string &tagline, string &overview, bool &adult, int &collection_id, string &collection_name, int &budget, int &revenue, string &genres, string &homepage, string &release_date, int &runtime, float &popularity, float &vote_average) {
  cSql sql(this, "select movie_title, movie_original_title, movie_tagline, movie_overview, movie_adult, movie_collection_id, movie_collection_name, movie_budget, movie_revenue, movie_genres, movie_homepage, movie_release_date, movie_runtime, movie_popularity, movie_vote_average from movies3 where movie_id = ?", movieID);
  return sql.readRow(title, original_title, tagline, overview, adult, collection_id, collection_name,
       budget, revenue, genres, homepage, release_date, runtime, popularity, vote_average);

}

bool cTVScraperDB::GetTv(int tvID, string &name, string &overview, string &firstAired, string &networks, string &genres, float &popularity, float &vote_average, string &status) {
  cSql sql(this, "select tv_name, tv_overview, tv_first_air_date, tv_networks, tv_genres, tv_popularity, tv_vote_average, tv_status from tv2 where tv_id = ?", tvID);
  return sql.readRow(name, overview, firstAired, networks, genres, popularity, vote_average, status);
}


bool cTVScraperDB::GetTv(int tvID, time_t &lastUpdated, string &status) {
  cSql sql(this, "select tv_last_updated, tv_status from tv2 where tv_id = ?", tvID);
  return sql.readRow(lastUpdated, status);
}

bool cTVScraperDB::GetTvEpisode(int tvID, int seasonNumber, int episodeNumber, int &episodeID, string &name, string &airDate, float &vote_average, string &overview, string &episodeGuestStars) {
  cSql sql(this, "select episode_id, episode_name, episode_air_date, episode_vote_average, episode_overview, episode_guest_stars from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?", tvID, seasonNumber, episodeNumber);
  return sql.readRow(episodeID, name, airDate, vote_average, overview, episodeGuestStars);
}

string cTVScraperDB::GetDescriptionMovie(int movieID) {
  return queryString("select movie_overview from movies3 where movie_id = ?", movieID);
}

bool cTVScraperDB::GetFromCache(const string &movieNameCache, csEventOrRecording *sEventOrRecording, sMovieOrTv &movieOrTv, bool baseNameEquShortText) {
// return true if cache was found
// if nothing was found, return false and set movieOrTv.id = 0

    int recording = 0;
    if (sEventOrRecording->Recording() ) {
      recording = 1;
      if (baseNameEquShortText) recording = 3;
    }
    int durationInSec = sEventOrRecording->DurationInSec();
    int durationInSecLow = std::max(0, durationInSec - 300);
    cSql sql(this, "SELECT year, episode_search_with_shorttext, movie_tv_id, season_number, episode_number " \
                   "FROM cache WHERE movie_name_cache = ? AND recording = ? AND duration BETWEEN ? and ? " \
                   "AND year != ?");
    for (auto &stmt: sql.resetBindStep(cStringRef(movieNameCache), recording, durationInSecLow, durationInSec + 300, 0)) {
      stmt.readRow(movieOrTv.year, movieOrTv.episodeSearchWithShorttext,
                   movieOrTv.id, movieOrTv.season, movieOrTv.episode);
// there was a year match. Check: do we have a year match?
      cYears years;
      sEventOrRecording->AddYears(years);
      years.addYears(movieNameCache.c_str() );
      years.finalize();
      bool yearMatch;
      if (movieOrTv.year < 0) yearMatch = years.find2(-1 * movieOrTv.year) == 1; // 1 near match
      else yearMatch = years.find2(movieOrTv.year) == 2; // 2: exact match
      if (yearMatch) {
        if      (movieOrTv.id     == 0)    movieOrTv.type = scrapNone;
        else if (movieOrTv.season == -100) movieOrTv.type = scrapMovie;
        else                               movieOrTv.type = scrapSeries;
        return true;
      }
    }
// search for cache entry without year match
    cSql sqlI(this, "select episode_search_with_shorttext, movie_tv_id, season_number, episode_number " \
                    "from cache WHERE movie_name_cache = ? AND recording = ? and duration BETWEEN ? and ? " \
                    "and year = ?");
    sqlI.resetBindStep(movieNameCache, recording, durationInSecLow, durationInSec + 300, 0);
    if (sqlI.readRow(movieOrTv.episodeSearchWithShorttext, movieOrTv.id, movieOrTv.season, movieOrTv.episode)) {
        movieOrTv.year = 0;
        if      (movieOrTv.id     == 0)    movieOrTv.type = scrapNone;
        else if (movieOrTv.season == -100) movieOrTv.type = scrapMovie;
        else                               movieOrTv.type = scrapSeries;
        return true;
    }
// no match found
  movieOrTv.id = 0;
  movieOrTv.type = scrapNone;
  return false;
}

void cTVScraperDB::InsertCache(const string &movieNameCache, csEventOrRecording *sEventOrRecording, sMovieOrTv &movieOrTv, bool baseNameEquShortText) {
    int recording = 0;
    if (sEventOrRecording->Recording() ) {
      recording = 1;
      if (baseNameEquShortText) recording = 3;
    }
    exec("INSERT OR REPLACE INTO cache (movie_name_cache, recording, duration, year, episode_search_with_shorttext, movie_tv_id, season_number, episode_number, cache_entry_created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
      movieNameCache, recording, sEventOrRecording->DurationInSec(),
      (movieOrTv.id?movieOrTv.year:0), movieOrTv.episodeSearchWithShorttext, movieOrTv.id,
      movieOrTv.season, movieOrTv.episode, time(0) );
}
void cTVScraperDB::DeleteOutdatedCache() const {
    time_t outdated = time(0) - 2*7*24*60*60;  // entries older than two weeks
    exec("delete from cache where cache_entry_created_at < ?", outdated);
}
int cTVScraperDB::DeleteFromCache(const char *movieNameCache) { // return number of deleted entries
  if (!movieNameCache) return 0;
  if (strcmp(movieNameCache, "ALL") == 0) exec("delete from cache");
  else {
    string cacheS = movieNameCache;
    transform(cacheS.begin(), cacheS.end(), cacheS.begin(), ::tolower);
    exec("delete from cache where movie_name_cache = ?", cacheS);
  }
  return sqlite3_changes(db);
}

void cTVScraperDB::insertTvMedia(int tvID, const char *path, eMediaType mediaType) {
// only for poster || fanart || banner
  if (!path || !*path) return;
  if (existsTvMedia(tvID, path) ) return;
  int num = queryInt("select count(tv_id) as found from tv_media where tv_id = ? and media_type = ?",
    tvID, (int)mediaType);
  
  exec("INSERT INTO tv_media (tv_id, media_path, media_type, media_number) VALUES (?, ?, ?, ?);",
    tvID, path, (int)mediaType, num);

// update path to poster and fanart in tv
  if (num == 0 && tvID < 0) {
// this is the first entry for thetvdb media
    if (mediaType == mediaPoster)
      exec("UPDATE tv2 set tv_posterUrl = ? where tv_id= ?", path, tvID);
    if (mediaType == mediaFanart)
      exec("UPDATE tv2 set tv_fanartUrl = ? where tv_id= ?", path, tvID);
  }
}

void cTVScraperDB::insertTvMediaSeasonPoster (int tvID, const char *path, eMediaType mediaType, int season) {
// only for poster || fanart || banner
  if (!path || !*path) return;
//  if (existsTvMedia (tvID, path) ) return;
  exec("INSERT OR REPLACE INTO tv_media (tv_id, media_path, media_type, media_number) VALUES (?, ?, ?, ?);",
    tvID, path, (int)mediaType, season);
}

bool cTVScraperDB::existsTvMedia (int tvID, const char *path) {
  return cSql(this, "SELECT 1 FROM tv_media WHERE tv_id = ? AND media_path = ? LIMIT 1", tvID, path).readRow();
}

void cTVScraperDB::deleteTvMedia (int tvID, bool movie, bool keepSeasonPoster) const {
  cConcatenate sql;
  sql << "delete from tv_media where tv_id = ? ";
  if (movie) sql << "and media_number <  0";
  else {     sql << "and media_number >= 0";
    if(keepSeasonPoster) sql << " and media_type != "  << (int)mediaSeason;
  }
  exec(sql.getCharS(), tvID);
}

int cTVScraperDB::findUnusedActorNumber (int seriesID) {
/*
  cSql sql(this, "SELECT actor_number FROM series_actors WHERE actor_series_id = ?");
  int highestActorNumber = -1;
  for (cSql &statement: sql.resetBindStep(seriesID)) highestActorNumber = std::max(highestActorNumber, statement.getInt(0));
  return highestActorNumber + 1;
*/
  cSql max(this, "SELECT MAX(actor_number) FROM series_actors WHERE actor_series_id = ?", seriesID);
  if (!max.readRow() ) return 0;
  return max.getInt(0) + 1;
}

void cTVScraperDB::addActorDownload (int tvID, bool movie, int actorId, const char *actorPath, cSql *stmt) {
  if (!actorPath || !*actorPath) return;
  if (stmt) stmt->resetBindStep(tvID, movie, actorId, actorPath);
  else addActorDownload().resetBindStep(tvID, movie, actorId, actorPath);
}

void cTVScraperDB::DeleteActorDownload (int tvID, bool movie) const {
  exec("DELETE FROM actor_download WHERE movie_id = ? AND is_movie = ?", tvID, movie);
}

cSqlGetSimilarTvShows::cSqlGetSimilarTvShows(const cTVScraperDB *db, int tv_id):
    m_sql(db, "SELECT tv_id FROM tv_similar WHERE equal_id = ?") {
  cSql sql(db, "SELECT equal_id FROM tv_similar WHERE tv_id = ?", tv_id);
  if (sql.readRow() ) m_sql.resetBindStep(sql.getInt(0));
  else m_ints.push_back(tv_id);
}
void cTVScraperDB::setSimilar(int tv_id1, int tv_id2) {
  const char *sqlInList = "select equal_id from tv_similar where tv_id = ?";
  int equalId1 = queryInt(sqlInList, tv_id1);
  int equalId2 = queryInt(sqlInList, tv_id2);
  const char *sqlInsert = "insert INTO tv_similar (tv_id, equal_id) VALUES (?, ?);";
  if (equalId1 == 0 && equalId2 == 0) {
    int equalId = queryInt("select max(equal_id) from tv_similar;");
    if (equalId <= 0) equalId = 1;
    else equalId++;
    exec(sqlInsert, tv_id1, equalId);
    exec(sqlInsert, tv_id2, equalId);
    return;
  }
  if (equalId1 == equalId2) return; // nothing to do, already marked as equal
  if (equalId1 != 0 && equalId2 == 0) { exec(sqlInsert, tv_id2, equalId1); return; }
  if (equalId1 == 0 && equalId2 != 0) { exec(sqlInsert, tv_id1, equalId2); return; }
// both are in the table, but with different IDs
  exec("UPDATE tv_similar set equal_id = ? WHERE equal_id = ?", equalId1, equalId2);
}
// cSql =========================================================
bool cSql::prepareInt() {
// true: successfull, m_statement is available
// false: error, and called setFailed()
  m_cur_row = -1;
  m_last_step_result = -10;
  m_num_cols = 0;
  int i = 0;
  for (m_num_q = 0; i < static_cast<int>(m_query.length()); i++) if (m_query[i] == '?') m_num_q++;

  int result = sqlite3_prepare_v2(m_db->db, m_query.data(), m_query.length(), &m_statement, 0); // If there is an error, m_statement is set to NULL.
  m_db->printSqlite3Errmsg(m_query);
  if (result != SQLITE_OK) { setFailed(); return false; }
  if (m_num_q != sqlite3_bind_parameter_count(m_statement)) {
    esyslog("tvscraper: ERROR in cSql::prepare, query %.*s, num_q %i, bind_parameter_count %i", static_cast<int>(m_query.length()), m_query.data(), m_num_q, sqlite3_bind_parameter_count(m_statement) );
    setFailed();
    return false;
  }
  return true;
}

void cSql::stepInt() {
  if (!assertStatement("stepInt")) return;
  if (m_last_step_result == SQLITE_DONE) return;
  m_last_step_result = sqlite3_step(m_statement);
  ++m_cur_row;
  const char *query = sqlite3_sql(m_statement);
  m_db->printSqlite3Errmsg(query?query:"cTVScraperDB::cSql::stepInt");
  m_num_cols = sqlite3_column_count(m_statement);
}

void cSql::assertRvalLval() {
  if (m_rval) {
    const char *query = sqlite3_sql(m_statement);
    esyslog("tvscraper: ERROR in cSql::assertRvalLval, m_rval == true query %s", query?query:"NULL");
  }
  if (m_lval) {
    const char *query = sqlite3_sql(m_statement);
    esyslog("tvscraper: WARNING in cSql::assertRvalLval, m_lval == true query %s", query?query:"NULL");
  }
  m_rval = false;
  m_lval = false;
}


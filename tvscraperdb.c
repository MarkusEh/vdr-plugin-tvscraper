#include <string>
#include <sstream>
#include <vector>
#include <sqlite3.h>
#include <stdarg.h>
#include <filesystem>
#include "tvscraperdb.h"
#include "services.h"
#include "rapidjson/pointer.h"
#include <inttypes.h>

using namespace std;

cTVScraperDB::cTVScraperDB(void):
    m_stmt_cache1(this, "SELECT year, episode_search_with_shorttext, movie_tv_id, season_number, episode_number " \
                   "FROM cache WHERE movie_name_cache = ? AND recording = ? AND duration BETWEEN ? AND ? " \
                   "AND year != ?"),
    m_stmt_cache2(this, "SELECT episode_search_with_shorttext, movie_tv_id, season_number, episode_number " \
                    "FROM cache WHERE movie_name_cache = ? AND recording = ? AND duration BETWEEN ? AND ? " \
                    "AND year = ?"),
    m_select_tv_equal_get_theTVDB_id(this, "SELECT thetvdb_id FROM tv_equal WHERE themoviedb_id = ?"),
    m_select_tv_equal_get_TMDb_id   (this, "SELECT themoviedb_id FROM tv_equal WHERE thetvdb_id = ?"),
    m_select_tv2_overview(this, "SELECT tv_name, tv_first_air_date, tv_IMDB_ID, tv_display_language FROM tv2 WHERE tv_id = ?"),
    m_select_tv_s_e_overview(this, "SELECT episode_name, episode_air_date, episode_run_time, episode_IMDB_ID " \
        "FROM tv_s_e WHERE tv_id = ? AND season_number = ? AND episode_number = ?"),
    m_select_tv_s_e_name2_tv_s_e(this, "SELECT tv_s_e_name2.episode_name, " \
        "tv_s_e.episode_air_date, tv_s_e.episode_run_time, tv_s_e.episode_IMDB_ID " \
        "FROM tv_s_e, tv_s_e_name2 " \
        "WHERE tv_s_e_name2.episode_id = tv_s_e.episode_id " \
        "AND tv_s_e.tv_id = ? AND tv_s_e.season_number = ? AND tv_s_e.episode_number = ? " \
        "AND tv_s_e_name2.language_id = ?"),
    m_select_movies3_overview(this, "SELECT movie_title, movie_collection_id, movie_collection_name, " \
                    "movie_release_date, movie_runtime, movie_IMDB_ID FROM movies3 where movie_id = ?"),
    m_select_event(this, "SELECT movie_tv_id, season_number, episode_number, runtime " \
                    "FROM event WHERE event_id = ? and channel_id = ?"),
    m_select_recordings2(this, "SELECT movie_tv_id, season_number, episode_number, runtime, duration_deviation " \
                    "FROM recordings2 WHERE event_id = ? AND event_start_time = ? AND channel_id = ?")
 {
    m_sqlite3_mutex = sqlite3_mutex_alloc(SQLITE_MUTEX_RECURSIVE);
    if (!m_sqlite3_mutex) esyslog("tvscraper: ERROR in cTVScraperDB::cTVScraperDB, sqlite3_mutex_alloc returned 0");

    db = NULL;
    const char *memHD = "/dev/shm/";
    inMem = !config.GetReadOnlyClient() && CheckDirExistsRam(memHD);
    if (inMem) {
      dbPathMem = concat(memHD, "tvscraper2.db");
    }
    dbPathPhys = concat(config.GetBaseDir(), "tvscraper2.db");
}

cTVScraperDB::~cTVScraperDB() {
  sqlite3_mutex_free(m_sqlite3_mutex);
  sqlite3_close(db);
}

int cTVScraperDB::printSqlite3Errmsg(cSv query) const {
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

void cTVScraperDB::AddColumnIfNotExists(const char *table, const char *column, const char *type) {
  if (TableColumnExists(table, column) ) return;
  stringstream sql;
  sql << "ALTER TABLE " << table << " ADD COLUMN " << column << " " << type;
  exec(sql.str().c_str() );
}

bool cTVScraperDB::Connect(void) {
    if (inMem) {
/*
        time_t timeMem = LastModifiedTime(dbPathMem.c_str() );
        time_t timePhys = LastModifiedTime(dbPathPhys.c_str() );
        bool readFromPhys = timePhys > timeMem;  // use Mem if exists. Note: Phys might have been modified from outside
*/
        struct stat buffer;
        bool readFromPhys = stat (dbPathMem.c_str(), &buffer) != 0; // readFromPhys if dbPathMem does not exist
        int rc = sqlite3_open_v2(dbPathMem.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
        if (rc != SQLITE_OK) {
            esyslog("tvscraper: failed to open or create %s", dbPathMem.c_str());
            return false;
        }
        esyslog("tvscraper: connecting to db %s", dbPathMem.c_str());
        if (readFromPhys) {
          esyslog("tvscraper: update data from %s", dbPathPhys.c_str());
          int rc = LoadOrSaveDb(db, dbPathPhys.c_str(), false);
          if (rc != SQLITE_OK) {
              esyslog("tvscraper: error while loading data from %s, errorcode %d", dbPathPhys.c_str(), rc);
              return false;
          }
        }
    } else {
        int rc;
        if (config.GetReadOnlyClient() )
          rc = sqlite3_open_v2(dbPathPhys.c_str(), &db, SQLITE_OPEN_READONLY                       | SQLITE_OPEN_FULLMUTEX, nullptr);
        else
          rc = sqlite3_open_v2(dbPathPhys.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
        if (rc != SQLITE_OK) {
            esyslog("tvscraper: failed to open or create %s", dbPathPhys.c_str());
            return false;
        }
        esyslog("tvscraper: connecting to db %s", dbPathPhys.c_str());
    }
    if (config.GetReadOnlyClient() ) return true;
    return CreateTables();
}

int cTVScraperDB::BackupToDisc(void) {
  int rc = SQLITE_OK;
  if (inMem) rc = LoadOrSaveDb(db, dbPathPhys.c_str(), true);
  return rc;
}

void write_backup_error(const char *context, const char *filename, int rc, sqlite3 *db = nullptr) {
  int extendedErrCode = db?sqlite3_extended_errcode(db):0;
  const char *err = db?sqlite3_errmsg(db):sqlite3_errstr(rc);
  esyslog("tvscraper: ERROR %s, filename %s, error: %s, error code %i extendedErrCode %i", context, filename, err?err:"no error message", rc, extendedErrCode);
}
int cTVScraperDB::LoadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave) {
    int rc;                   /* Function return code */
    sqlite3 *pFile;           /* Database connection opened on zFilename */
    sqlite3_backup *pBackup;  /* Backup object used to copy data */
    sqlite3 *pTo;             /* Database to copy to (pFile or pInMemory) */
    sqlite3 *pFrom;           /* Database to copy from (pFile or pInMemory) */

    if (isSave) {
      esyslog("tvscraper: access %s for write", zFilename);
      rc = sqlite3_open_v2(zFilename, &pFile, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    } else {
      esyslog("tvscraper: access %s for read", zFilename);
      rc = sqlite3_open_v2(zFilename, &pFile, SQLITE_OPEN_READONLY                       | SQLITE_OPEN_FULLMUTEX, nullptr);
    }
    if( rc==SQLITE_OK ){
        pFrom = (isSave ? pInMemory : pFile);
        pTo   = (isSave ? pFile     : pInMemory);
        pBackup = sqlite3_backup_init(pTo, "main", pFrom, "main");
        if( pBackup ){
            int rc_s = sqlite3_backup_step(pBackup, -1);
            if (rc_s != SQLITE_DONE) write_backup_error("sqlite3_backup_step", zFilename, rc_s, pTo);
            rc = sqlite3_backup_finish(pBackup);
            if (rc != SQLITE_OK) write_backup_error("sqlite3_backup_finish", zFilename, rc, pTo);
            else if (rc_s != SQLITE_DONE) rc = rc_s;
        } else {
          write_backup_error("sqlite3_backup_init", zFilename, sqlite3_errcode(pTo), pTo);
        }
    } else {
      write_backup_error("opening database", zFilename, rc, pFile);
    }
    int rc_c = sqlite3_close(pFile);
    if (rc_c != SQLITE_OK) write_backup_error("closing database", zFilename, rc_c, pFile);
    esyslog("tvscraper: access to %s finished, rc = %d", zFilename, rc);
    return rc;
}

bool cTVScraperDB::CreateTables(void) {
    stringstream sql;
// tv2: data for a TV show.
// tv_id > 0 -> data from The Movie Database (TMDB)
// tv_id < 0 -> data from TheTVDB
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
    sql << "tv_number_of_episodes_in_specials integer, ";
    sql << "tv_display_language integer, ";
    sql << "tv_last_updated integer"; // time stamp: external DB was contacted, and checked for new episodes
    sql << "tv_last_changed integer"; // time stamp: last time when new episodes were added to tv_s_e
    sql << ");";

// used for TheTVDB and TMDB
// each TV show can have several episode run times
// entries in this table are not deletet, so it is a cache
// better: use individual run times in tv_s_e
    sql << "CREATE TABLE IF NOT EXISTS tv_episode_run_time (";
    sql << "tv_id integer, ";
    sql << "episode_run_time integer);";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_tv_episode_run_time on tv_episode_run_time (tv_id, episode_run_time);";

    sql << "DROP TABLE IF EXISTS tv_vote;";

// ===========  tv_score            ================================
// notes for TheTVDB:
//   score not part of search results -> required here
//   tv_vote_average / vote_count not available any more in TheTVDB
//   we also cache the list of available languages
// notes for The Movie Database (TMDB):
//   score not required for TMDB, as TMDB has this list in the search results
//     for movies and series
//   languages not available -> empty
//   tv_actors_last_update -> only data used for TMDB
// entries in this table are not deleted, so it is a cache
// tv_id < 0 => TheTVDB
// tv_id > 0 => The Movie Database (TMDB)
    sql << "CREATE TABLE IF NOT EXISTS tv_score (";
    sql << "tv_id integer primary key, ";
    sql << "tv_score real, ";  // only TheTVDB, for TMDB vote* is part of the search result
    sql << "tv_languages nvarchar, ";
    sql << "tv_languages_last_update integer, ";
    sql << "tv_actors_last_update integer, ";
    sql << "tv_data_available integer);"; // 0 no data; 1: score; 2: languages; 3: actors; 4: alternative_titles; 5: equal ids
// check actors the TMDB
// for TMDB: &append_to_response=credits,translations,alternative_titles
//    if translation is missing in "get movie detail": overview == "", tagline == "", and title == original_title
//    if translation is missing in "search result":    overview == overview in orig. language, and title == original_title

    sql << "CREATE TABLE IF NOT EXISTS tv_media (";
    sql << "tv_id integer, ";
    sql << "media_path nvarchar, ";
    sql << "media_type integer, ";
    sql << "media_number integer);";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_tv_media  on tv_media (tv_id, media_path);";
    sql << "CREATE        INDEX IF NOT EXISTS idx_tv_media2 on tv_media (tv_id, media_type, media_number);";
// if mediaType == season_poster, media_number is the season
// otherwise, it numbers the media (0, 1, ...)
// for movies, media_number is < 0

    sql << "DROP TABLE IF EXISTS movie_runtime;";
    sql << "CREATE TABLE IF NOT EXISTS movie_runtime2 (";
    sql << "movie_id integer primary key, ";
    sql << "movie_runtime integer, ";
    sql << "movie_tagline nvarchar(255), ";
    sql << "movie_director nvarchar, ";
    sql << "movie_writer nvarchar, ";
    sql << "movie_languages nvarchar, ";
    sql << "movie_last_update integer, ";
    sql << "movie_data_available integer);"; //0 no data; 1: runtime; 2: tagline; 3: director/writer; 4: alternative_titles+languages

// ===========  alternative_titles  ================================
// notes for TheTVDB:
//   not required for TheTVDB, as TheTVDB has this list in the search results
//   (without any language information)
//   In get series, TheTVDB provides a language for each alias name
//   (3 letter code, same as for translations)
// notes for The Movie Database (TMDB):
//   not part of search results -> required here
//   TMDB provides a country (!) together with each alternative title
//   see also https://www.themoviedb.org/talk/610d7b404cbe12004b4474a2
//   available for movies and series
    sql << "CREATE TABLE IF NOT EXISTS alternative_titles (";
    sql << "external_database integer, ";  // 1: TMDB movie; 2: TMDB series; 4: TheTVDB series
    sql << "id integer, ";   // id of the object in the external database
    sql << "alternative_title nvarchar, ";
    sql << "iso_3166_1 nvarchar);";  // country !!
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_alternative_titles on alternative_titles (external_database, id, alternative_title);";
/*
for thetvdb:
    for (const rapidjson::Value &alias: cJsonArrayIterator(data, "aliases")) {
      if (alias.IsString() )
      dist_a = compareStrings.minDistance(delim, removeLastPartWithP(alias.GetString()), dist_a);
    }
-> ignore country information
*/


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
// delete old table, with normaices strings
    sql << "DROP INDEX IF EXISTS idx_tv_s_e_name;";
    sql << "DROP TABLE IF EXISTS tv_s_e_name;";
// new table: Original strings for TheTVDB, to allow future improvements
    sql << "CREATE TABLE IF NOT EXISTS tv_s_e_name2 (";
    sql << "episode_id integer, ";
    sql << "language_id integer, ";
    sql << "episode_name nvarchar";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_tv_s_e_name2 on tv_s_e_name2 (episode_id, language_id); ";

// Original strings, for The Movie Database
    sql << "CREATE TABLE IF NOT EXISTS tv_s_e_name_moviedb (";
    sql << "episode_id integer, ";
    sql << "language_id integer, ";
    sql << "episode_name nvarchar";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_tv_s_e_name_moviedb on tv_s_e_name_moviedb (episode_id, language_id); ";

// time stamp, last update of tv_s_e_name2 and tv_s_e_name_moviedb
// tv_id < 0 => TheTVDB
// tv_id > 0 => The Movie Database (TMDB)
    sql << "CREATE TABLE IF NOT EXISTS tv_name (";
    sql << "tv_id integer, ";
    sql << "language_id integer, ";
    sql << "tv_last_updated integer);";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_tv_name on tv_name (tv_id, language_id); ";

// The Movie Database (TMDB)
    sql << "CREATE TABLE IF NOT EXISTS actor_tv (";
    sql << "tv_id integer, ";
    sql << "actor_id integer, ";
    sql << "actor_role nvarchar(255)";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_actor_tv on actor_tv (tv_id, actor_id); ";

// The Movie Database (TMDB)
    sql << "CREATE TABLE IF NOT EXISTS actor_tv_episode (";
    sql << "episode_id integer, ";
    sql << "actor_id integer, ";
    sql << "actor_role nvarchar(255)";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_actor_tv on actor_tv_episode (episode_id, actor_id); ";

// TheTVDB
// actors for tv shows from TheTVDB
// note: actor_series_id > 0, as this is always from TheTVDB
// so here, as an exception, we have positive actor_series_id for data from TheTVDB
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

// ID mapping TheTVDB /TMDb series IDs
    sql << "CREATE TABLE IF NOT EXISTS tv_equal (";
    sql << "themoviedb_id integer primary key, "; // always > 0!
    sql << "thetvdb_id integer);";                // always < 0!
    sql << "CREATE INDEX IF NOT EXISTS idx_tv_equal ON tv_equal (thetvdb_id); ";

// data for movies from The Movie Database
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

// actors from The Movie Database
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
//  sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_actor_download on actor_download (movie_id, is_movie, actor_id); ";

// movie actors from The Movie Database
    sql << "CREATE TABLE IF NOT EXISTS actor_movie (";
    sql << "actor_id integer, ";
    sql << "movie_id integer, ";
    sql << "actor_role nvarchar(255)";
    sql << ");";
    sql << "DROP INDEX IF EXISTS idx2;";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS actor_movie_idx on actor_movie (movie_id, actor_id); ";

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
    sql << "movie_tv_id integer, ";   // movie if season_number == -100. Otherwisse, tv
    sql << "season_number integer, ";
    sql << "episode_number integer, ";
    sql << "runtime integer, ";       // runtime of thetvdb, which does best match to runtime of recording
    sql << "duration_deviation integer);";
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
    AddColumnIfNotExists("tv_score", "tv_languages", "nvarchar");
    AddColumnIfNotExists("tv_score", "tv_languages_last_update", "integer");
    AddColumnIfNotExists("tv_score", "tv_actors_last_update", "integer");
    AddColumnIfNotExists("tv_score", "tv_data_available", "integer");
    AddColumnIfNotExists("tv2", "tv_number_of_episodes_in_specials", "integer");
    AddColumnIfNotExists("tv2", "tv_display_language", "integer");
    AddColumnIfNotExists("movie_runtime2", "movie_director", "nvarchar");
    AddColumnIfNotExists("movie_runtime2", "movie_writer", "nvarchar");
    AddColumnIfNotExists("movie_runtime2", "movie_languages", "nvarchar");
    AddColumnIfNotExists("movie_runtime2", "movie_last_update", "integer");
    AddColumnIfNotExists("movie_runtime2", "movie_data_available", "integer");
    AddColumnIfNotExists("actors", "actor_has_image", "integer");
    AddColumnIfNotExists("event", "runtime", "integer");
    AddColumnIfNotExists("recordings2", "runtime", "integer");
    AddColumnIfNotExists("recordings2", "duration_deviation", "integer");

    AddColumnIfNotExists("tv_s_e", "episode_run_time", "integer");
    if (!TableColumnExists("tv2", "tv_last_changed") ) {
      exec("ALTER TABLE tv2 ADD COLUMN tv_last_changed integer");
      cSql stmt_update(this, "UPDATE tv2 SET tv_last_changed = ? WHERE tv_id = ?");
      for(cSql &stmt: cSql(this, "SELECT tv_id, tv_last_updated FROM tv2")) {
        stmt_update.resetBindStep(stmt.getInt64(1), stmt.getInt(0));
      }
    }
// move from actor_thumbnail to actor_number, and delete column actor_path (if exists)
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
        const cSv actor_thumbnail = stmtActors.getStringView(3);
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

int cTVScraperDB::DeleteMovie(int movieID) const {
//delete this movie from db
// no entries in cache tables are deleted. See DeleteMovieCache
// Images
  DeleteFile(cToSvConcat(config.GetBaseDirMovies(), movieID, "_backdrop.jpg"));
  DeleteFile(cToSvConcat(config.GetBaseDirMovies(), movieID, "_poster.jpg"));
// DB Entries
  deleteTvMedia (movieID, true, false);  // deletes entries in table tv_media
  DeleteActorDownload (movieID, true);  // deletes entries in actor_download
  exec("DELETE FROM movies3 WHERE movie_id = ?", movieID);
  return sqlite3_changes(db);
}
void cTVScraperDB::DeleteMovieCache(int movieID) const {
// this should be called if a movie does not exist in external database any more
// or to enforce the system to re-read data from external database
// delete all cache entries in this local db (in other words: everything which is not deleted with DeleteMovie.
// Note: DeleteMovie is not called. If the movieID is still used in event/recordings2,
//   some data for display are still available. We accept that some data (which where in the cache tables)
//   are missing
  if (config.enableDebug) esyslog("tvscraper: cTVScraperDB::DeleteMovieCache movieID %i", movieID);
  exec("DELETE FROM movie_runtime2       WHERE movie_id = ?", movieID);
  exec("DELETE FROM actor_movie          WHERE movie_id = ?", movieID);
  exec("DELETE FROM alternative_titles   WHERE external_database = 1 AND id = ?", movieID);
  exec("DELETE FROM cache                WHERE movie_tv_id = ? and season_number = -100", movieID);
}
bool cTVScraperDB::CheckMovieOutdatedEvents(int movieID, int season_number, int episode_number) const {
// check if there is still an event for which movieID is required
  const char *sql_m = "select count(event_id) from event where season_number  = ? and movie_tv_id = ? and valid_till > ?";
  const char *sql_t = "select count(event_id) from event where season_number != ? and movie_tv_id = ? and valid_till > ?";
  if (season_number == -100) return queryInt(sql_m, -100, movieID, time(0) ) > 0;
                else         return queryInt(sql_t, -100, movieID, time(0) ) > 0;
}

bool cTVScraperDB::CheckMovieOutdatedRecordings(int movieID, int season_number, int episode_number) const {
// check if there is still a recording for which movieID is required
  const char *sql_m = "select count(event_id) from recordings2 where season_number  = ? and movie_tv_id = ?";
  const char *sql_t = "select count(event_id) from recordings2 where season_number != ? and movie_tv_id = ?";
  if (season_number == -100) return queryInt(sql_m, -100, movieID) > 0;
                        else return queryInt(sql_t, -100, movieID) > 0;
}

void cTVScraperDB::DeleteSeriesCache(int seriesID) const {
// seriesID > 0 => moviedb
// seriesID < 0 => tvdb
// this should be called if a series does not exist in external database any more
// or to enforce the system to re-read data from external database
// delete all cache entries in this local db (in other words: everything which is not deleted with DeleteSeries.
// Note: DeleteSeries is not called. If the series ID is still used in event/recordings2,
//   some data for display are still available. We accept that some data (which where in the cache tables)
//   are missing
  if (config.enableDebug) esyslog("tvscraper: cTVScraperDB::DeleteSeriesCache seriesID %i", seriesID);
  exec("DELETE FROM tv_episode_run_time  WHERE tv_id = ?", seriesID);
  exec("DELETE FROM tv_score             WHERE tv_id = ?", seriesID);
  if (seriesID > 0) {
// only TMDb
    exec("DELETE FROM actor_tv           WHERE tv_id = ?", seriesID);
    exec("DELETE FROM alternative_titles WHERE external_database = 2 AND id = ?", seriesID);
    exec("DELETE FROM tv_equal           WHERE themoviedb_id = ?", seriesID);
  } else {
// only TheTVDB
    exec("DELETE FROM series_actors      WHERE actor_series_id = ?", -seriesID);
    exec("DELETE FROM tv_equal           WHERE thetvdb_id = ?", seriesID);
  }
  exec("DELETE FROM tv_similar           WHERE tv_id = ?", seriesID);
  exec("DELETE FROM cache                WHERE movie_tv_id = ? and season_number != -100", seriesID);
}
int cTVScraperDB::DeleteSeries(int seriesID) const {
// seriesID > 0 => moviedb
// seriesID < 0 => tvdb
// no entries in cache tables are deleted. See DeleteSeriesCache

// images  =============================
  cToSvConcat folder;
  if (seriesID < 0) folder << config.GetBaseDirSeries() << -seriesID;
               else folder << config.GetBaseDirMovieTv() << seriesID;
  DeleteAll(folder);
// DB entries ==========================
  if (seriesID > 0) {
// only for TMDb  ======================
// actor_tv_episode is only meaningfull if there are entries in tv_s_e, which will be deleted ...
    exec("DELETE FROM actor_tv_episode WHERE episode_id in ( SELECT episode_id FROM tv_s_e WHERE tv_s_e.tv_id = ? );", seriesID);
    exec("DELETE FROM tv_s_e_name_moviedb WHERE episode_id in ( SELECT episode_id FROM tv_s_e WHERE tv_s_e.tv_id = ? );", seriesID);
  } else {
// only for TheTVDB ===================
    exec("DELETE FROM tv_s_e_name2 WHERE episode_id in ( SELECT episode_id FROM tv_s_e WHERE tv_s_e.tv_id = ? );", seriesID);
  }
// delete tv_name
  exec("DELETE FROM tv_name WHERE tv_id = ?", seriesID);
// delete tv_s_e
  exec("DELETE FROM tv_s_e WHERE tv_id = ?", seriesID);
// delete tv
  exec("DELETE FROM tv2    WHERE tv_id = ?", seriesID);
  int num_del = sqlite3_changes(db);
// don't delete from cache DBs, see DeleteSeriesCache
  deleteTvMedia (seriesID, false, false);  // deletes entries in table tv_media
  DeleteActorDownload (seriesID, false); // deletes entries in table actor_download
  return num_del;
}

void cTVScraperDB::InsertTv(int tvID, const char *name, const char *originalName, const char *overview, const char *firstAired, const char *networks, const string &genres, float popularity, float vote_average, int vote_count, const char *posterUrl, const char *fanartUrl, const char *IMDB_ID, const char *status, const set<int> &EpisodeRunTimes, const char *createdBy, const char *languages) {
  cSql stmt(this, "SELECT tv_last_updated, tv_last_changed, tv_display_language FROM tv2 WHERE tv_id = ?", tvID);
  if (stmt.readRow() ) {
    exec("INSERT OR REPLACE INTO tv2 (tv_id, tv_name, tv_original_name, tv_overview, tv_first_air_date, tv_networks, tv_genres, tv_popularity, tv_vote_average, tv_vote_count, tv_posterUrl, tv_fanartUrl, tv_IMDB_ID, tv_status, tv_created_by, tv_last_season, tv_number_of_episodes, tv_last_updated, tv_last_changed, tv_display_language) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, 0, ?, ?, ?);",
      tvID, name, originalName, overview, firstAired, networks, genres, popularity,
      vote_average, vote_count, posterUrl, fanartUrl, IMDB_ID, status, createdBy, stmt.getInt64(0), stmt.getInt64(1), stmt.getCharS(2) );
  } else {
    exec("INSERT INTO tv2 (tv_id, tv_name, tv_original_name, tv_overview, tv_first_air_date, tv_networks, tv_genres, tv_popularity, tv_vote_average, tv_vote_count, tv_posterUrl, tv_fanartUrl, tv_IMDB_ID, tv_status, tv_created_by, tv_last_season, tv_number_of_episodes) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, 0);",
      tvID, name, originalName, overview, firstAired, networks, genres, popularity,
      vote_average, vote_count, posterUrl, fanartUrl, IMDB_ID, status, createdBy);
  }

    InsertTvEpisodeRunTimes(tvID, EpisodeRunTimes);
// note: we call InsertTv only in case of reading all data (includeing actors, even if they are updated elsewhere)
    exec("INSERT OR REPLACE INTO tv_score (tv_id, tv_score, tv_languages, tv_languages_last_update, tv_actors_last_update, tv_data_available) VALUES (?, ?, ?, ?, ?, ?)", tvID, popularity, languages, time(0), time(0), 5);
}

void cTVScraperDB::InsertTvEpisodeRunTimes(int tvID, const set<int> &EpisodeRunTimes) {
  if (EpisodeRunTimes.size() == 0) return;
  cSql stmt(this, "INSERT OR REPLACE INTO tv_episode_run_time (tv_id, episode_run_time) VALUES (?, ?);");
  for (const int episodeRunTime: EpisodeRunTimes)
    stmt.resetBindStep(tvID, episodeRunTime);
}
bool cTVScraperDB::TvRuntimeAvailable(int tvID) {
  for (cSql &stmt: cSql(this, "SELECT episode_run_time FROM tv_episode_run_time WHERE tv_id = ?", tvID)) {
    if (stmt.getInt(0) > 0) return true;
  }
  return false;
}

void cTVScraperDB::TvSetEpisodesUpdated(int tvID) {
  exec("UPDATE tv2 set tv_last_updated = ? where tv_id= ?", time(0), tvID);
}

void cTVScraperDB::TvSetNumberOfEpisodes(int tvID, int LastSeason, int NumberOfEpisodes, int NumberOfEpisodesInSpecials) {
  exec("UPDATE tv2 SET tv_last_season = ? , tv_number_of_episodes = ? , tv_number_of_episodes_in_specials = ? where tv_id= ?",
    LastSeason, NumberOfEpisodes, NumberOfEpisodesInSpecials, tvID);
}

bool cTVScraperDB::TvGetNumberOfEpisodes(int tvID, int &LastSeason, int &NumberOfEpisodes, int &NumberOfEpisodesInSpecials) {
  cSql sql(this, "select tv_last_season, tv_number_of_episodes, tv_number_of_episodes_in_specials from tv2 where tv_id = ?", tvID);
  return sql.readRow(LastSeason, NumberOfEpisodes, NumberOfEpisodesInSpecials);
}

bool cTVScraperDB::episodeNameUpdateRequired(int tvID, int langId) {
// return true if there might be new episodes in this language
  cSql stmt_tv2(this, "SELECT tv_last_changed, tv_last_updated, tv_status FROM tv2 WHERE tv_id = ?", tvID);
  if (!stmt_tv2.readRow()) {
//  if (config.enableDebug) esyslog("tvscraper: INFO: cTVScraperDB::episodeNameUpdateRequired not found in tv2, tvID %i, langId %i", tvID, langId);
    return true;
  }
  time_t tv2_tv_last_updated = stmt_tv2.getInt64(1);

  cSql stmt_tv_name_last_updated(this, "SELECT tv_last_updated FROM tv_name WHERE tv_id = ? AND language_id = ?", tvID, langId);
  if (!stmt_tv_name_last_updated.readRow() && (config.GetDefaultLanguage()->m_id != langId || tvID < 0) ) {
//    if (config.enableDebug) dsyslog("tvscraper: INFO: cTVScraperDB::episodeNameUpdateRequired not found in tv_name, tvID %i, langId %i", tvID, langId);
    return true;
  }
// check: additional episodes added, but not in langId?
// Note: we cannot compare the number of episodes in tv_s_e with the number of episodes tv_s_e_name2 for lang:
//       there just might be some texts missing, ...
  if (stmt_tv2.getInt64(0) > stmt_tv_name_last_updated.getInt64(0) && (config.GetDefaultLanguage()->m_id != langId || tvID < 0)) {
    if (config.enableDebug) dsyslog("tvscraper: INFO: cTVScraperDB::episodeNameUpdateRequired tv2.tv_last_changed %lli > tv_name.tv_last_updated %lli, tvID %i, langId %i", (long long)stmt_tv2.getInt64(0), (long long)stmt_tv_name_last_updated.getInt64(0), tvID, langId);
    return true;
  }

// check: would we expect new episodes in external db, for tv_s_e?
  if (!config.isUpdateFromExternalDbRequired(tv2_tv_last_updated )) return false;
  cSv status = stmt_tv2.getStringView(2);
  if (status.compare("Ended") == 0) return false; // see https://thetvdb-api.readthedocs.io/api/series.html
  if (status.compare("Canceled") == 0) return false;
// not documented for themoviedb, see https://developers.themoviedb.org/3/tv/get-tv-details . But test indicates the same values ("Ended" & "Canceled")...
  if (config.enableDebug) dsyslog("tvscraper: INFO: cTVScraperDB::episodeNameUpdateRequired tv2.tv_last_updated %lli, tvID %i, langId %i", (long long)tv2_tv_last_updated, tvID, langId);
  return true;
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

void cTVScraperDB::InsertMovie(int movieID, const char *title, const char *original_title, const char *tagline, const char *overview, bool adult, int collection_id, const char *collection_name, int budget, int revenue, const char *genres, const char *homepage, const char *release_date, int runtime, float popularity, float vote_average, int vote_count, const char *productionCountries, const char *posterUrl, const char *fanartUrl, const char *IMDB_ID, const char *languages) {

  exec("INSERT OR REPLACE INTO movies3 (movie_id, movie_title, movie_original_title, movie_tagline, movie_overview, movie_adult, movie_collection_id, movie_collection_name, movie_budget, movie_revenue, movie_genres, movie_homepage, movie_release_date, movie_runtime, movie_popularity, movie_vote_average, movie_vote_count, movie_production_countries, movie_posterUrl, movie_fanartUrl, movie_IMDB_ID) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
    movieID, title, original_title, tagline, overview, adult, collection_id, collection_name, budget, revenue,
    genres, homepage, release_date, runtime, popularity, vote_average,
    vote_count, productionCountries, posterUrl, fanartUrl, IMDB_ID);
// extra table for runtime & tagline, will not be deleted
  if (runtime == 0) runtime = -1; // -1 : no runtime available in themoviedb
  exec("INSERT OR REPLACE INTO movie_runtime2 (movie_id, movie_runtime, movie_tagline, movie_languages, movie_last_update, movie_data_available) VALUES (?, ?, ?, ?, ?, ?);",
    movieID, runtime, tagline, languages, time(0), 4); // current version of cMovieDbMovie::ReadAndStore stores all of; 1: runtime; 2: tagline; 3: director/writer; 4: alternative_titles
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
  CONCATENATE(filename_old, recording->FileName(), "/tvscrapper.json");
  CONCATENATE(filename_new, recording->FileName(), "/tvscraper.json");
  cJsonDocumentFromFile jInfo_old(filename_old, true);
  cJsonDocumentFromFile jInfo_new(filename_new, true);

  cJsonDocumentFromFile *jInfo;
  if (jInfo_old.HasMember("timer") ) jInfo = &jInfo_old;
  else jInfo = &jInfo_new;

// set new attributes
rapidjson::Value jTvscraper;
jTvscraper.SetObject();

// set attributes
const char *sql_tv = "select tv_name, tv_first_air_date from tv2 where tv_id = ?";
const char *sql_mv = "select movie_title, movie_release_date from movies3 where movie_id = ?";
const char *sql;
cSql stmtEpisodeName(this); // must exist until json is written
if (season_number != -100) {
// TV Show
  jTvscraper.AddMember("type", rapidjson::Value().SetString("tv show"), jInfo->GetAllocator() );
  sql = sql_tv;
  if( season_number != 0 || episode_number != 0) {  // season / episode was found
    jTvscraper.AddMember("season_number", rapidjson::Value().SetInt(season_number), jInfo->GetAllocator() );
    jTvscraper.AddMember("episode_number", rapidjson::Value().SetInt(episode_number), jInfo->GetAllocator() );
// get episode name
    int langInt = queryInt("SELECT tv_display_language FROM tv2 WHERE tv_id = ?", movie_tv_id);
    if (langInt > 0) {
      stmtEpisodeName.finalizePrepareBindStep(
        "SELECT tv_s_e_name2.episode_name " \
        "FROM tv_s_e, tv_s_e_name2 " \
        "WHERE tv_s_e_name2.episode_id = tv_s_e.episode_id " \
        "AND tv_s_e.tv_id = ? AND tv_s_e.season_number = ? AND tv_s_e.episode_number = ?" \
        "AND tv_s_e_name2.language_id = ?",
        movie_tv_id, season_number, episode_number, langInt);
    } else {
      stmtEpisodeName.finalizePrepareBindStep(
        "SELECT episode_name FROM tv_s_e WHERE tv_id = ? AND season_number = ? AND episode_number = ?",
        movie_tv_id, season_number, episode_number);
    }
    const char *episode_name;
    if (stmtEpisodeName.readRow(episode_name) && episode_name) 
      jTvscraper.AddMember("episode_name", rapidjson::Value().SetString(rapidjson::StringRef(episode_name) ), jInfo->GetAllocator() );
}} else {
// movie
  jTvscraper.AddMember("type", rapidjson::Value().SetString("movie"), jInfo->GetAllocator() );
  sql = sql_mv;
}
cSql sqlI(this, cStringRef(sql), movie_tv_id);
if (sqlI.readRow() ) {
  const char *title = sqlI.getCharS(0);
  const char *date = sqlI.getCharS(1);
  if (title) jTvscraper.AddMember("name", rapidjson::Value().SetString(rapidjson::StringRef(title)), jInfo->GetAllocator() );
  if (date && strlen(date) >= 4)
    jTvscraper.AddMember("year", rapidjson::Value().SetInt(atoi(date)), jInfo->GetAllocator() );
}
jTvscraper.AddMember("movie_tv_id", rapidjson::Value().SetInt(abs(movie_tv_id) ), jInfo->GetAllocator() );

// First erase ALL existing entries. Note: These will not be overwritten by rapidjson
  rapidjson::Value::MemberIterator res;
  for (res = jInfo->FindMember("themoviedb"); res != jInfo->MemberEnd(); res = jInfo->FindMember("themoviedb") ) jInfo->RemoveMember(res);
  for (res = jInfo->FindMember("thetvdb")   ; res != jInfo->MemberEnd(); res = jInfo->FindMember("thetvdb")    ) jInfo->RemoveMember(res);

// now add the new entries
if (movie_tv_id > 0) { jInfo->AddMember("themoviedb", jTvscraper, jInfo->GetAllocator() ); }
                else { jInfo->AddMember("thetvdb",    jTvscraper, jInfo->GetAllocator() ); }

// write file
jsonWriteFile(*jInfo, filename_new);
struct stat buffer;
if (stat(filename_old, &buffer) == 0) RenameFile(filename_old, cToSvConcat(&filename_old[0], ".bak"));
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

bool cTVScraperDB::GetMovieTvID(const cRecording *recording, int &movie_tv_id, int &season_number, int &episode_number, int *runtime, int *duration_deviation) const {
  if (!recording || !recording->Info() || !recording->Info()->GetEvent()) return false;
  csRecording sRecording(recording);

//  config.timeSelectFromRecordings.start();
  cUseStmt pre_stmt(m_select_recordings2);
  pre_stmt.stmt()->resetBindStep(sRecording.EventID(), sRecording.StartTime(), sRecording.ChannelIDs() );
/*
  config.timeSelectFromRecordings.stop();
  if ( config.timeSelectFromRecordings.getNumCalls() >= 1964) {
    config.timeSelectFromRecordings.print("live: GetMovieTvID_recording");
    config.timeSelectFromRecordings.reset();
  }
*/
  if (!pre_stmt.stmt()->readRow() ) return false;
  if (runtime) *runtime = pre_stmt.stmt()->getInt(3);
  if (duration_deviation) {
    if (recording->IsEdited() ) *duration_deviation = 0;  // assume who ever cut the recording, checked for completness
    else {
      if (cSv(recording->Info()->Aux()).find("<isEdited>true</isEdited>") != std::string_view::npos) *duration_deviation = 0;
      else *duration_deviation = pre_stmt.stmt()->getInt(4, -2, -2);  // -2 if there is no entry in db
    }
  }
  return pre_stmt.stmt()->readRow(movie_tv_id, season_number, episode_number);
}
bool cTVScraperDB::SetDurationDeviation(const cRecording *recording, int duration_deviation) const {
  if (!recording || duration_deviation < 0 || config.GetReadOnlyClient() ) return false;
  csRecording sRecording(recording);
  exec("UPDATE recordings2 SET duration_deviation = ? WHERE event_id = ? and event_start_time = ? and channel_id = ?", duration_deviation, sRecording.EventID(), sRecording.StartTime(), sRecording.ChannelIDs() );
  return true;
}

bool cTVScraperDB::GetMovieTvID(const cEvent *event, int &movie_tv_id, int &season_number, int &episode_number, int *runtime) const {
  if (!event) return false;
  cToSvChannel channelIDs(event->ChannelID() );
  if (cSv(channelIDs).empty() ) esyslog("tvscraper: ERROR in cTVScraperDB::GetMovieTvID (event), !channelIDs");
  cUseStmt stmt(m_select_event);
  m_select_event.resetBindStep(event->EventID(), channelIDs);
  if (!m_select_event.readRow() ) return false;
  if (runtime) *runtime = m_select_event.getInt(3);
  return m_select_event.readRow(movie_tv_id, season_number, episode_number);
}

std::string cTVScraperDB::GetEpisodeName(int tvID, int seasonNumber, int episodeNumber) const {
  if (seasonNumber == 0 && episodeNumber == 0) return "";
  cSql stmtEpisodeName(this);
  int langInt = queryInt("SELECT tv_display_language FROM tv2 WHERE tv_id = ?", tvID);
  if (langInt > 0) {
    stmtEpisodeName.finalizePrepareBindStep(
      "SELECT tv_s_e_name2.episode_name " \
      "FROM tv_s_e, tv_s_e_name2 " \
      "WHERE tv_s_e_name2.episode_id = tv_s_e.episode_id " \
      "AND tv_s_e.tv_id = ? AND tv_s_e.season_number = ? AND tv_s_e.episode_number = ?" \
      "AND tv_s_e_name2.language_id = ?",
      tvID, seasonNumber, episodeNumber, langInt);
  } else {
    stmtEpisodeName.finalizePrepareBindStep(
      "SELECT episode_name FROM tv_s_e WHERE tv_id = ? AND season_number = ? AND episode_number = ?",
      tvID, seasonNumber, episodeNumber);
  }
  const char *episode_name;
  if (stmtEpisodeName.readRow(episode_name) && episode_name) return episode_name;
  return "";
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

bool cTVScraperDB::GetTvEpisode(int tvID, int seasonNumber, int episodeNumber, int &episodeID, std::string &name, std::string &airDate, float &vote_average, std::string &overview, std::string &episodeGuestStars) {
  cSql sql(this);
  int langInt = queryInt("SELECT tv_display_language FROM tv2 WHERE tv_id = ?", tvID);
  if (langInt > 0) {
    sql.finalizePrepareBindStep(
      "SELECT tv_s_e.episode_id, tv_s_e_name2.episode_name, tv_s_e.episode_air_date, " \
      "tv_s_e.episode_vote_average, tv_s_e.episode_overview, tv_s_e.episode_guest_stars " \
      "FROM tv_s_e, tv_s_e_name2 " \
      "WHERE tv_s_e_name2.episode_id = tv_s_e.episode_id " \
      "AND tv_s_e.tv_id = ? AND tv_s_e.season_number = ? AND tv_s_e.episode_number = ?" \
      "AND tv_s_e_name2.language_id = ?",
      tvID, seasonNumber, episodeNumber, langInt);
  } else {
    sql.finalizePrepareBindStep(
      "SELECT episode_id, episode_name, episode_air_date, episode_vote_average, episode_overview, episode_guest_stars " \
      "FROM tv_s_e WHERE tv_id = ? AND season_number = ? AND episode_number = ?",
      tvID, seasonNumber, episodeNumber);
  }
  return sql.readRow(episodeID, name, airDate, vote_average, overview, episodeGuestStars);
}

string cTVScraperDB::GetDescriptionMovie(int movieID) {
  return queryString("select movie_overview from movies3 where movie_id = ?", movieID);
}

bool cTVScraperDB::GetFromCache(const string &movieNameCache, csEventOrRecording *sEventOrRecording, sMovieOrTv &movieOrTv, bool baseNameEquShortText) {
// return true if cache was found
// if nothing was found, return false and set movieOrTv.id = 0

// note: we use m_stmt_cache1 & m_stmt_cache2.
//   this is not thread save, so GetFromCache must only be called in ONE thread.
//   this one thread is the worker thread (worker.c)
//   Performance improvement: .4ms -> .137ms per call. 2/3 of time to compile the statement ...

    int recording = 0;
    if (sEventOrRecording->Recording() ) {
      recording = 1;
      if (baseNameEquShortText) recording = 3;
    }
    int durationInSec = sEventOrRecording->DurationInSec();
    int durationInSecLow = std::max(0, durationInSec - 300);
    cYears years;
    bool first = true;
    m_cache_time.start();
    {
      cUseStmt stmt_cache1(m_stmt_cache1);
      for (auto &stmt: m_stmt_cache1.resetBindStep(cStringRef(movieNameCache), recording, durationInSecLow, durationInSec + 300, 0)) {
        stmt.readRow(movieOrTv.year, movieOrTv.episodeSearchWithShorttext,
                     movieOrTv.id, movieOrTv.season, movieOrTv.episode);
  // there was a year match. Check: do we have a year match?
        if (first) {
          sEventOrRecording->AddYears(years);
          years.addYears(movieNameCache.c_str() );
          years.finalize();
          first = false;
        }
        bool yearMatch;
        if (movieOrTv.year < 0) yearMatch = years.find2(-1 * movieOrTv.year) == 1; // 1 near match
        else yearMatch = years.find2(movieOrTv.year) == 2; // 2: exact match
        if (yearMatch) {
          if      (movieOrTv.id     == 0)    movieOrTv.type = scrapNone;
          else if (movieOrTv.season == -100) movieOrTv.type = scrapMovie;
          else                               movieOrTv.type = scrapSeries;
          m_cache_time.stop();
          return true;
        }
      }
    }
// search for cache entry without year match
    cUseStmt stmt_cache2(m_stmt_cache2);
    m_stmt_cache2.resetBindStep(movieNameCache, recording, durationInSecLow, durationInSec + 300, 0);
    m_cache_time.stop();
    if (m_stmt_cache2.readRow(movieOrTv.episodeSearchWithShorttext, movieOrTv.id, movieOrTv.season, movieOrTv.episode)) {
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

void cTVScraperDB::InsertCache(cSv movieNameCache, csEventOrRecording *sEventOrRecording, sMovieOrTv &movieOrTv, bool baseNameEquShortText) {
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
    cToSvConcat cacheS;
    cacheS.appendToLower(movieNameCache, g_locale);
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
  cToSvConcat sql;
  sql << "DELETE FROM tv_media WHERE tv_id = ? ";
  if (movie) sql << "AND media_number <  0";
  else {     sql << "AND media_number >= 0";
    if(keepSeasonPoster) sql << " AND media_type != "  << (int)mediaSeason;
  }
  exec(sql, tvID);
}

int cTVScraperDB::findUnusedActorNumber (int seriesID) {
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
void cTVScraperDB::setEqual(int themoviedb_id, int thetvdb_id) {
// use this method, and don't exec INSERT OR REPLACE INTO tv_equal ...
// to make sure that it alsow will work if we add fields to tv_equal

  if (themoviedb_id <= 0 || thetvdb_id >= 0) {
    esyslog("tvscraper: ERROR cTVScraperDB::setEqual, themoviedb_id %d, thetvdb_id %d", themoviedb_id, thetvdb_id);
    return;
  }
  exec("INSERT OR REPLACE INTO tv_equal (themoviedb_id, thetvdb_id) VALUES(?, ?)", themoviedb_id, thetvdb_id);
}

int cTVScraperDB::get_theTVDB_id(int tv_id) const {
  return cUseStmt(m_select_tv_equal_get_theTVDB_id, tv_id).stmt()->getInt(0);
}
int cTVScraperDB::get_TMDb_id(int tv_id) const {
  return cUseStmt(m_select_tv_equal_get_TMDb_id, tv_id).stmt()->getInt(0);
}
int cTVScraperDB::getNormedTvId(int tv_id) const {
// Input: ID of series from TMDb (>0) or TheTVDB (<0)
//        Do NOT call with movie ID from TMDb!!!
// Output: Same ID or equal ID from other ext. database
//   Current implementation: Use TheTVDB if available.
//   This might change!!
  if (tv_id < 0) return tv_id;
  int id = get_theTVDB_id(tv_id);
  return id<0 ? id:tv_id;
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
  if (m_last_step_result == SQLITE_BUSY) {
    isyslog("tvscraper: INFO in cSql::stepInt(): SQLITE_BUSY, rc = %d, repeat", m_last_step_result);
    sleep(1);
    m_last_step_result = sqlite3_step(m_statement);
  }
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


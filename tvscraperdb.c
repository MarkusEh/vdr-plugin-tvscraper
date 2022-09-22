#include <string>
#include <sstream>
#include <vector>
#include <sqlite3.h>
#include <jansson.h>
#include <stdarg.h>
#include <filesystem>
#include "tvscraperdb.h"
#include "services.h"
using namespace std;

cTVScraperDB::cTVScraperDB(void) {
    db = NULL;
    string memHD = "/dev/shm/";
    inMem = !config.GetReadOnlyClient() && CheckDirExists(memHD.c_str());
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

int cTVScraperDB::printSqlite3Errmsg(const char *query) const{
// return 0 if there was no error
// otherwise, return error code
  int errCode = sqlite3_errcode(db);
  if (errCode == SQLITE_OK || errCode == SQLITE_DONE || errCode == SQLITE_ROW) return 0;
  int extendedErrCode = sqlite3_extended_errcode(db);
  if (errCode == SQLITE_CONSTRAINT && (extendedErrCode == SQLITE_CONSTRAINT_UNIQUE || extendedErrCode == SQLITE_CONSTRAINT_PRIMARYKEY)) return 0; // not an error, just the entry already exists
  const char *err = sqlite3_errmsg(db);
  if(err) {
    if (strcmp(err, "not an error") != 0)
      esyslog("tvscraper: ERROR query failed: %s , error: %s, error code %i extendedErrCode %i", query, err, errCode, extendedErrCode);
    else return 0;
  } else 
    esyslog("tvscraper: ERROR query failed: %s , no error text, error code %i extendedErrCode %i", query, errCode, extendedErrCode);
  return errCode;
}
int cTVScraperDB::execSql(const string &sql) const {
// return 0 if there was no error
// otherwise, return error code
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
  int errCode = printSqlite3Errmsg(sql.c_str() );
  if (!errCode) {
    sqlite3_step(stmt);
    errCode = printSqlite3Errmsg(sql.c_str() );
  }
  sqlite3_finalize(stmt);
  return errCode;
}

int cTVScraperDB::execSql(const char *query, const char *bind, ...) const {
// return 0 if there was no error
// otherwise, return error code

// bind
//    s: char *
//    i: int
//    d: double
// bind: see prepare_bind comments for complete list of supported values
  sqlite3_stmt *statement;
  va_list vl;
  va_start(vl, bind);
  int errCode = prepare_bind(&statement, query, bind, vl);
  va_end(vl);
  if (statement) sqlite3_step(statement);
  sqlite3_finalize(statement);
  return errCode;
}

vector<vector<string> > cTVScraperDB::Query(const char *query, const char *bind, ...) const {
// bind: see prepare_bind comments for complete list of supported values
  vector<vector<string> > results;
  va_list vl;
  va_start(vl, bind);
  sqlite3_stmt *statement;
  prepare_bind(&statement, query, bind, vl);
  if (statement) {
    int cols = sqlite3_column_count(statement);
    while(sqlite3_step(statement) == SQLITE_ROW) {
      vector<string> values;
      for(int col = 0; col < cols; col++)
        values.push_back(charPointerToString(sqlite3_column_text(statement, col)));
      results.push_back(values);
    }
    printSqlite3Errmsg(query);
    sqlite3_finalize(statement);
  }
  va_end(vl);
  return results;
}

string cTVScraperDB::QueryString(const char *query, const char *bind, ...) const {
// bind: see prepare_bind comments for complete list of supported values
  va_list vl;
  va_start(vl, bind);
  string result;
  if (QueryValue(query, bind, "S", vl, &result) ) return result;
  return "";
}

bool cTVScraperDB::QueryInt(int &result, const char *query, const char *bind, ...) const {
// return true if the requested entry exists in database
// bind: see prepare_bind comments for complete list of supported values
  va_list vl;
  va_start(vl, bind);
  return QueryValue(query, bind, "i", vl, &result);
}

int cTVScraperDB::QueryInt(const char *query, const char *bind, ...) const {
// bind: see prepare_bind comments for complete list of supported values
  va_list vl;
  va_start(vl, bind);
  int result;
  if (QueryValue(query, bind, "i", vl, &result) ) return result;
  return 0;
}

sqlite3_int64 cTVScraperDB::QueryInt64(const char *query, const char *bind, ...) const {
// bind: see prepare_bind comments for complete list of supported values
  va_list vl;
  va_start(vl, bind);
  sqlite3_int64 result;
  if (QueryValue(query, bind, "l", vl, &result) ) return result;
  return 0;
}

bool cTVScraperDB::QueryValue(const char *query, const char *bind, const char *fmt_result, va_list &vl, ...) const {
// read one value to ... (there must be one more parameter)
// return true if a line was found
// va_list &vl: bind parameters
// ... : result parameter (pointer)
// bind & fmt_result:
//    s: const unsigned char * is not allowed for fmt_result as the reference will not be valid any more once this method returns ...
//    i: int
//    d: double
// bind: see prepare_bind comments for complete list of supported values
// fmt_result: see step_read_result comments for complete list of supported values
  if(fmt_result && strchr(fmt_result, 's') != NULL)
    esyslog("tvscraper: ERROR in cTVScraperDB::QueryValue, fmt_result: \"%s\" must not contain s, use S for strings", fmt_result);
  va_list vl_r;
  va_start(vl_r, vl);
  sqlite3_stmt *statement;
  bool lineFound = (prepare_bind(  &statement, query, bind, vl) == SQLITE_OK) &&
                   (step_read_result(statement, fmt_result, vl_r) == SQLITE_ROW); 
  sqlite3_finalize(statement);
  va_end(vl);
  va_end(vl_r);
  return lineFound;
}

bool cTVScraperDB::QueryLine(const char *query, const char *bind, const char *fmt_result, ...) const {
// read one line to parameter list
// return true if a line was found
// va_list &vl: first bind parameters, then pointers to result parameters
// bind & fmt_result:
//    s: const unsigned char * is not allowed for fmt_result as the reference will not be valid any more once this method returns ...
//    S: std::string
//    i: int
//    d: double
// bind: see prepare_bind comments for complete list of supported values
// fmt_result: see step_read_result comments for complete list of supported values
  if(fmt_result && strchr(fmt_result, 's') != NULL)
    esyslog("tvscraper: ERROR in cTVScraperDB::QueryLine, fmt_result: \"%s\" must not contain s, use S for strings", fmt_result);
  va_list vl;
  va_start(vl, fmt_result);
  sqlite3_stmt *statement;
  bool lineFound = (prepare_bind(  &statement, query, bind, vl) == SQLITE_OK) &&
                   (step_read_result(statement, fmt_result, vl) == SQLITE_ROW); 
  sqlite3_finalize(statement);
  va_end(vl);
  return lineFound;
}

bool cTVScraperDB::QueryStep(sqlite3_stmt *&statement, const char *fmt_result, ...) const {
// return true if a row was found. Otherwise false
// before returning false, sqlite3_finalize(statement) is called
// fmt_result: see step_read_result comments for complete list of supported values

  if (!statement) return false;
  va_list vl;
  va_start(vl, fmt_result);
  int OK_CODE = step_read_result(statement, fmt_result, vl);
  va_end(vl);
  if (OK_CODE == SQLITE_ROW) return true;
  sqlite3_finalize(statement);
  statement = NULL;
  return false;
}

int cTVScraperDB::step_read_result(sqlite3_stmt *statement, const char *fmt_result, va_list &vl) const {
// read one line to parameter list
// return SQLITE_ROW if a line was found (return the result of sqlite3_step, or -1 in case of an error)
// va_list &vl: pointers to result parameters
// fmt_result:
//    S: std::string *
//    s: const unsigned char *
//    i: int
//    b: bool
//    l: sqlite3_int64
//    t: time_t
//    d: double
//    f: float
  int result = sqlite3_step(statement);
  printSqlite3Errmsg("cTVScraperDB::step_read_result");
  if (result != SQLITE_ROW) return result;
  int cols = sqlite3_column_count(statement);
  if (strlen(fmt_result) != (size_t)cols) {
    esyslog("tvscraper: ERROR in cTVScraperDB::step_read_result, fmt_result: %s, cols %i", fmt_result, cols);
    return -1;
  }
  std::string *valr_s;
  for (int i=0; fmt_result[i]; i++) {
    switch (fmt_result[i]) {
      case 'S':
        valr_s=static_cast<std::string*>(va_arg(vl,void*) );
        *valr_s = charPointerToString(sqlite3_column_text(statement, i));
        break;
      case 's':
        *(va_arg(vl,const unsigned char **)) = sqlite3_column_text(statement, i);
        break;
      case 'd':
        *(va_arg(vl,double*)) = sqlite3_column_double(statement, i);
        break;
      case 'f':
        *(va_arg(vl,float*)) = (float)sqlite3_column_double(statement, i);
        break;
      case 'i':
        if (sqlite3_column_type(statement, i) == SQLITE_NULL) *(va_arg(vl,int*)) = -1;
        else *(va_arg(vl,int*)) = sqlite3_column_int(statement, i);
        break;
      case 'b':
        *(va_arg(vl,bool*)) = (bool)sqlite3_column_int(statement, i);
        break;
      case 'l':
        *(va_arg(vl,sqlite3_int64*)) = sqlite3_column_int64(statement, i);
        break;
      case 't':
        *(va_arg(vl,time_t*)) = (time_t) sqlite3_column_int64(statement, i);
        break;
      default:
        esyslog("tvscraper: ERROR in cTVScraperDB::step_read_result, fmt_result: %s, unknown %c", fmt_result, fmt_result[i]);
        va_arg(vl, void*);
    }
  }
  return result;
}

sqlite3_stmt *cTVScraperDB::QueryPrepare(const char *query, const char *bind, ...) const {
// starting point to read multipe lines from db
// bind: see prepare_bind comments for complete list of supported values
// example code:
/*
  int tvID;
  for (sqlite3_stmt *statement = db->QueryPrepare("select tv_id from tv2;", "");
       db->QueryStep(statement, "i", &tvID);) {
// code to be executed for each line
  }
*/

  va_list vl;
  va_start(vl, bind);
  sqlite3_stmt *statement;
  prepare_bind(&statement, query, bind, vl);
  va_end(vl);
  return statement;
}

int cTVScraperDB::prepare_bind(sqlite3_stmt **statement, const char *query, const char *bind, va_list &vl) const {
// va_list &vl: bind parameters
// bind:
//    s: char *
//    i: int
//    l: sqlite3_int64
//    t: time_t
//    d: double
//    f: float
  size_t num_q = 0;
  for (const char *q = query; *q; q++) if (*q == '?') num_q++;
  if (num_q != strlen(bind) ) esyslog("tvscraper: ERROR in cTVScraperDB::prepare_bind, query: %s, num_q %i, bind: %s", query, (int)num_q, bind);

  int result = sqlite3_prepare_v2(db, query, -1, statement, 0);
  printSqlite3Errmsg(query);
  if (result != SQLITE_OK) return result;
  for (int i=0; bind[i]; i++) {
    switch (bind[i]) {
      case 's':
        sqlite3_bind_text(*statement, i + 1, va_arg(vl,char*), -1, SQLITE_STATIC);
        break;
      case 'd':
      case 'f':  // float parameters are automatically promoted to double, so they come in here as double ...
        sqlite3_bind_double(*statement, i + 1, va_arg(vl,double));
        break;
      case 'i':
        sqlite3_bind_int(*statement, i + 1, va_arg(vl,int));
        break;
      case 'l':
        sqlite3_bind_int64(*statement, i + 1, va_arg(vl,sqlite3_int64));
        break;
      case 't':
        sqlite3_bind_int64(*statement, i + 1, (sqlite3_int64)va_arg(vl,time_t));
        break;
    }
  }
  return result;
}

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
  return QueryInt("select count(type) from sqlite_master where type= ? and name= ?", "ss", "table", table) == 1;
}

void cTVScraperDB::AddCulumnIfNotExists(const char *table, const char *column, const char *type) {
  if (TableColumnExists(table, column) ) return;
  stringstream sql;
  sql << "ALTER TABLE " << table << " ADD COLUMN " << column << " " << type;
  execSql(sql.str() );
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
/*
select tv_s_e_name.episode_name, tv_s_e.season_number, tv_s_e.episode_number from tv_s_e, tv_s_e_name
   where tv_s_e_name.episode_id = tv_s_e.episode_id and tv_s_e.tv_id = ? and tv_s_e_name.language_id = ?;
num episodes in tv_se
select count(episode_id) from tv_s_e where tv_id = ?;

num episodes in tv_se
select count(episode_id) from tv_s_e, tv_s_e_name
   where tv_s_e_name.episode_id = tv_s_e.episode_id and tv_s_e.tv_id = ? and tv_s_e_name.language_id = ?;
*/

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
      execSql("drop table tv2;", "");
      execSql("drop table tv_s_e;", "");
      execSql("drop table movies3;", "");
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
      execSql(sql.str() );
      execSql("CREATE UNIQUE INDEX IF NOT EXISTS idx_series_actors2 on series_actors2 (actor_series_id, actor_name, actor_role); ");
      execSql("CREATE INDEX IF NOT EXISTS idx12 on series_actors2 (actor_series_id);");

      for (const vector<string> &actor: Query("SELECT actor_series_id, actor_name, actor_role, actor_thumbnail FROM series_actors;", "") ) {
        if (actor.size() != 4) continue;
        stringstream sql;
        sql << "INSERT INTO series_actors2 (actor_series_id, actor_name, actor_role, actor_number) ";
        sql << "VALUES (" << actor[0] << ", ?, ?, ";
        size_t pos;
        if (actor[3].length() > 10 && (pos = actor[3].find(".jpg") ) !=  std::string::npos) {
          sql << actor[3].substr(6, pos - 6);
        } else {
          sql << -1;
        }
        sql << ");";
        execSql(sql.str().c_str(), "ss", actor[1].c_str(), actor[2].c_str());
      }
      execSql("DROP TABLE series_actors;");
      execSql("ALTER TABLE series_actors2 RENAME to series_actors;");
    }
    return true;
}

sqlite3_stmt *cTVScraperDB::GetAllMovies() const {
  return QueryPrepare("select movie_id from movies3;", "");
}

void cTVScraperDB::ClearOutdated() const {
// delete all invalid events pointing to movies
// and delete all invalid events pointing to series
  time_t now = time(0);
  execSql("delete from event where valid_till < ?", "l", (sqlite3_int64)now);
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
  execSql("DELETE FROM movies3 WHERE movie_id = ?", "i", movieID);
  return sqlite3_changes(db);
}

bool cTVScraperDB::CheckMovieOutdatedEvents(int movieID, int season_number, int episode_number) const {
// check if there is still an event for which movieID is required
  const char sql_m[] = "select count(event_id) from event where season_number  = ? and movie_tv_id = ? and valid_till > ?";
  const char sql_t[] = "select count(event_id) from event where season_number != ? and movie_tv_id = ? and valid_till > ?";
  if (season_number == -100) return QueryInt(sql_m, "iit", -100, movieID, time(0) ) > 0;
                else         return QueryInt(sql_t, "iit", -100, movieID, time(0) ) > 0;
}

bool cTVScraperDB::CheckMovieOutdatedRecordings(int movieID, int season_number, int episode_number) const {
// check if there is still a recording for which movieID is required
  const char sql_m[] = "select count(event_id) from recordings2 where season_number  = ? and movie_tv_id = ?";
  const char sql_t[] = "select count(event_id) from recordings2 where season_number != ? and movie_tv_id = ?";
  if (season_number == -100) return QueryInt(sql_m, "ii", -100, movieID) > 0;
                        else return QueryInt(sql_t, "ii", -100, movieID) > 0;
}

int cTVScraperDB::DeleteSeries(int seriesID) const {
// seriesID > 0 => moviedb
// seriesID < 0 => tvdb
// this one only deletes the entry in tv database. No images, ... are deleted
  if (seriesID > 0) {
// delete actor_tv_episode, data are only for moviedb
    execSql("delete from actor_tv_episode where episode_id in ( select episode_id from tv_s_e where tv_s_e.tv_id = ? );",
      "i", seriesID);
  };
// delete tv_s_e
  execSql("DELETE FROM tv_s_e WHERE tv_id = ?", "i", seriesID);
// delete tv
  execSql("DELETE FROM tv2    WHERE tv_id = ?", "i", seriesID);
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

void cTVScraperDB::InsertTv(int tvID, const string &name, const string &originalName, const string &overview, const string &firstAired, const string &networks, const string &genres, float popularity, float vote_average, int vote_count, const string &posterUrl, const string &fanartUrl, const string &IMDB_ID, const string &status, const set<int> &EpisodeRunTimes, const string &createdBy) {
  execSql("INSERT OR REPLACE INTO tv2 (tv_id, tv_name, tv_original_name, tv_overview, tv_first_air_date, tv_networks, tv_genres, tv_popularity, tv_vote_average, tv_vote_count, tv_posterUrl, tv_fanartUrl, tv_IMDB_ID, tv_status, tv_created_by, tv_last_season, tv_number_of_episodes) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, 0);",
    "issssssddisssss", tvID,
    name.c_str(),  originalName.c_str(), overview.c_str(), firstAired.c_str(), networks.c_str(), genres.c_str(),
    (double)popularity, (double)vote_average, vote_count,
    posterUrl.c_str(), fanartUrl.c_str(), IMDB_ID.c_str(), status.c_str(), createdBy.c_str() );

    for (const int &episodeRunTime : EpisodeRunTimes) {
       execSql("INSERT OR REPLACE INTO tv_episode_run_time (tv_id, episode_run_time) VALUES (?, ?);",
         "ii", tvID, episodeRunTime);
    }
    execSql("INSERT OR REPLACE INTO tv_vote (tv_id, tv_vote_average, tv_vote_count) VALUES (?, ?, ?)",
      "idi", tvID, (double)vote_average, vote_count);
    execSql("INSERT OR REPLACE INTO tv_score (tv_id, tv_score) VALUES (?, ?)",
      "id", tvID, (double)popularity);
}

void cTVScraperDB::InsertTvEpisodeRunTimes(int tvID, const set<int> &EpisodeRunTimes) {
  for (const int &episodeRunTime : EpisodeRunTimes) {
     execSql("INSERT OR REPLACE INTO tv_episode_run_time (tv_id, episode_run_time) VALUES (?, ?);",
       "ii", tvID, episodeRunTime);
  }
}

void cTVScraperDB::TvSetEpisodesUpdated(int tvID) {
  execSql("UPDATE tv2 set tv_last_updated = ? where tv_id= ?",
    "li", (sqlite3_int64)time(0), tvID);
}

void cTVScraperDB::TvSetNumberOfEpisodes(int tvID, int LastSeason, int NumberOfEpisodes) {
  execSql("UPDATE tv2 set tv_last_season = ? , tv_number_of_episodes = ? where tv_id= ?",
    "iii", LastSeason, NumberOfEpisodes, tvID);
}

bool cTVScraperDB::TvGetNumberOfEpisodes(int tvID, int &LastSeason, int &NumberOfEpisodes) {
  return QueryLine("select tv_last_season, tv_number_of_episodes from tv2 where tv_id = ?", "i", "ii", tvID, &LastSeason, &NumberOfEpisodes);
}

bool cTVScraperDB::SearchTvEpisode(int tvID, const string &episode_search_name, int &season_number, int &episode_number) {
// return true if an episode match like episode_search_name was found
  return QueryLine("select season_number, episode_number from tv_s_e where tv_id = ? and episode_name like ?",
    "is", "ii", tvID, episode_search_name.c_str(), &season_number, &episode_number);
}

void cTVScraperDB::InsertTv_s_e(int tvID, int season_number, int episode_number, int episode_absolute_number, int episode_id, const string &episode_name, const string &airDate, float vote_average, int vote_count, const string &episode_overview, const string &episode_guest_stars, const string &episode_director, const string &episode_writer, const string &episode_IMDB_ID, const string &episode_still_path, int episode_run_time) {

  execSql("INSERT OR REPLACE INTO tv_s_e (tv_id, season_number, episode_number, episode_absolute_number, episode_id, episode_name, episode_air_date, episode_vote_average, episode_vote_count, episode_overview, episode_guest_stars, episode_director, episode_writer, episode_IMDB_ID, episode_still_path, episode_run_time) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
    "iiiiissdissssssi", tvID, season_number, episode_number, episode_absolute_number, episode_id,
     episode_name.c_str(), airDate.c_str(), (double)vote_average, vote_count,
     episode_overview.c_str(), episode_guest_stars.c_str(), episode_director.c_str(), episode_writer.c_str(),
     episode_IMDB_ID.c_str(), episode_still_path.c_str(), episode_run_time);
}

string cTVScraperDB::GetEpisodeStillPath(int tvID, int seasonNumber, int episodeNumber) const {
  return QueryString("select episode_still_path from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?", "iii", tvID, seasonNumber, episodeNumber);
}

bool cTVScraperDB::episodeNameUpdateRequired(int tvID, int langId) {
  const char *sqld = "select count(episode_id) from tv_s_e where tv_id = ?;";
  const char *sqll = "select count(tv_s_e_name.episode_id) from tv_s_e, tv_s_e_name where tv_s_e_name.episode_id = tv_s_e.episode_id and tv_s_e.tv_id = ? and tv_s_e_name.language_id = ?;";
  int numEpisodesD = QueryInt(sqld, "i", tvID);
  int numEpisodesL = QueryInt(sqll, "ii", tvID, langId);
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
    cString channelIDs = sEventOrRecording->ChannelIDs();
    int runtime = GetRuntime(sEventOrRecording, movie_tv_id, season_number, episode_number);
    if (!(const char *)channelIDs) esyslog("tvscraper: ERROR in cTVScraperDB::InsertEvent, !channelIDs");
    execSql("INSERT OR REPLACE INTO event (event_id, channel_id, valid_till, runtime, movie_tv_id, season_number, episode_number) VALUES (?, ?, ?, ?, ?, ?, ?);",
      "isliiii", (int)eventID, (const char *)channelIDs, (sqlite3_int64)validTill, runtime, movie_tv_id, season_number, episode_number);
}

void cTVScraperDB::InsertMovie(int movieID, const string &title, const string &original_title, const string &tagline, const string &overview, bool adult, int collection_id, const string &collection_name, int budget, int revenue, const string &genres, const string &homepage, const string &release_date, int runtime, float popularity, float vote_average, int vote_count, const string &productionCountries, const string &posterUrl, const string &fanartUrl, const string &IMDB_ID){

  execSql("INSERT OR REPLACE INTO movies3 (movie_id, movie_title, movie_original_title, movie_tagline, movie_overview, movie_adult, movie_collection_id, movie_collection_name, movie_budget, movie_revenue, movie_genres, movie_homepage, movie_release_date, movie_runtime, movie_popularity, movie_vote_average, movie_vote_count, movie_production_countries, movie_posterUrl, movie_fanartUrl, movie_IMDB_ID) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
    "issssiisiisssiddissss", movieID, title.c_str(), original_title.c_str(), tagline.c_str(), overview.c_str(),
    (int)adult, collection_id, collection_name.c_str(), budget, revenue,
    genres.c_str(), homepage.c_str(), release_date.c_str(), runtime, (double)popularity, (double)vote_average,
    vote_count, productionCountries.c_str(), posterUrl.c_str(), fanartUrl.c_str(), IMDB_ID.c_str() );
// extra table for runtime & tagline, will not be deleted
  if (runtime == 0) runtime = -1; // -1 : no runtime available in themoviedb
  execSql("INSERT OR REPLACE INTO movie_runtime2 (movie_id, movie_runtime, movie_tagline) VALUES (?, ?, ?);",
    "iis", movieID, runtime, tagline.c_str() );
}

void cTVScraperDB::InsertMovieDirectorWriter(int movieID, const string &director, const string &writer) {
  execSql("UPDATE movie_runtime2 SET movie_director = ?, movie_writer = ? WHERE movie_id = ?",
   "ssi", director.c_str(), writer.c_str(), movieID);
}

string cTVScraperDB::GetMovieTagline(int movieID) {
  return QueryString("select movie_tagline from movie_runtime2 where movie_id = ?", "i", movieID);
}

int cTVScraperDB::GetMovieRuntime(int movieID) const {
// -1 : no runtime available in themoviedb
//  0 : themoviedb never checked for runtime
    return QueryInt("select movie_runtime from movie_runtime2 where movie_id = ?", "i", movieID);
}

vector<vector<string> > cTVScraperDB::GetTvRuntimes(int tvID) const {
  return Query("select episode_run_time from tv_episode_run_time where tv_id = ?", "i", tvID);
}

void cTVScraperDB::InsertMovieActor(int movieID, int actorID, const string &name, const string &role, bool hasImage) {
  if (!hasImage) {
    int hImage = QueryInt("select actor_has_image from actors where actor_id = ?", "i", actorID);
// hImage == -1 => information unknown
// hImage == 0  => no image
// hImage == 1  => image
    if (hImage > 0) hasImage = true;
  }
  execSql("INSERT OR REPLACE INTO actors (actor_id, actor_name, actor_has_image) VALUES (?, ?, ?);",
    "isi", actorID, name.c_str(), (int)hasImage );

  execSql("INSERT OR REPLACE INTO actor_movie (actor_id, movie_id, actor_role) VALUES (?, ?, ?);",
    "iis", actorID, movieID, role.c_str() );
}

void cTVScraperDB::InsertTvActor(int tvID, int actorID, const string &name, const string &role, bool hasImage) {
  execSql("INSERT OR REPLACE INTO actors (actor_id, actor_name, actor_has_image) VALUES (?, ?, ?);",
    "isi", actorID, name.c_str(), (int)hasImage );

  execSql("INSERT OR REPLACE INTO actor_tv (tv_id, actor_id, actor_role) VALUES (?, ?, ?);",
    "iis", tvID, actorID, role.c_str() );
}

void cTVScraperDB::InsertTvEpisodeActor(int episodeID, int actorID, const string &name, const string &role, bool hasImage) {
  if (!hasImage) {
    int hImage = QueryInt("select actor_has_image from actors where actor_id = ?", "i", actorID);
// hImage == -1 => information unknown
// hImage == 0  => no image
// hImage == 1  => image
    if (hImage > 0) hasImage = true;
  }
  execSql("INSERT OR REPLACE INTO actors (actor_id, actor_name, actor_has_image) VALUES (?, ?, ?)",
    "isi", actorID, name.c_str(), (int)hasImage );

  execSql("INSERT OR REPLACE INTO actor_tv_episode (episode_id, actor_id, actor_role) VALUES (?, ?, ?);",
    "iis", episodeID, actorID, role.c_str() );
}

void cTVScraperDB::InsertActor(int seriesID, const string &name, const string &role, const string &path) {
  bool debug = seriesID == 78804;
  debug = false;
  const char sql_an[] = "select actor_number from series_actors where actor_series_id = ? and actor_name = ? and actor_role = ?";
  int actorNumber;
  if (QueryInt(actorNumber, sql_an, "iss", seriesID, name.c_str(), role.c_str())) {
// entry already in db
    if (actorNumber >= 0 && !path.empty() ) {
      stringstream actorPath;
      actorPath << config.GetBaseDirSeries() << seriesID << "/actor_" << actorNumber << ".jpg";
      if (!FileExists(actorPath.str() )) AddActorDownload(seriesID * -1, false, actorNumber, path);
    }
    if (actorNumber == -1 && !path.empty() ) {
      actorNumber = findUnusedActorNumber(seriesID);
      if (debug) esyslog("tvscraper: InsertActor, update, actorNumber = %i, actor_name = %s, actor_role = %s", actorNumber, name.c_str(), role.c_str() );
      const char sql_un[] = "update series_actors set actor_number = ? where actor_series_id = ? and actor_name = ? and actor_role = ?";
      execSql(sql_un, "iiss", actorNumber, seriesID, name.c_str(), role.c_str());
      AddActorDownload(seriesID * -1, false, actorNumber, path);
    }
  } else {
// no entry in db
  if (path.empty() ) actorNumber = -1;
  else actorNumber = findUnusedActorNumber(seriesID);
  if (debug) esyslog("tvscraper: InsertActor, new, actorNumber = %i, actor_name = %s, actor_role = %s", actorNumber, name.c_str(), role.c_str() );
  execSql("INSERT INTO series_actors (actor_series_id, actor_name, actor_role, actor_number) VALUES (?, ?, ?, ?);",
    "issi", seriesID, name.c_str(), role.c_str(), actorNumber);
  if (actorNumber != -1) AddActorDownload(seriesID * -1, false, actorNumber, path);
  }
}

bool cTVScraperDB::MovieExists(int movieID) {
  return QueryInt("select count(movie_id) as found from movies3 where movie_id = ?", "i", movieID) == 1;
}

bool cTVScraperDB::TvExists(int tvID) {
  return QueryInt("select count(tv_id) as found from tv2 where tv_id = ?", "i", tvID) == 1;
}

int cTVScraperDB::GetRuntime(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number) {
// return runtime from internat database, which matches best the runtime of the current event
if (season_number == -100) return GetMovieRuntime(movie_tv_id);
  char sql[] = "select episode_run_time from tv_episode_run_time where tv_id = ?";
  if (QueryInt("select count(episode_run_time) from tv_episode_run_time where tv_id = ?", "i", movie_tv_id) <= 1)
    return QueryInt(sql, "i", movie_tv_id);
// tv show, more than one runtime is available. Select the best fitting one
  int runtime_distance = 20000;
  int best_runtime = 0;
  int runtime;
  for (sqlite3_stmt *statement = QueryPrepare(sql, "i", movie_tv_id); QueryStep(statement, "i", &runtime);) {
    int dist = sEventOrRecording->DurationDistance(runtime);
    if (dist < runtime_distance) {
      best_runtime = runtime;
      dist = runtime_distance;
    }
  }
  return best_runtime;
}

void cTVScraperDB::InsertRecording2(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number) {
    tEventID eventID = sEventOrRecording->EventID();
    time_t eventStartTime = sEventOrRecording->StartTime();
    cString channelIDs = sEventOrRecording->ChannelIDs();
    int runtime = GetRuntime(sEventOrRecording, movie_tv_id, season_number, episode_number);
    if (!(const char *)channelIDs) esyslog("tvscraper: ERROR in cTVScraperDB::InsertRecording2, !channelIDs");
  execSql("INSERT OR REPLACE INTO recordings2 (event_id, event_start_time, channel_id, runtime, movie_tv_id, season_number, episode_number) VALUES (?, ?, ?, ?, ?, ?, ?)",
    "ilsiiii", (int)eventID, (sqlite3_int64)eventStartTime, (const char *)channelIDs, runtime,
               movie_tv_id, season_number, episode_number);

    const cRecording *recording = sEventOrRecording->Recording();
    if (recording)
      WriteRecordingInfo(sEventOrRecording->Recording(), movie_tv_id, season_number, episode_number);
// copy images to recording folder will be done later, with cMovieOrTv->copyImagesToRecordingFolder(
// required as InsertRecording2 is called before the images are downloaded
}

void cTVScraperDB::WriteRecordingInfo(const cRecording *recording, int movie_tv_id, int season_number, int episode_number) {
if (!recording || !recording->FileName() ) return;  // no place to write the information
string filename = recording->FileName();
filename.append("/tvscrapper.json");
// get "root" json (this is *jInfo)
json_t *jInfo;
if (std::filesystem::exists(filename) ) {
// read existing json file
  json_error_t error;
  jInfo = json_load_file(filename.c_str(), 0, &error);
  if (!jInfo) {
    esyslog("tvscraper: ERROR cannot load json \"%s\", error \"%s\"", filename.c_str(), error.text);
// this file will be overwritten, so create a copy
    std::error_code ec;
    std::filesystem::copy_file(filename, filename + ".bak", ec);
    if (ec.value() != 0) esyslog("tvscraper: ERROR \"%s\", code %i  tried to copy \"%s\" to \"%s.bak\"", ec.message().c_str(), ec.value(), filename.c_str(), filename.c_str() );
    jInfo = json_object();
  }
} else jInfo = json_object();
// "themoviedb / thetvdb" node: this is owned by "tvscraper", so we can overwrite (if it exists)
json_t *jTvscraper = json_object();
if (movie_tv_id > 0) { json_object_set(jInfo, "themoviedb", jTvscraper); json_object_del(jInfo, "thetvdb"); }
                else { json_object_set(jInfo, "thetvdb", jTvscraper);    json_object_del(jInfo, "themoviedb"); }
// set attributes
const char sql_tv[] = "select tv_name, tv_first_air_date from tv2 where tv_id = ?";
const char sql_mv[] = "select movie_title, movie_release_date from movies3 where movie_id = ?";
const char *sql;
if (season_number != -100) {
// TV Show
  json_object_set_new(jTvscraper, "type", json_string("tv show"));
  sql = sql_tv;
  if( season_number != 0 || episode_number != 0) {  // season / episode was found
    json_object_set_new(jTvscraper, "season_number", json_integer(season_number) );
    json_object_set_new(jTvscraper, "episode_number", json_integer(episode_number) );
// get episode name
    string episode_name = QueryString("select episode_name from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?", "iii", movie_tv_id, season_number, episode_number);
    if (!episode_name.empty() ) json_object_set_new(jTvscraper, "episode_name", json_string(episode_name.c_str()) );
}} else {
// movie
  json_object_set_new(jTvscraper, "type", json_string("movie"));
  sql = sql_mv;
}
string title;
string date;
if (QueryLine(sql, "i", "SS", movie_tv_id, &title, &date ) ) {
  json_object_set_new(jTvscraper, "name", json_string(title.c_str()) );
  if (date.length() >= 4)
    json_object_set_new(jTvscraper, "year", json_integer(atoi(date.substr(0, 4).c_str()) ));
}
json_object_set_new(jTvscraper, "movie_tv_id", json_integer(abs(movie_tv_id) ));

// write file
json_dump_file(jInfo, filename.c_str(), JSON_INDENT(2));
json_decref(jInfo);
json_decref(jTvscraper);
}

bool cTVScraperDB::SetRecording(csEventOrRecording *sEventOrRecording) {
// used to be an event, is now a recording
// only called in workers, if a timer is recording
// event is attached to timer
  stringstream sql;
  tEventID eventID = sEventOrRecording->EventID();
  cString channelIDs = sEventOrRecording->ChannelIDs();
  if (!(const char *)channelIDs) esyslog("tvscraper: ERROR in cTVScraperDB::SetRecording, !channelIDs");
  sql << "select movie_tv_id, season_number, episode_number from event where event_id = " << eventID;
  sql << " and channel_id = ?";
  int movieTvId;
  int seasonNumber;
  int episodeNumber;
  if (QueryLine(sql.str().c_str(), "s", "iii", (const char *)channelIDs, &movieTvId, &seasonNumber, &episodeNumber)) {
    InsertRecording2(sEventOrRecording, movieTvId, seasonNumber, episodeNumber);
      return true;
    }
    return false;
}

void cTVScraperDB::ClearRecordings2(void) {
    execSql("DELETE FROM recordings2 where 0 = 0");
}

bool cTVScraperDB::CheckStartScrapping(int minimumDistance) {
    bool startScrapping = false;
    time_t now = time(0);
    time_t last_scrapped = QueryInt64("select last_scrapped from scrap_checker", "");
    char sql[] =  "INSERT INTO scrap_checker (last_scrapped) VALUES (?)";
    if (last_scrapped) {
      int difference = (int)(now - last_scrapped);
      if (difference > minimumDistance) {
	startScrapping = true;
	execSql("delete from scrap_checker");
	execSql(sql, "t", now);
      }
    } else {
      startScrapping = true;
      execSql(sql, "t", now);
    }
    return startScrapping;
}

bool cTVScraperDB::GetMovieTvID(csEventOrRecording *sEventOrRecording, int &movie_tv_id, int &season_number, int &episode_number, int *runtime) const {
  tEventID eventID = sEventOrRecording->EventID();
  time_t eventStartTime = sEventOrRecording->StartTime();
  cString channelIDs = sEventOrRecording->ChannelIDs();
  if (!(const char *)channelIDs) esyslog("tvscraper: ERROR in cTVScraperDB::GetMovieTvID, !channelIDs");
  stringstream sql;
  if (runtime) sql << "select runtime, movie_tv_id, season_number, episode_number ";
  else         sql << "select          movie_tv_id, season_number, episode_number ";
  if (!sEventOrRecording->Recording() ){
    sql << "from event where event_id = " << eventID;
    sql << " and channel_id = ?";
  } else {
    sql << "from recordings2 where event_id = " << eventID;
    sql << " and event_start_time = " << eventStartTime;
    sql << " and channel_id = ?";
  }
  if (runtime)
    return QueryLine(sql.str().c_str(), "s", "iiii",
      (const char *)channelIDs, runtime, &movie_tv_id, &season_number, &episode_number);
  else
    return QueryLine(sql.str().c_str(), "s", "iii",
      (const char *)channelIDs,          &movie_tv_id, &season_number, &episode_number);
}

bool cTVScraperDB::GetMovieTvID(const cRecording *recording, int &movie_tv_id, int &season_number, int &episode_number) const {
  if (!recording || !recording->Info() || !recording->Info()->GetEvent()) return false;
  csRecording sRecording(recording);
  int eventID = sRecording.EventID();
  time_t eventStartTime = sRecording.StartTime();
  cString channelIDs = sRecording.ChannelIDs();
  if (!(const char *)channelIDs) esyslog("tvscraper: ERROR in cTVScraperDB::GetMovieTvID (recording), !channelIDs");
  const char *sql = "select movie_tv_id, season_number, episode_number from recordings2 where event_id = ? and event_start_time = ? and channel_id = ?";
  return QueryLine(sql, "its", "iii", eventID, eventStartTime, (const char *)channelIDs, &movie_tv_id, &season_number, &episode_number);
}

bool cTVScraperDB::GetMovieTvID(const cEvent *event, int &movie_tv_id, int &season_number, int &episode_number) const {
  if (!event) return false;
  int eventID = event->EventID();
  cString channelIDs = event->ChannelID().ToString();
  if (!(const char *)channelIDs) esyslog("tvscraper: ERROR in cTVScraperDB::GetMovieTvID (event), !channelIDs");
  const char *sql = "select movie_tv_id, season_number, episode_number from event where event_id = ? and channel_id = ?";
  return QueryLine(sql, "is", "iii", eventID, (const char *)channelIDs, &movie_tv_id, &season_number, &episode_number);
}

vector<vector<string> > cTVScraperDB::GetActorsMovie(int movieID) {
  return Query("select actors.actor_id, actor_name, actor_role from actors, actor_movie where actor_movie.actor_id = actors.actor_id and actor_movie.movie_id = ?",
    "i", movieID);
}

vector<vector<string> > cTVScraperDB::GetActorsSeries(int seriesID) {
  return Query("SELECT actor_number, actor_name, actor_role FROM series_actors WHERE actor_series_id = ?",
    "i", seriesID);
}

std::string cTVScraperDB::GetEpisodeName(int tvID, int seasonNumber, int episodeNumber) const {
  if (seasonNumber == 0 && episodeNumber == 0) return "";
  return QueryString("select episode_name from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?", "iii", tvID, seasonNumber, episodeNumber);
}

vector<vector<string> > cTVScraperDB::GetGuestActorsTv(int episodeID) {
  return Query("select actors.actor_id, actor_name, actor_role from actors, actor_tv_episode where actor_tv_episode.actor_id = actors.actor_id and actor_tv_episode.episode_id = ?",
    "i", episodeID);
}

vector<vector<string> > cTVScraperDB::GetActorsTv(int tvID) {
// read tv actors (cast)
  return Query("select actors.actor_id, actor_name, actor_role from actors, actor_tv where actor_tv.actor_id = actors.actor_id and actor_tv.tv_id = ?",
     "i", tvID);
}

string cTVScraperDB::GetDescriptionTv(int tvID) {
  return QueryString("select tv_overview from tv2 where tv_id = ?", "i", tvID);
}

string cTVScraperDB::GetDescriptionTv(int tvID, int seasonNumber, int episodeNumber) {
  return QueryString("select episode_overview from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?", "iii", tvID, seasonNumber, episodeNumber);
}

int cTVScraperDB::GetMovieCollectionID(int movieID) const {
  return QueryInt("select movie_collection_id from movies3 where movie_id = ?", "i", movieID);
}

bool cTVScraperDB::GetMovie(int movieID, string &title, string &original_title, string &tagline, string &overview, bool &adult, int &collection_id, string &collection_name, int &budget, int &revenue, string &genres, string &homepage, string &release_date, int &runtime, float &popularity, float &vote_average) {
  return QueryLine("select movie_title, movie_original_title, movie_tagline, movie_overview, movie_adult, movie_collection_id, movie_collection_name, movie_budget, movie_revenue, movie_genres, movie_homepage, movie_release_date, movie_runtime, movie_popularity, movie_vote_average from movies3 where movie_id = ?",
      "i", "SSSSbiSiiSSSiff", movieID,
       &title, &original_title, &tagline, &overview, &adult, &collection_id, &collection_name,
       &budget, &revenue, &genres, &homepage, &release_date, &runtime, &popularity, &vote_average);
}

bool cTVScraperDB::GetTv(int tvID, string &name, string &overview, string &firstAired, string &networks, string &genres, float &popularity, float &vote_average, string &status) {
  return QueryLine("select tv_name, tv_overview, tv_first_air_date, tv_networks, tv_genres, tv_popularity, tv_vote_average, tv_status from tv2 where tv_id = ?",
     "i", "SSSSSffS", tvID, 
     &name, &overview, &firstAired, &networks, &genres, &popularity, &vote_average, &status);
}

/*
bool cTVScraperDB::GetTvVote(int tvID, float &vote_average, int &vote_count) {
  return QueryLine("select tv_vote_average, tv_vote_count from tv_vote where tv_id = ?",
     "i", "fi", tvID, &vote_average, &vote_count);
}
*/

bool cTVScraperDB::GetTv(int tvID, time_t &lastUpdated, string &status) {
  return QueryLine("select tv_last_updated, tv_status from tv2 where tv_id = ?",
    "i", "tS", tvID, &lastUpdated, &status);
}

bool cTVScraperDB::GetTvEpisode(int tvID, int seasonNumber, int episodeNumber, int &episodeID, string &name, string &airDate, float &vote_average, string &overview, string &episodeGuestStars) {
  return QueryLine("select episode_id, episode_name, episode_air_date, episode_vote_average, episode_overview, episode_guest_stars from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?",
    "iii", "iSSfSS", tvID, seasonNumber, episodeNumber, 
    &episodeID, &name, &airDate, &vote_average, &overview, &episodeGuestStars);
}

string cTVScraperDB::GetDescriptionMovie(int movieID) {
  return QueryString("select movie_overview from movies3 where movie_id = ?", "i", movieID);
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
    const char sql[] = "select year, episode_search_with_shorttext, movie_tv_id, season_number, episode_number " \
                       "from cache WHERE movie_name_cache = ? AND recording = ? and duration BETWEEN ? and ? " \
                       "and year != ?";
    for (sqlite3_stmt *statement = QueryPrepare(sql,
        "siiii", movieNameCache.c_str(), recording, durationInSec - 300, durationInSec + 300, 0);
          QueryStep(statement, "iiiii", &movieOrTv.year, &movieOrTv.episodeSearchWithShorttext,
                    &movieOrTv.id, &movieOrTv.season, &movieOrTv.episode);) {
// there was a year match. Check: do have a year match?
      vector<int> years;
      AddYears(years, movieNameCache.c_str() );
      sEventOrRecording->AddYears(years);
      if (find(years.begin(), years.end(), movieOrTv.year) != years.end() ) {
        if      (movieOrTv.id     == 0)    movieOrTv.type = scrapNone;
        else if (movieOrTv.season == -100) movieOrTv.type = scrapMovie;
        else                               movieOrTv.type = scrapSeries;
        sqlite3_finalize(statement);
        return true;
      }
    }
// search for cache entry without year match
    const char sql1[] = "select episode_search_with_shorttext, movie_tv_id, season_number, episode_number " \
                        "from cache WHERE movie_name_cache = ? AND recording = ? and duration BETWEEN ? and ? " \
                        "and year = ?";
    if (QueryLine(sql1, "siiii", "iiii",
          movieNameCache.c_str(), recording, durationInSec - 300, durationInSec + 300, 0,
          &movieOrTv.episodeSearchWithShorttext, &movieOrTv.id, &movieOrTv.season, &movieOrTv.episode)) {
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
    time_t now = time(0);
    int recording = 0;
    if (sEventOrRecording->Recording() ) {
      recording = 1;
      if (baseNameEquShortText) recording = 3;
    }
    execSql("INSERT OR REPLACE INTO cache (movie_name_cache, recording, duration, year, episode_search_with_shorttext, movie_tv_id, season_number, episode_number, cache_entry_created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
      "siiiiiiit", movieNameCache.c_str(), recording, sEventOrRecording->DurationInSec(),
      (movieOrTv.id?movieOrTv.year:0), movieOrTv.episodeSearchWithShorttext, movieOrTv.id,
      movieOrTv.season, movieOrTv.episode, now);
}
void cTVScraperDB::DeleteOutdatedCache() const {
    time_t outdated = time(0) - 2*7*24*60*60;  // entries older than two weeks
    execSql("delete from cache where cache_entry_created_at < ?", "l", (sqlite3_int64)outdated);
}
int cTVScraperDB::DeleteFromCache(const char *movieNameCache) { // return number of deleted entries
  if (!movieNameCache) return 0;
  if (strcmp(movieNameCache, "ALL") == 0) execSql("delete from cache");
  else execSql("delete from cache where movie_name_cache = ?", "s", movieNameCache);
  return sqlite3_changes(db);
}

vector<vector<string> > cTVScraperDB::GetTvMedia(int tvID, eMediaType mediaType, bool movie) {
// return media_path, media_number
// if mediaType == season_poster, media_number is the season
// otherwise, it numbers the media (0, 1, ...)
// for movies, media_number is < 0
  stringstream sql;
  sql << "select media_path, media_number from tv_media where tv_id = ? and media_type = ?";
  if (movie) sql << " and media_number <  0";
    else     sql << " and media_number >= 0";
  return Query(sql.str().c_str(), "ii", tvID, (int)mediaType);
}

void cTVScraperDB::insertTvMedia (int tvID, const string &path, eMediaType mediaType) {
// only for poster || fanart || banner
  if (existsTvMedia (tvID, path) ) return;
  int num = QueryInt("select count(tv_id) as found from tv_media where tv_id = ? and media_type = ?",
    "ii", tvID, (int)mediaType);
  
  execSql("INSERT INTO tv_media (tv_id, media_path, media_type, media_number) VALUES (?, ?, ?, ?);",
    "isii", tvID, path.c_str(), (int)mediaType, num);

// update path to poster and fanart in tv
  if (num == 0 && tvID < 0) {
// this is the first entry for thetvdb media
    if (mediaType == mediaPoster)
      execSql("UPDATE tv2 set tv_posterUrl = ? where tv_id= ?", "si", path.c_str(), tvID);
    if (mediaType == mediaFanart)
      execSql("UPDATE tv2 set tv_fanartUrl = ? where tv_id= ?", "si", path.c_str(), tvID);
  }
}

void cTVScraperDB::insertTvMediaSeasonPoster (int tvID, const string &path, eMediaType mediaType, int season) {
// only for poster || fanart || banner
  if (existsTvMedia (tvID, path) ) return;
  
  execSql("INSERT INTO tv_media (tv_id, media_path, media_type, media_number) VALUES (?, ?, ?, ?);",
    "isii", tvID, path.c_str(), (int)mediaType, season);
}

bool cTVScraperDB::existsTvMedia (int tvID, const string &path) {
  return QueryInt("select count(tv_id) as found from tv_media where tv_id = ? and media_path = ?",
    "is", tvID, path.c_str() ) > 0;
}

void cTVScraperDB::deleteTvMedia (int tvID, bool movie, bool keepSeasonPoster) const {
  stringstream sql;
  sql << "delete from tv_media where tv_id = " << tvID;
  if (movie) sql << " and media_number <  0";
    else  {  sql << " and media_number >= 0";
      if(keepSeasonPoster) sql << " and media_type != "  << mediaSeason;
    }
  execSql(sql.str() );
}

int cTVScraperDB::findUnusedActorNumber (int seriesID) {
// 1. create set of existing numbers (numbers)
  const char sql[] = "select actor_number from series_actors where actor_series_id = ?";
  int actorNumber;
  std::set<int> numbers;
  for (sqlite3_stmt *stmt = QueryPrepare(sql, "i", seriesID); QueryStep(stmt, "i", &actorNumber);) numbers.insert(actorNumber);
// 2. first number not in set of existing numbers (numbers)
  if (numbers.empty() ) return 0;
  return *(numbers.rbegin()) + 1;
//  for (actorNumber = 0; numbers.find(actorNumber) !=  numbers.end(); actorNumber++);
//  return actorNumber;
}

void cTVScraperDB::AddActorDownload (int tvID, bool movie, int actorId, const string &actorPath) {
  if (actorPath.empty() ) return;
  execSql("INSERT INTO actor_download (movie_id, is_movie, actor_id, actor_path) VALUES (?, ?, ?, ?);",
    "iiis", tvID, int(movie), actorId, actorPath.c_str() );
}

vector<vector<string> > cTVScraperDB::GetActorDownload(int tvID, bool movie) {
// return actor_id, actor_path
  return Query("select actor_id, actor_path from actor_download where movie_id = ? and is_movie = ?",
    "ii", tvID, (int)movie );
}

void cTVScraperDB::DeleteActorDownload (int tvID, bool movie) const {
  stringstream sql;
  sql << "delete from actor_download where movie_id = " << tvID << " and is_movie = " << int(movie);
  execSql(sql.str() );
}

vector<int> cTVScraperDB::getSimilarTvShows(int tv_id) const {
  char sql[] = "select equal_id from tv_similar where tv_id = ?";
  int equalId = QueryInt(sql, "i", tv_id);
  if (equalId == 0) return {tv_id};
  vector<int> result;
  int id;
  char sql2[] = "select tv_id from tv_similar where equal_id = ?";
  for (sqlite3_stmt *stmt = QueryPrepare(sql2, "i", equalId); QueryStep(stmt, "i", &id);) result.push_back(id);
  return result;
}
void cTVScraperDB::setSimilar(int tv_id1, int tv_id2) {
  char sqlInList[] = "select equal_id from tv_similar where tv_id = ?";
  int equalId1 = QueryInt(sqlInList, "i", tv_id1);
  int equalId2 = QueryInt(sqlInList, "i", tv_id2);
  char sqlInsert[] = "insert INTO tv_similar (tv_id, equal_id) VALUES (?, ?);";
  if (equalId1 == 0 && equalId2 == 0) {
    int equalId = QueryInt("select max(equal_id) from tv_similar;", "");
    if (equalId <= 0) equalId = 1;
    else equalId++;
    execSql(sqlInsert, "ii", tv_id1, equalId);
    execSql(sqlInsert, "ii", tv_id2, equalId);
    return;
  }
  if (equalId1 == equalId2) return; // nothing to do, already marked as equal
  if (equalId1 != 0 && equalId2 == 0) { execSql(sqlInsert, "ii", tv_id2, equalId1); return; }
  if (equalId1 == 0 && equalId2 != 0) { execSql(sqlInsert, "ii", tv_id1, equalId2); return; }
// both ar in the table, but with different IDs
  execSql("UPDATE tv_similar set equal_id = ?  WHERE equal_id = ?", "ii", equalId1, equalId2);
}

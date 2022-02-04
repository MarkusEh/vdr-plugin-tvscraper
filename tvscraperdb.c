#include <string>
#include <sstream>
#include <vector>
#include <sqlite3.h>
#include <jansson.h>
#include <filesystem>
#include "tvscraperdb.h"
#include "services.h"
#include "imageserver.h"


using namespace std;

cTVScraperDB::cTVScraperDB(void) {
    db = NULL;
    string memHD = "/dev/shm/";
    inMem = CheckDirExists(memHD.c_str());
    if (inMem) {
        stringstream sstrDbFileMem;
        sstrDbFileMem << memHD << "tvscraper2.db";
        dbPathMem = sstrDbFileMem.str();
    }
    stringstream sstrDbFile;
    sstrDbFile << config.GetBaseDir() << "/tvscraper2.db";
    dbPathPhys = sstrDbFile.str();
}

cTVScraperDB::~cTVScraperDB() {
    sqlite3_close(db);
}

int cTVScraperDB::printSqlite3Errmsg(const string &query) {
// return 0 if there was no error
// otherwise, return error code
    int errCode = sqlite3_errcode(db);
    if (errCode == SQLITE_OK || errCode == SQLITE_DONE || errCode == SQLITE_ROW) return 0;
    int extendedErrCode = sqlite3_extended_errcode(db);
    if (errCode == SQLITE_CONSTRAINT && (extendedErrCode == SQLITE_CONSTRAINT_UNIQUE || extendedErrCode == SQLITE_CONSTRAINT_PRIMARYKEY)) return 0; // not an error, just the entry already exists
    const char *err = sqlite3_errmsg(db);
    if(err) {
      string error = err;
      if(error.compare("not an error") != 0) {
          esyslog("tvscraper: ERROR query failed: %s , error: %s, error code %i extendedErrCode %i", query.c_str(), error.c_str(), errCode, extendedErrCode);
      }
    }
    return errCode;
}
int cTVScraperDB::execSql(const string &sql) {
// return 0 if there was no error
// otherwise, return error code
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
  int errCode = printSqlite3Errmsg(sql);
  if (!errCode) {
    sqlite3_step(stmt);
    errCode = printSqlite3Errmsg(sql);
  }
  sqlite3_finalize(stmt);
  return errCode;
}

int cTVScraperDB::execSqlBind(const string &sql, const string &value) {
// return 0 if there was no error
// otherwise, return error code
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
  int errCode = printSqlite3Errmsg(sql);
  if (!errCode) {
    sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    errCode = printSqlite3Errmsg(sql);
  }
  sqlite3_finalize(stmt);
  return errCode;
}

vector<vector<string> > cTVScraperDB::Query(const string &query) {
    sqlite3_stmt *statement;
    vector<vector<string> > results;
    if(sqlite3_prepare_v2(db, query.c_str(), -1, &statement, 0) == SQLITE_OK) {
        int cols = sqlite3_column_count(statement);
        int result = 0;
        while(true) {
            result = sqlite3_step(statement);
            if(result == SQLITE_ROW) {
                vector<string> values;
                for(int col = 0; col < cols; col++) {
                    values.push_back(charPointerToString(sqlite3_column_text(statement, col)));
                }
                results.push_back(values);
            } else {
                break;
            }
        }
        sqlite3_finalize(statement);
    }
    printSqlite3Errmsg(query);
    return results;
}

bool cTVScraperDB::QueryLine(vector<string> &result, const string &query) {
// read one line to result
// return true if a line was found
    result.clear();
    bool lineFound = false;
    sqlite3_stmt *statement;
    if(sqlite3_prepare_v2(db, query.c_str(), -1, &statement, 0) == SQLITE_OK) {
        int cols = sqlite3_column_count(statement);
        if( sqlite3_step(statement) == SQLITE_ROW) {
           lineFound = true;
           for(int col = 0; col < cols; col++) {
              result.push_back(charPointerToString(sqlite3_column_text(statement, col)));
           }
        }
        sqlite3_finalize(statement);
    }
    printSqlite3Errmsg(query);
    return lineFound;
}

int cTVScraperDB::QueryInt(const string &query) {
    int result = 0;
    sqlite3_stmt *statement;
    if(sqlite3_prepare_v2(db, query.c_str(), -1, &statement, 0) == SQLITE_OK) {
        int cols = sqlite3_column_count(statement);
        if (sqlite3_step(statement) == SQLITE_ROW && cols > 0) result = sqlite3_column_int(statement, 0);
        sqlite3_finalize(statement);
    }
    printSqlite3Errmsg(query);
    return result;
}

sqlite3_int64 cTVScraperDB::QueryInt64(const string &query) {
    sqlite3_int64 result = 0;
    sqlite3_stmt *statement;
    if(sqlite3_prepare_v2(db, query.c_str(), -1, &statement, 0) == SQLITE_OK) {
        int cols = sqlite3_column_count(statement);
        if (sqlite3_step(statement) == SQLITE_ROW && cols > 0) result = sqlite3_column_int64(statement, 0);
        sqlite3_finalize(statement);
    }
    printSqlite3Errmsg(query);
    return result;
}

bool cTVScraperDB::QueryValue(string &result, const string &query) {
    result.clear();
    bool lineFound = false;
    sqlite3_stmt *statement;
    if(sqlite3_prepare_v2(db, query.c_str(), -1, &statement, 0) == SQLITE_OK) {
        int cols = sqlite3_column_count(statement);
        if (sqlite3_step(statement) == SQLITE_ROW) {
          lineFound = true;
          if (cols > 0) result = charPointerToString(sqlite3_column_text(statement, 0));
        }
        sqlite3_finalize(statement);
    }
    printSqlite3Errmsg(query);
    return lineFound;
}

vector<vector<string> > cTVScraperDB::QueryEscaped(string query, string where) {
    sqlite3_stmt *statement;
    vector<vector<string> > results;
    if(sqlite3_prepare_v2(db, query.c_str(), -1, &statement, 0) == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, where.c_str(), -1, SQLITE_TRANSIENT);
        int cols = sqlite3_column_count(statement);
        int result = 0;
        while(true) {
            result = sqlite3_step(statement);
            if(result == SQLITE_ROW) {
                vector<string> values;
                for(int col = 0; col < cols; col++) {
                    values.push_back(charPointerToString(sqlite3_column_text(statement, col)));
                }
                results.push_back(values);
            } else {
                break;  
            }
        }
        sqlite3_finalize(statement);
    }
    printSqlite3Errmsg(query);
    return results; 
}

bool cTVScraperDB::TableColumnExists(const char *table, const char *column) {
  bool found = false;
  stringstream sql;
  sql << "SELECT * FROM " << table << " WHERE 1 = 2;";
  sqlite3_stmt *statement;
  if(sqlite3_prepare_v2(db, sql.str().c_str(), -1, &statement, 0) == SQLITE_OK)
    for (int i=0; i< sqlite3_column_count(statement); i++) if (strcmp(sqlite3_column_name(statement, i), column) == 0) { found = true; break; }
  sqlite3_finalize(statement); 
  printSqlite3Errmsg(sql.str() );
  return found;
}

void cTVScraperDB::AddCulumnIfNotExists(const char *table, const char *column, const char *type) {
  if (TableColumnExists(table, column) ) return;
  stringstream sql;
  sql << "ALTER TABLE " << table << " ADD COLUMN " << column << " " << type;
  execSql(sql.str() );
}

bool cTVScraperDB::Connect(void) {
    if (inMem) {
        if (sqlite3_open(dbPathMem.c_str(),&db)!=SQLITE_OK) {
            esyslog("tvscraper: failed to open or create %s", dbPathMem.c_str());
            return false;
        }
        esyslog("tvscraper: connecting to db %s", dbPathMem.c_str());
        int rc = LoadOrSaveDb(db, dbPathPhys.c_str(), false);
        if (rc != SQLITE_OK) {
            esyslog("tvscraper: error while loading data from %s, errorcode %d", dbPathPhys.c_str(), rc);
            return false;
        }
    } else {
        if (sqlite3_open(dbPathPhys.c_str(),&db)!=SQLITE_OK) {
            esyslog("tvscraper: failed to open or create %s", dbPathPhys.c_str());
            return false;
        }
        esyslog("tvscraper: connecting to db %s", dbPathPhys.c_str());
    }
    CreateTables();
    return true;
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
    return rc;
}

bool cTVScraperDB::CreateTables(void) {
    stringstream sql;
    sql << "CREATE TABLE IF NOT EXISTS tv (";
    sql << "tv_id integer primary key, ";
    sql << "tv_name nvarchar(255), ";
    sql << "tv_original_name nvarchar(255), ";
    sql << "tv_overview text, ";
    sql << "tv_first_air_date text, ";
    sql << "tv_networks text, ";
    sql << "tv_genres text, ";
    sql << "tv_popularity real, ";
    sql << "tv_vote_average real, ";
    sql << "tv_status text, ";
    sql << "tv_last_season integer, ";
    sql << "tv_number_of_episodes integer, ";
    sql << "tv_last_updated integer";
    sql << ");";

    sql << "CREATE TABLE IF NOT EXISTS tv_episode_run_time (";
    sql << "tv_id integer, ";
    sql << "episode_run_time integer);";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_tv_episode_run_time on tv_episode_run_time (tv_id, episode_run_time);";

    sql << "CREATE TABLE IF NOT EXISTS movie_runtime (";
    sql << "movie_id integer primary key, ";
    sql << "movie_runtime integer);";

    sql << "CREATE TABLE IF NOT EXISTS tv_s_e (";
    sql << "tv_id integer, ";
    sql << "season_number integer, ";
    sql << "episode_number integer, ";
    sql << "episode_id integer, ";
    sql << "episode_name nvarchar(255), ";
    sql << "episode_air_date nvarchar(255), ";
    sql << "episode_vote_average real, ";
    sql << "episode_overview nvarchar, ";
    sql << "episode_guest_stars nvarchar, ";
    sql << "episode_still_path nvarchar";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_tv_s_e on tv_s_e (tv_id, season_number, episode_number); ";
    sql << "CREATE INDEX IF NOT EXISTS idx_tv_s_e_episode on tv_s_e (tv_id, episode_name COLLATE NOCASE); ";

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

    sql << "CREATE TABLE IF NOT EXISTS series_actors (";
    sql << "actor_series_id integer, ";
    sql << "actor_name nvarchar(255), ";
    sql << "actor_role nvarchar(255), ";
    sql << "actor_thumbnail nvarchar(255), ";
    sql << "actor_path nvarchar";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_series_actors on series_actors (actor_series_id, actor_name, actor_role); ";
    sql << "CREATE INDEX IF NOT EXISTS idx1 on series_actors (actor_series_id); ";

    sql << "CREATE TABLE IF NOT EXISTS movies2 (";
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
    sql << "movie_vote_average real";
    sql << ");";

    sql << "CREATE TABLE IF NOT EXISTS actors (";
    sql << "actor_id integer primary key, ";
    sql << "actor_name nvarchar(255)";
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
    sql << "duration integer, ";
    sql << "year integer, ";
    sql << "episode_search_with_shorttext integer, "; // bool: if true, season_number&episode_number must be searched with shorttext
    sql << "movie_tv_id integer, ";   // movie if season_number == -100. Otherwisse, tv
    sql << "season_number integer, ";
    sql << "episode_number integer, ";
    sql << "cache_entry_created_at integer";
    sql << ");";
    sql << "DROP INDEX IF EXISTS idx_cache;";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_cache2 on cache (movie_name_cache, recording, duration, year); ";

    sql << "CREATE TABLE IF NOT EXISTS event (";
    sql << "event_id integer, ";
    sql << "channel_id nvarchar(255), ";
    sql << "valid_till integer, ";
    sql << "movie_tv_id integer, ";   // movie if season_number == -100. Otherwisse, tv
    sql << "season_number integer, ";
    sql << "episode_number integer";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_event on event (event_id, channel_id); ";

    sql << "CREATE TABLE IF NOT EXISTS recordings2 (";
    sql << "event_id integer, ";
    sql << "event_start_time integer, ";
    sql << "channel_id nvarchar(255), ";
    sql << "movie_tv_id integer, ";   // movie if season_number == -100. Otherwisse, tv
    sql << "season_number integer, ";
    sql << "episode_number integer";
    sql << ");";
    sql << "CREATE UNIQUE INDEX IF NOT EXISTS idx_recordings2 on recordings2 (event_id, event_start_time, channel_id); ";

    sql << "CREATE TABLE IF NOT EXISTS scrap_checker (";
    sql << "last_scrapped integer ";
    sql << ");";
    
    char *errmsg;
    if (sqlite3_exec(db,sql.str().c_str(),NULL,NULL,&errmsg)!=SQLITE_OK) {
        esyslog("tvscraper: ERROR: createdb: %s", errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return false;
    }
    AddCulumnIfNotExists("tv_s_e", "episode_still_path", "nvarchar");
    AddCulumnIfNotExists("series_actors", "actor_path", "nvarchar");
    return true;
}

void cTVScraperDB::ClearOutdated(const string &movieDir, const string &seriesDir) {
    //first check which movie images can be deleted
    time_t now = time(0);
    stringstream sql;
    sql << "select movie_tv_id, season_number, episode_number from event where valid_till < " << now;
    vector<vector<string> > result = Query(sql.str());
    int numOutdated = result.size();
        for (int i=0; i < numOutdated; i++) {
            bool keepMovie = false;
            vector<string> row = result[i];
            if (row.size() == 3) {
                int movieID = atoi(row[0].c_str());
                int season_number = atoi(row[1].c_str());
                int episode_number = atoi(row[2].c_str());
                if (movieID > 0) {
                    //are there other still valid events pointing to that movie?
                    keepMovie = CheckMovieOutdatedEvents(movieID, season_number, episode_number);
                    if (!keepMovie) {
                        //are there recordings pointing to that movie?
                        keepMovie = CheckMovieOutdatedRecordings(movieID, season_number, episode_number);
                    }
                    if (!keepMovie) {
                        if (season_number == -100) DeleteMovie(movieID, movieDir);
                                else DeleteSeries(movieID, movieDir, seriesDir);
                    }
                }
            }
    }
// delete all invalid events pointing to movies
// and delete all invalid events pointing to series, series will all be kept for later use
    stringstream sql2;
    sql2 << "delete from event where valid_till < " << now;
    execSql(sql2.str() );
    DeleteOutdatedCache();
    esyslog("tvscraper: Cleanup Done");
}

void cTVScraperDB::DeleteMovie(int movieID, string movieDir) {
    //delete images
    stringstream backdrop;
    backdrop << movieDir << "/" << movieID << "_backdrop.jpg";
    stringstream poster;
    poster << movieDir << "/" << movieID << "_poster.jpg";
    DeleteFile(backdrop.str());
    DeleteFile(poster.str());
    //delete cast of this movie
    stringstream sql;
    sql << "DELETE FROM actor_movie WHERE movie_id = " << movieID;
    execSql(sql.str() );
}

bool cTVScraperDB::CheckMovieOutdatedEvents(int movieID, int season_number, int episode_number) {
// check if there is still an event for which movieID is required
    time_t now = time(0);
    stringstream sql;
    if (season_number == -100)
           sql << "select event_id from event where season_number  = -100 and movie_tv_id = " << movieID << " and valid_till > " << now;
      else sql << "select event_id from event where season_number != -100 and movie_tv_id = " << movieID << " and valid_till > " << now;
    return QueryInt(sql.str() ) > 0;
}

bool cTVScraperDB::CheckMovieOutdatedRecordings(int movieID, int season_number, int episode_number) {
// check if there is still a recording for which movieID is required
    stringstream sql;
    if (season_number == -100)
           sql << "select event_id from recordings2 where season_number  = -100 and movie_tv_id = " << movieID;
      else sql << "select event_id from recordings2 where season_number != -100 and movie_tv_id = " << movieID;
    return QueryInt(sql.str() ) > 0;
}

void cTVScraperDB::DeleteSeries(int seriesID, const string &movieDir, const string &seriesDir) {
// seriesID > 0 => moviedb
// seriesID < 0 => tvdb
// delete images
  stringstream folder;
  if (seriesID < 0) folder << seriesDir << "/" << seriesID * (-1);
               else folder << movieDir <<  "/tv/" << seriesID;
  DeleteAll(folder.str() );
  if (seriesID > 0) {
// delete actor_tv_episode, data are only for moviedb
    stringstream sql;
    sql << "delete from actor_tv_episode where episode_id in ( select episode_id from tv_s_e where tv_s_e.tv_id =" << seriesID << ");";
    execSql(sql.str() );
// delete actor_tv, data are only for moviedb
    stringstream sql0;
    sql0 << "DELETE FROM actor_tv WHERE tv_id = " << seriesID;
    execSql(sql0.str() );
  } else {
// delete series_actors, data are only for tvdb
    stringstream sql0;
    sql0 << "DELETE FROM series_actors WHERE actor_series_id = " << seriesID * (-1);
    execSql(sql0.str() );
  }
// delete tv_s_e
  stringstream sql;
  sql << "DELETE FROM tv_s_e WHERE tv_id = " << seriesID;
  execSql(sql.str() );
// delete tv
  stringstream sql2;
  sql2 << "DELETE FROM tv    WHERE tv_id = " << seriesID;
  execSql(sql2.str() );
// don't delete from tv_episode_runtime, very small, but very usefull
}

/*
void cTVScraperDB::DeleteTv(int tvID, const string &seriesDir) {
    //delete images

    stringstream backdrop;
    backdrop << movieDir << "/" << movieID << "_backdrop.jpg";
    stringstream poster;
    poster << movieDir << "/" << movieID << "_poster.jpg";
    DeleteFile(backdrop.str());
    DeleteFile(poster.str());
    //delete cast of this movie
    stringstream sql;
    sql << "DELETE FROM actor_movie WHERE movie_id = " << movieID;
    execSql(sql.str() );
}
*/

void cTVScraperDB::InsertTv(int tvID, const string &name, const string &originalName, const string &overview, const string &firstAired, const string &networks, const string &genres, float popularity, float vote_average, const string &status, const vector<int> &EpisodeRunTimes) {
    stringstream sql;
    sql << "INSERT INTO tv (tv_id, tv_name, tv_original_name, tv_overview, tv_first_air_date, tv_networks, tv_genres, tv_popularity, tv_vote_average, tv_status, tv_last_season, tv_number_of_episodes) ";
    sql << "VALUES (";
    sql << tvID << ", ";
    sql << "?, ?, ?, ?, ?, ?, ";
    sql << "?, ?, ?, ";
    sql << "0, 0";
    sql << ");";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, NULL);
    if (printSqlite3Errmsg(sql.str()) == 0) {
      sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, originalName.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, overview.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, firstAired.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 5, networks.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 6, genres.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_double(stmt, 7, (double)popularity);
      sqlite3_bind_double(stmt, 8, (double)vote_average);
      sqlite3_bind_text(stmt, 9, status.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      printSqlite3Errmsg(sql.str());
    }
    sqlite3_finalize(stmt);

    for (const int &episodeRunTime : EpisodeRunTimes) {
       stringstream sql;
       sql << "INSERT INTO tv_episode_run_time (tv_id, episode_run_time) ";
       sql << "VALUES (" << tvID << ", " << episodeRunTime << ");";
       execSql(sql.str() );
    }
}

void cTVScraperDB::TvSetEpisodesUpdated(int tvID) {
    stringstream sql;
    time_t now = time(0);
    sql << "UPDATE tv set tv_last_updated = " << now;
    sql << " where tv_id=" << tvID;
    execSql(sql.str() );
}

void cTVScraperDB::TvSetNumberOfEpisodes(int tvID, int LastSeason, int NumberOfEpisodes) {
    stringstream sql;
    sql << "UPDATE tv set tv_last_season = " << LastSeason;
    sql <<             ", tv_number_of_episodes = " << NumberOfEpisodes;
    sql << " where tv_id=" << tvID;
    execSql(sql.str() );
}

bool cTVScraperDB::TvGetNumberOfEpisodes(int tvID, int &LastSeason, int &NumberOfEpisodes) {
    stringstream sql;
    sql << "select tv_last_season, tv_number_of_episodes from tv where tv.tv_id = " << tvID;
    vector<string> result;
    if (QueryLine(result, sql.str() ) && result.size() == 2) {
        LastSeason = atoi(result[0].c_str());
        NumberOfEpisodes = atoi(result[1].c_str());
        return true;
    }
    return false;
}

int cTVScraperDB::SearchTvEpisode(int tvID, const string &episode_search_name, const string &episode_search_name_full, int &season_number, int &episode_number, string &episode_name) {
// return number of found episodes
// if episodes are found, return season_number & episode_number of first found episode
    stringstream sql;
    sql << "select season_number, episode_number, episode_name from tv_s_e where tv_id = " << tvID << " and episode_name like ?";
    vector<vector<string> > result = QueryEscaped(sql.str(), episode_search_name);
    int min_distance = -1;
    for (vector<string> &row : result) if (row.size() == 3) {
      int cur_distance = sentence_distance(episode_search_name_full, row[2]);
      if (min_distance == -1 || cur_distance < min_distance) {
        min_distance = cur_distance;
        season_number = atoi(row[0].c_str());
        episode_number = atoi(row[1].c_str());
        episode_name = row[2];
        if (episode_search_name_full.empty() ) return result.size();
      }
    }
    if (min_distance != -1) return result.size();
    season_number = 0;
    episode_number = 0;
    return 0;
}

bool cTVScraperDB::SearchEpisode(sMovieOrTv &movieOrTv, const string &tvSearchEpisodeString) {
// in: movieOrTv.id, tvSearchEpisodeString
// out: movieOrTv.season, movieOrTv.episode
   if (SearchEpisode_int(movieOrTv, tvSearchEpisodeString) ) return true;
   if (!isdigit(tvSearchEpisodeString[0]) ) return false;
   std::size_t found_blank = tvSearchEpisodeString.find(' ');
   if (found_blank ==std::string::npos || found_blank > 7 || found_blank + 1 >= tvSearchEpisodeString.length() ) return false;
   if (SearchEpisode_int(movieOrTv, tvSearchEpisodeString.substr(found_blank + 1) )) return true;
   return false;
}

std::size_t cTVScraperDB::SearchEpisode_int(sMovieOrTv &movieOrTv, const string &tvSearchEpisodeString) {
// return 0, if no match was found
// otherwise, number of matching characters
  movieOrTv.season = 0;
  movieOrTv.episode = 0;
  std::size_t min_search_string_length = 4;
  std::size_t found_blank = tvSearchEpisodeString.find(' ');
  if (found_blank !=std::string::npos && found_blank < min_search_string_length) min_search_string_length += found_blank;
  if (tvSearchEpisodeString.length() < min_search_string_length) return 0;
  int n_found;
  string episodeName;
  n_found = SearchTvEpisode(movieOrTv.id, tvSearchEpisodeString, tvSearchEpisodeString, movieOrTv.season, movieOrTv.episode, episodeName);
  if (n_found > 0) return tvSearchEpisodeString.length();
  n_found = SearchTvEpisode(movieOrTv.id, tvSearchEpisodeString + "%", tvSearchEpisodeString, movieOrTv.season, movieOrTv.episode, episodeName);
  if (n_found > 0) return tvSearchEpisodeString.length();
  if (tvSearchEpisodeString.length() > 7) {
    n_found = SearchTvEpisode(movieOrTv.id, "%" + tvSearchEpisodeString + "%", tvSearchEpisodeString, movieOrTv.season, movieOrTv.episode, episodeName);
    if (n_found > 0) return tvSearchEpisodeString.length();
  }
  std::size_t il, im, ih;
  il = 0;
  ih = tvSearchEpisodeString.length();
  im = ih;
  while(ih - il > 1) {
    im = (ih - il)/2 + il;
    if(im < min_search_string_length) {
      if (ih <= min_search_string_length) return 0;
      im = min_search_string_length;
    }
    n_found = SearchTvEpisode(movieOrTv.id, ((im > 7)?"%":"") + tvSearchEpisodeString.substr(0, im) + "%", tvSearchEpisodeString, movieOrTv.season, movieOrTv.episode, episodeName);
//  if (n_found == 1) return im; // don't stop, make sure to find maximum number of matching chars
    if (n_found < 1) ih = im;
      else il = im;
  }
  if (n_found >= 1) {
// sanity check: do we have a sufficient match?
    if (im <= min(episodeName.length(), tvSearchEpisodeString.length() ) * 60 / 100) return 0;
    return im;
  }
  n_found = SearchTvEpisode(movieOrTv.id, ((im - 1 > 7)?"%":"") + tvSearchEpisodeString.substr(0, im - 1) + "%", tvSearchEpisodeString, movieOrTv.season, movieOrTv.episode, episodeName);
  if (n_found >= 1) {
// sanity check: do we have a sufficient match?
    if (im -1 > min(episodeName.length(), tvSearchEpisodeString.length() ) * 60 / 100) return im-1;
    return 0;
  }
  return 0;
}

void cTVScraperDB::InsertTv_s_e(int tvID, int season_number, int episode_number, int episode_id, const string &episode_name, const string &airDate, float vote_average, const string &episode_overview, const string &episode_guest_stars, const string &episode_still_path) {
    stringstream sql;
    sql << "INSERT INTO tv_s_e (tv_id, season_number, episode_number, episode_id, episode_name, episode_air_date, episode_vote_average, episode_overview, episode_guest_stars, episode_still_path) ";
    sql << "VALUES (";
    sql << tvID << ", ";
    sql << season_number << ", ";
    sql << episode_number << ", ";
    sql << episode_id << ", ";
    sql << "?, ?, ";
    sql << vote_average << ", ";
    sql << "?, ?, ? ";
    sql << ");";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, NULL);
    if (!printSqlite3Errmsg(sql.str() )) {
      sqlite3_bind_text(stmt, 1, episode_name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, airDate.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, episode_overview.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, episode_guest_stars.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 5, episode_still_path.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      printSqlite3Errmsg(sql.str() );
    }
    sqlite3_finalize(stmt);
}

string cTVScraperDB::GetEpisodeStillPath(int tvID, int seasonNumber, int episodeNumber) {
  stringstream sql;
  sql << "select episode_still_path from tv_s_e where tv_id = " << tvID << " and season_number = " << seasonNumber << " and episode_number = " << episodeNumber;
  string result;   
  QueryValue(result, sql.str() );
  return result;
}

void cTVScraperDB::InsertEvent(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number) {
    tEventID eventID = sEventOrRecording->EventID();
    time_t validTill = sEventOrRecording->EndTime();
    cString channelIDs = sEventOrRecording->ChannelIDs();
    stringstream sql;
    sql << "INSERT OR REPLACE INTO event (event_id, channel_id, valid_till, movie_tv_id, season_number, episode_number) ";
    sql << "VALUES (";
    sql << eventID << ", ?, " << validTill << ", " << movie_tv_id;
    sql << ", " << season_number << ", " << episode_number;
    sql << ");";
    execSqlBind(sql.str(), (const char *)channelIDs);
}

void cTVScraperDB::InsertMovie(int movieID, const string &title, const string &original_title, const string &tagline, const string &overview, bool adult, int collection_id, const string &collection_name, int budget, int revenue, const string &genres, const string &homepage, const string &release_date, int runtime, float popularity, float vote_average){
    stringstream sql;
    sql << "INSERT INTO movies2 (movie_id, movie_title, movie_original_title, movie_tagline, movie_overview, movie_adult, movie_collection_id, movie_collection_name, movie_budget, movie_revenue, movie_genres, movie_homepage, movie_release_date, movie_runtime, movie_popularity, movie_vote_average) ";
    sql << "VALUES (";
    sql << movieID << ", ";
    sql << "?, ?, ?, ?, ";
    sql << (int)adult  << ", ";
    sql << collection_id  << ", ";
    sql << "?, ";
    sql << budget << ", ";
    sql << revenue << ", ";
    sql << "?, ?, ?, ";
    sql << runtime << ", ";
    sql << "?, ? ";
    sql << " );";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, NULL);
    if (!printSqlite3Errmsg(sql.str() )) {
      sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, original_title.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, tagline.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, overview.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 5, collection_name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 6, genres.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 7, homepage.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 8, release_date.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_double(stmt, 9, (double)popularity);
      sqlite3_bind_double(stmt, 10, (double)vote_average);
      sqlite3_step(stmt);
      printSqlite3Errmsg(sql.str() );
    }
    sqlite3_finalize(stmt);
// extra table for runtime, will not be deleted
    stringstream sql2;
    if (runtime == 0) runtime = -1; // -1 : no runtime available in themoviedb
    sql2 << "INSERT INTO movie_runtime (movie_id, movie_runtime) ";
    sql2 << "VALUES (" << movieID << ", " << runtime;
    sql2 << " );";
    execSql(sql2.str() );
}

int cTVScraperDB::GetMovieRuntime(int movieID) {
// -1 : no runtime available in themoviedb
//  0 : themoviedb never checked for runtime
    stringstream sql;
    sql << "select movie_runtime from movie_runtime where movie_id = " << movieID;
    return QueryInt(sql.str() );
}

void cTVScraperDB::InsertMovieActor(int movieID, int actorID, string name, string role) {
    stringstream sql;
    sql << "INSERT INTO actors (actor_id, actor_name) ";
    sql << "VALUES (";
    sql << actorID << ", ?";
    sql << ");";
    execSqlBind(sql.str(), name);

    stringstream sql2;
    sql2 << "INSERT INTO actor_movie (actor_id, movie_id, actor_role) ";
    sql2 << "VALUES (";
    sql2 << actorID << ", " << movieID << ", ?";
    sql2 << ");";
    execSqlBind(sql2.str(), role);
}

void cTVScraperDB::InsertTvActor(int tvID, int actorID, const string &name, const string &role) {
    stringstream sql;
    sql << "INSERT INTO actors (actor_id, actor_name) ";
    sql << "VALUES (";
    sql << actorID << ", ?";
    sql << ");";
    execSqlBind(sql.str(), name);

    stringstream sql2;
    sql2 << "INSERT INTO actor_tv (tv_id, actor_id, actor_role) ";
    sql2 << "VALUES (";
    sql2 << tvID << ", ";
    sql2 << actorID << ", ?";
    sql2 << ");";
    execSqlBind(sql2.str(), role);
}

void cTVScraperDB::InsertTvEpisodeActor(int episodeID, int actorID, const string &name, const string &role) {
    stringstream sql;
    sql << "INSERT INTO actors (actor_id, actor_name) ";
    sql << "VALUES (";
    sql << actorID << ", ?";
    sql << ");";
    execSqlBind(sql.str(), name);

    stringstream sql2;
    sql2 << "INSERT INTO actor_tv_episode (episode_id, actor_id, actor_role) ";
    sql2 << "VALUES (";
    sql2 << episodeID << ", ";
    sql2 << actorID << ", ?";
    sql2 << ");";
    execSqlBind(sql2.str(), role);
}

void cTVScraperDB::InsertActor(int seriesID, const string &name, const string &role, const string &thumb, const string &path) {
    stringstream sql;
    sql << "INSERT INTO series_actors (actor_series_id, actor_name, actor_role, actor_thumbnail, actor_path) ";
    sql << "VALUES (";
    sql << seriesID << ", ";
    sql << "?, ?, ?, ?";
    sql << ");";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, NULL);
    if (!printSqlite3Errmsg(sql.str() )) {
      sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, role.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, thumb.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, path.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      printSqlite3Errmsg(sql.str() );
    }
    sqlite3_finalize(stmt);
}

bool cTVScraperDB::MovieExists(int movieID) {
    stringstream sql;
    sql << "select count(movies2.movie_id) as found from movies2 where movies2.movie_id = " << movieID;
    return QueryInt(sql.str() ) == 1;
}

bool cTVScraperDB::TvExists(int tvID) {
    stringstream sql;
    sql << "select count(tv.tv_id) as found from tv where tv.tv_id = " << tvID;
    return QueryInt(sql.str() ) == 1;
}

int cTVScraperDB::SearchMovie(string movieTitle) {
    string sql = "select movie_id from movies2 where movie_title=?";
    vector<vector<string> > result = QueryEscaped(sql, movieTitle);
    int movieID = 0;
    if (result.size() > 0) {
        vector<vector<string> >::iterator it = result.begin();
        vector<string> row = *it;
        if (row.size() > 0) {
            movieID = atoi(row[0].c_str());
        }
    }
    return movieID;
}

int cTVScraperDB::SearchTv(string tvTitle) {
    string sql = "select tv_id from tv where tv_name=?";
    vector<vector<string> > result = QueryEscaped(sql, tvTitle);
    int tvID = 0;
    if (result.size() > 0) {
        vector<vector<string> >::iterator it = result.begin();
        vector<string> row = *it;
        if (row.size() > 0) {
            tvID = atoi(row[0].c_str());
        }
    }
    return tvID;
}

void cTVScraperDB::InsertRecording2(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number) {
    tEventID eventID = sEventOrRecording->EventID();
    time_t eventStartTime = sEventOrRecording->StartTime();
    cString channelIDs = sEventOrRecording->ChannelIDs();
    stringstream sql;
    sql << "INSERT OR REPLACE INTO recordings2 (event_id, event_start_time, channel_id, movie_tv_id, season_number, episode_number)";
    sql << "VALUES (";
    sql << eventID << ", " << eventStartTime << ", ?, " << movie_tv_id << ", " << season_number << ", " << episode_number;
    sql << ");";
    execSqlBind(sql.str(), (const char *)channelIDs );
    const cRecording *recording = sEventOrRecording->Recording();
    if (recording) {
      WriteRecordingInfo(sEventOrRecording->Recording(), movie_tv_id, season_number, episode_number);
// copy pictures to recording folder
// get poster path
      cImageServer imageServer(this);
      cTvMedia poster = imageServer.GetPoster(movie_tv_id, season_number, episode_number);
      if (!poster.path.empty() ) CopyFile(poster.path, string(recording->FileName() ) + "/poster.jpg" );
      if (season_number == -100) {
        cTvMedia fanart = imageServer.GetMovieFanart(movie_tv_id);
        if (!fanart.path.empty() ) CopyFile(fanart.path, string(recording->FileName() ) + "/fanart.jpg" );
      } else {
        vector<cTvMedia> fanarts = imageServer.GetSeriesFanarts(movie_tv_id, season_number, episode_number);
        if (fanarts.size() > 0) CopyFile(fanarts[0].path, string(recording->FileName() ) + "/fanart.jpg" );
      }
    }
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
  if (movie_tv_id > 0) json_object_set(jInfo, "themoviedb", jTvscraper);
                  else json_object_set(jInfo, "thetvdb", jTvscraper);
// set attributes
  stringstream sql;
  if (season_number != -100) {
// TV Show
    json_object_set_new(jTvscraper, "type", json_string("tv show"));
    sql << "select tv_name, tv_first_air_date from tv where tv_id = " << movie_tv_id;
    if( season_number != 0 || episode_number != 0) {  // season / episode was found
      json_object_set_new(jTvscraper, "season_number", json_integer(season_number) );
      json_object_set_new(jTvscraper, "episode_number", json_integer(episode_number) );
  }} else {
// movie
    json_object_set_new(jTvscraper, "type", json_string("movie"));
    sql << "select movie_title, movie_release_date from movies2 where movie_id = " << movie_tv_id;
  }
  vector<string> qu_result;
  if (QueryLine(qu_result, sql.str() ) && qu_result.size() == 2) {
    json_object_set_new(jTvscraper, "name", json_string(qu_result[0].c_str()) );
    if (qu_result[1].length() >= 4)
      json_object_set_new(jTvscraper, "year", json_integer(atoi(qu_result[1].substr(0, 4).c_str()) ));
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
    sql << "select movie_tv_id, season_number, episode_number from event where event_id = " << eventID;
    sql << " and channel_id = '" << (const char *)channelIDs << "'";
    vector<string> result;
    if (QueryLine(result, sql.str() ) && result.size() == 3) {
        int movieTvId = atoi(result[0].c_str());
        int seasonNumber = atoi(result[1].c_str());
        int episodeNumber = atoi(result[2].c_str());

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
    time_t last_scrapped = QueryInt64("select last_scrapped from scrap_checker");
    if (last_scrapped) {
            int difference = (int)(now - last_scrapped);
            if (difference > minimumDistance) {
                startScrapping = true;
                execSql("delete from scrap_checker");
                stringstream sql3;
                sql3 << "INSERT INTO scrap_checker (last_scrapped) VALUES (" << now << ")";
                execSql(sql3.str() );
        }
    } else {
            startScrapping = true;
            stringstream sql2;
            sql2 << "INSERT INTO scrap_checker (last_scrapped) VALUES (" << now << ")";
            execSql(sql2.str() );
    }
    return startScrapping;
}

bool cTVScraperDB::GetMovieTvID(csEventOrRecording *sEventOrRecording, int &movie_tv_id, int &season_number, int &episode_number) {
    tEventID eventID = sEventOrRecording->EventID();
    time_t eventStartTime = sEventOrRecording->StartTime();
    cString channelIDs = sEventOrRecording->ChannelIDs();
    stringstream sql;
    if (!sEventOrRecording->Recording() ){
        sql << "select movie_tv_id, season_number, episode_number from event where event_id = " << eventID;
        sql << " and channel_id = '" << (const char *)channelIDs << "'";
         }
    else {
        sql << "select movie_tv_id, season_number, episode_number from recordings2 where event_id = " << eventID;
        sql << " and event_start_time = " << eventStartTime;
        sql << " and channel_id = '" << (const char *)channelIDs << "'";
          }
    vector<string> result;
    if (QueryLine(result, sql.str() ) && result.size() == 3 ) {
            movie_tv_id = atoi(result[0].c_str());
            season_number = atoi(result[1].c_str());
            episode_number = atoi(result[2].c_str());
            return true;
    }
    return false;
}

vector<vector<string> > cTVScraperDB::GetActorsMovie(int movieID) {
    stringstream sql;
    sql << "select actors.actor_id, actor_name, actor_role ";
    sql << "from actors, actor_movie ";
    sql << "where actor_movie.actor_id = actors.actor_id ";
    sql << "and actor_movie.movie_id = " << movieID;
    return Query(sql.str());
}

vector<vector<string> > cTVScraperDB::GetActorsSeries(int seriesID) {
    stringstream sql;
    sql << "select actor_name, actor_role, actor_thumbnail ";
    sql << "from series_actors ";
    sql << "where actor_series_id = " << seriesID;
    return Query(sql.str());
}

vector<vector<string> > cTVScraperDB::GetActorsSeriesPath(int seriesID) {
    stringstream sql;
    sql << "select actor_thumbnail actor_path ";
    sql << "from series_actors ";
    sql << "where actor_series_id = " << seriesID;
    return Query(sql.str());
}

int cTVScraperDB::GetEpisodeID(int tvID, int seasonNumber, int episodeNumber) {
    stringstream sql1;
    sql1 << "select episode_id from tv_s_e where tv_id = " << tvID << " and season_number = " << seasonNumber << " and episode_number = " << episodeNumber;
    return QueryInt(sql1.str() );
}
vector<vector<string> > cTVScraperDB::GetGuestActorsTv(int episodeID) {
    stringstream sql2;
    sql2 << "select actors.actor_id, actor_name, actor_role ";
    sql2 << "from actors, actor_tv_episode ";
    sql2 << "where actor_tv_episode.actor_id = actors.actor_id ";
    sql2 << "and actor_tv_episode.episode_id = " << episodeID;
    return Query(sql2.str());
}

vector<vector<string> > cTVScraperDB::GetActorsTv(int tvID) {
// read tv actors (cast)
    stringstream sql;
    sql << "select actors.actor_id, actor_name, actor_role ";
    sql << "from actors, actor_tv ";
    sql << "where actor_tv.actor_id = actors.actor_id ";
    sql << "and actor_tv.tv_id = " << tvID;
    vector<vector<string> > result = Query(sql.str());
    return result;
}

string cTVScraperDB::GetDescriptionTv(int tvID) {
    string description;
    stringstream sql;
    sql << "select tv_overview from tv where tv_id = " << tvID;
    QueryValue(description, sql.str() );
    return description;
}

string cTVScraperDB::GetDescriptionTv(int tvID, int seasonNumber, int episodeNumber) {
    string description;
    stringstream sql;
    sql << "select episode_overview from tv_s_e where tv_id = " << tvID << " and season_number = " << seasonNumber << " and episode_number = " << episodeNumber;
    QueryValue(description, sql.str() );
    return description;
}

bool cTVScraperDB::GetMovie(int movieID, string &title, string &original_title, string &tagline, string &overview, bool &adult, int &collection_id, string &collection_name, int &budget, int &revenue, string &genres, string &homepage, string &release_date, int &runtime, float &popularity, float &vote_average) {
    stringstream sql;
    sql << "select movie_title, movie_original_title, movie_tagline, movie_overview, movie_adult, movie_collection_id, movie_collection_name, movie_budget, movie_revenue, movie_genres, movie_homepage, movie_release_date, movie_runtime, movie_popularity, movie_vote_average from movies2 ";
    sql << "where movie_id = " << movieID;
    vector<string> row;
    if (QueryLine(row, sql.str() ) && row.size() > 0) {
            title = row[0];
            original_title = row[1];
            tagline = row[2];
            overview = row[3];
            adult = (bool)atoi(row[4].c_str() );
            collection_id = atoi(row[5].c_str() );
            collection_name = row[6];
            budget = atoi(row[7].c_str() );
            revenue = atoi(row[8].c_str() );
            genres = row[9];
            homepage = row[10];
            release_date = row[11];
            runtime = atoi(row[12].c_str() );
            popularity = atof(row[13].c_str() );
            vote_average = atof(row[14].c_str() );
            return true;
    }
    return false;
}

bool cTVScraperDB::GetTv(int tvID, string &name, string &overview, string &firstAired, string &networks, string &genres, float &popularity, float &vote_average, string &status) {
    stringstream sql;
    sql << "select tv_name, tv_overview, tv_first_air_date, tv_networks, tv_genres, tv_popularity, tv_vote_average, tv_status from tv ";
    sql << "where tv_id = " << tvID;
    vector<string> row;
    if (QueryLine(row, sql.str() ) && row.size() > 0) {
            name = row[0];
            overview = row[1];
            firstAired = row[2];
            networks = row[3];
            genres = row[4];
            popularity = atof(row[5].c_str() );
            vote_average = atof(row[6].c_str() );
            status = row[7];
            return true;
    }
    return false;
}

bool cTVScraperDB::GetTv(int tvID, time_t &lastUpdated, string &status) {
    stringstream sql;
    sql << "select tv_last_updated, tv_status from tv ";
    sql << "where tv_id = " << tvID;
    vector<string> row;

    if (QueryLine(row, sql.str() ) && row.size() > 1) {
            char *ep;
            lastUpdated = strtoul(row[0].c_str(), &ep, 10);
            status = row[1];
            return true;
    }
    return false;
}

bool cTVScraperDB::GetTvEpisode(int tvID, int seasonNumber, int episodeNumber, int &episodeID, string &name, string &airDate, float &vote_average, string &overview, string &episodeGuestStars) {
    stringstream sql;
    sql << "select episode_id, episode_name, episode_air_date, episode_vote_average, episode_overview, episode_guest_stars ";
    sql << "from tv_s_e where tv_id = " << tvID << " and season_number = " << seasonNumber << " and episode_number = " << episodeNumber;
    vector<string> row;
    if (QueryLine(row, sql.str() ) && row.size() > 0) {
            episodeID = atoi(row[0].c_str() );
            name = row[1];
            airDate = row[2];
            vote_average = atof(row[3].c_str() );
            overview = row[4];
            episodeGuestStars = row[5];
            return true;
    }
    return false;
}

string cTVScraperDB::GetDescriptionMovie(int movieID) {
    string description;
    stringstream sql;
    sql << "select movie_overview from movies2 where movie_id = " << movieID;
    QueryValue(description, sql.str() );
    return description;
}
bool cTVScraperDB::GetFromCache(const string &movieNameCache, csEventOrRecording *sEventOrRecording, sMovieOrTv &movieOrTv, bool baseNameEquShortText) {
// return true if cache was found
// if nothing was found, return false and set movieOrTv.id = 0

    stringstream sql;
    int recording = 0;
    if (sEventOrRecording->Recording() ) {
      recording = 1;
      if (baseNameEquShortText) recording = 3;
    }
    int durationInSec = sEventOrRecording->DurationInSec();
    sql << "select year, episode_search_with_shorttext, movie_tv_id, season_number, episode_number ";
    sql << "from cache WHERE movie_name_cache = ? AND recording = " << recording << " and duration BETWEEN " << (durationInSec - 300) << " and  " << (durationInSec + 300);
    vector<vector<string> > results = QueryEscaped(sql.str(), movieNameCache);
    for (vector<string> &result : results) if (result.size() == 5 && result[0].length() >= 4) {
// there was a year match. Check: do have a year match?
      vector<int> years;
      AddYears(years, movieNameCache.c_str() );
      sEventOrRecording->AddYears(years);
      if (find(years.begin(), years.end(), atoi(result[0].c_str() )) != years.end() ) {
        StrToMovieOrTv(result, movieOrTv);
        return true;
      }
    }
// no match with year found, check for match without year
    for (vector<string> &result : results) if (result.size() == 5 && result[0].length() != 4) {
      StrToMovieOrTv(result, movieOrTv);
      return true;
    }
// no match found
  movieOrTv.id = 0;
  return false;
}

bool cTVScraperDB::StrToMovieOrTv(const vector<string> &result, sMovieOrTv &movieOrTv) {
  if (result.size() != 5) return false;
  movieOrTv.episodeSearchWithShorttext = (bool) atoi(result[1].c_str() );
  movieOrTv.id  = atoi(result[2].c_str() );
  movieOrTv.season = atoi(result[3].c_str() );
  movieOrTv.episode = atoi(result[4].c_str() );
  if      (movieOrTv.id     == 0)    movieOrTv.type = scrapNone;
  else if (movieOrTv.season == -100) movieOrTv.type = scrapMovie;
  else                               movieOrTv.type = scrapSeries;
  return true;
}

void cTVScraperDB::InsertCache(const string &movieNameCache, csEventOrRecording *sEventOrRecording, sMovieOrTv &movieOrTv, bool baseNameEquShortText) {
    time_t now = time(0);
    int recording = 0;
    if (sEventOrRecording->Recording() ) {
      recording = 1;
      if (baseNameEquShortText) recording = 3;
    }
    stringstream sql;
    sql << "INSERT INTO cache (movie_name_cache, recording, duration, year, episode_search_with_shorttext, movie_tv_id, season_number, episode_number, cache_entry_created_at)";
    sql << "VALUES (?, ";
    sql << recording << ", " << sEventOrRecording->DurationInSec() << ", " << (movieOrTv.id?movieOrTv.year:0) << ", " << (int)movieOrTv.episodeSearchWithShorttext << ", " << movieOrTv.id << ", " << movieOrTv.season << ", " << movieOrTv.episode << ", " << now;
    sql << ");";
    execSqlBind(sql.str(), movieNameCache);
}
void cTVScraperDB::DeleteOutdatedCache() {
    time_t outdated = time(0) - 2*7*24*60*60;  // entries older than two weeks
    stringstream sql;
    sql << "delete from cache where cache_entry_created_at < " << outdated;
    execSql(sql.str() );
}
int cTVScraperDB::DeleteFromCache(const char *movieNameCache) { // return number of deleted entries
  execSqlBind("delete from cache where movie_name_cache = ?", movieNameCache );
  return sqlite3_changes(db);
}

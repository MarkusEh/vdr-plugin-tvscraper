#ifndef __TVSCRAPER_TVSCRAPERDB_H
#define __TVSCRAPER_TVSCRAPERDB_H
#include <sqlite3.h>

using namespace std; 

// --- cTVScraperDB --------------------------------------------------------

class cTVScraperDB;
typedef const char* cCharS;
class cSql {
//  execute sql statement   ==================================================
// for that, just create an instance and provide the paramters.
// the statement will be executed, as soon as all parameters are available
//   example 1
// cSql stmt(db, "delete movies where movie_id = ?", movieId);
//   example 2
// cSql stmt(db);
// stmt.prepare("delete movies where movie_id = ?", movieId);
// stmt.prepare("delete series where series = ?", seriesId);

//  execute sql statement, and read one row ==================================
// start as described in "execute sql statement"
// stmt.readRow() will return "true" if a row was found
// you can read the values with the same stmt.readRow() call, or later on with >>
// in addition, methods returning the values are available,
// where you can specify the column (starting with 0) you request:
//  int getInt(int column) sqlite3_int64 getInt64(int column)
//  const char *getCharS(int column) std::string_view getStringView(int column)
//   example 1
// cSql stmt(db, "select movie_name from movies where movie_id = ?", movieId);
// if (!stmt.readRow(movieName) ) cout << "movie id not found\n";
// else cout << "movie name: " << movieName << "\n";
//   example 2
// cSql stmt(db, "select movie_name from movies where movie_id = ?", movieId);
// if (!stmt.readRow() ) cout << "movie id not found\n";
// else cout << "movie name: " << stmt.getCharS(0) << "\n";

//  execute sql statement, and read several rows ==================================
// cSql has a forward iterator, and can be used in for loops
//   example
// for (cSql &stmt: cSql(db, "select movie_name from movies where movie_id = ?", movieId) {
//   cout << "movie name: " << stmt.getCharS(0) << "\n";
// }

//  execute sql statement, and write several rows =================================
// the parameters can be reset, to avoid multiple compilations of the sql statement
//  example 1
// cSql stmt(db, "INSERT OR REPLACE INTO tv_episode_run_time (tv_id, episode_run_time) VALUES (?, ?);");
// for (const int &episodeRunTime: EpisodeRunTimes) stmt.resetBindParameters(tvID, episodeRunTime);
//  example 2
// cSql stmt(db, "INSERT INTO best_numbers (number) VALUES (?);");
// for (int i: <vector<int>>{4,2}) stmt.resetBindParameters(i); 
// note: there must be one or more parameters for this to work

//  RESTRICTIONS / CAREFULL!!!
// char* and string_view in result values:
//   in loops: valid only in this loop, invalid in the next loop (invalidated by step).
//             also invalid after the loop ends / outside the loop.
//   otherwise: invalidated by the next call to prepare, resetBindParameters, or destruction if the cSql instance

// for the parameters:
// char*, strings and string_view provided: the refernces must be valid until the object is destroyed.
// this shoudn't be an issue for requesting 0 or 1 rows, as step is directly called after the binding parameters are available
//   after that, the binding parameters should not be needed any more
// for for(:) loops: until C++23: make sure to not use temporaries as binding parameters
// see https://en.cppreference.com/w/cpp/language/range-for
// If range-expression returns a temporary, its lifetime is extended until the end of the loop.
// until C++23: Lifetimes of all temporaries within range-expression are not extended
// since C++23: Lifetimes of all temporaries within range-expression are extended if they would otherwise be destroyed at the end of range-expression


  public:
    cSql(const cSql&) = delete;
    cSql &operator= (const cSql &) = delete;
    cSql(const cTVScraperDB *db): m_db(db) {}
    template<typename... Args>
    cSql(const cTVScraperDB *db, const char *query, const Args&... args): m_db(db) { prepare(query, args...); }
    class iterator: public std::iterator<std::forward_iterator_tag, cSql, int, cSql*, cSql &> {
        cSql *m_sql = NULL;
        int m_row = -1;
      public:
        explicit iterator(cSql &sql): m_sql(&sql) {
          m_row = -1;
          if (!sql.reset() ) return;
          if (m_sql->readRow() ) m_row = 0;
        }
        explicit iterator(): m_row(-1) { }
        iterator& operator++() {
          if (m_row == -1) return *this;
          m_sql->stepInt();
          if (m_sql->readRow() ) m_row++;
          else m_row = -1;
          return *this;
        }
        bool operator==(iterator other) const { return m_row == other.m_row; }
        bool operator!=(iterator other) const { return m_row != other.m_row; }
        reference operator*() const {
          return *m_sql;
        }
      };
    iterator begin() { return iterator(*this); }
    const iterator end() { return iterator(); }

    template<typename... Args>
    cSql & prepare(const char *query, Args&&... args) {
      finalize();  // if this cSql was used, close anything still open. Reset m_statement to NULL
      m_cur_q = -1;
      int i = 0;
      for (m_num_q = 0; query[i]; i++) if (query[i] == '?') m_num_q++;
      prepareDelayed(query);
      if (m_num_q == 0) {
        m_auto_step = true;
        stepInt();
      }
      bindParameters(std::forward<Args>(args)...);  // called here to create error messages, if m_num_q == 0
      return *this;
    }
  private:
    void prepareDelayed(const char *query);
// bind parameters
    void bindParameters() {}
    template<typename T, typename... Args>
    void bindParameters(T &&arg1, Args&&... args) {
      *this << std::forward<T>(arg1);
      bindParameters(std::forward<Args>(args)...);
    }
    void bind_int64(sqlite3_int64 i) {
      if (!preCheckBind() ) return;
      sqlite3_bind_int64(m_statement, ++m_cur_q, i);
      if (allBindingValuesAvailable() ) { m_auto_step = true; stepInt(); }
    }
  public:
  private:
    cSql & operator<<(long long int i) { bind_int64(static_cast<sqlite3_int64>(i)); return *this; }
    cSql & operator<<(long int i) { bind_int64(static_cast<sqlite3_int64>(i)); return *this; }
    cSql & operator<<(int i) { bind_int64(static_cast<sqlite3_int64>(i)); return *this; }
    cSql & operator<<(unsigned int i) { bind_int64(static_cast<sqlite3_int64>(i)); return *this; }
    cSql & operator<<(unsigned long int i) { bind_int64(static_cast<sqlite3_int64>(i)); return *this; }
    cSql & operator<<(unsigned long long int i) { bind_int64(static_cast<sqlite3_int64>(i)); return *this; }
    cSql & operator<<(double f) {
      if (preCheckBind() ) {
        sqlite3_bind_double(m_statement, ++m_cur_q, f);
        if (allBindingValuesAvailable() ) { m_auto_step = true; stepInt(); }
      }
      return *this;
    }
    cSql & operator<<(const char *s) {
      if (preCheckBind() ) {
//  SQLITE_STATIC may be passsed to indicate that the application remains responsible for disposing of the object.
//  In this case, the object and the provided pointer to it must remain valid until either
//    - the prepared statement is finalized or
//    - the same SQL parameter is bound to something else, whichever occurs sooner
// remark: we assume it is only needed until the 1st call to step ...

        sqlite3_bind_text(m_statement, ++m_cur_q, s, -1, SQLITE_STATIC);
        if (allBindingValuesAvailable() ) { m_auto_step = true; stepInt(); }
      }
      return *this;
    }
    cSql & operator<<(const std::string_view &str) {
      if (preCheckBind() ) {
        sqlite3_bind_text(m_statement, ++m_cur_q, str.data(), str.length(), SQLITE_STATIC);
        if (allBindingValuesAvailable() ) { m_auto_step = true; stepInt(); }
      }
      return *this;
    }
    cSql & operator<<(const std::string &str) {
      if (preCheckBind() ) {
        sqlite3_bind_text(m_statement, ++m_cur_q, str.data(), str.length(), SQLITE_STATIC);
        if (allBindingValuesAvailable() ) { m_auto_step = true; stepInt(); }
      }
      return *this;
    }
  public:
    template<typename T, typename... Args>
    cSql & resetBindParameters(T &&arg1, Args&&... args) {
// must be called with 1 (or more) parameters
//      if (m_cur_q == -1) prepareDelayed();
      if (!assertStatement("resetBindParameters")) return *this; // Error: prepare MUST be called first
      const char *query = sqlite3_sql(m_statement);
      if (m_num_q == 0) {
// there are no bind parameters, that could be reset
        esyslog("tvscraper: ERROR in cSql::resetBindParameters, m_num_q == 0, query %s", query?query:"NULL");
        return *this;
      }
      if (m_cur_q > 0) {
// 1 (or more) parameters where bound, so a reset is required
// consistency checks
        if (!allBindingValuesAvailable() )
          esyslog("tvscraper: ERROR in cSql::resetBindParameters, not all query parameters provided, m_cur_q %d m_num_q %d query %s", m_cur_q, m_num_q, query?query:"NULL");
        if (m_last_step_result == -10) esyslog("tvscraper: ERROR in cSql::resetBindParameters, statement not executed query %s", query?query:"NULL");
        assertAllValuesRequested("cSql::resetBindParameters");
// reset of parameters is required
        sqlite3_reset(m_statement);
        sqlite3_clear_bindings(m_statement);
        m_last_step_result = -10;
        m_auto_step = false;
        m_cur_q = 0;
        m_num_cols = 0;
        m_cur_col = -1;
      }
      bindParameters(std::forward<T>(arg1), std::forward<Args>(args)...);
      return *this;
    }
  private:
    bool reset() {
// go back to the first row
      if (m_cur_col == 0 && m_auto_step && !m_explicit_col) return true;  // already in first row, nothing to do
      if (!assertStatement("reset")) return false;
      if (!allBindingValuesAvailable() ) {
        const char *query = sqlite3_sql(m_statement);
        esyslog("tvscraper: ERROR in cSql::reset, not all query parameters provided, m_cur_q %d m_num_q %d query %s", m_cur_q, m_num_q, query?query:"NULL");
        return false;
      }
// skip check whether all values are requested. This allows to just check if data are available (with step()),
// if not, write data, reset and read data
      sqlite3_reset(m_statement);
      m_last_step_result = -10;
      m_auto_step = true;
      stepInt();
      return true;
    }
  public:
// read result
//    bool readRow() ======================================================
// returns true if a row was found
// you can call this without parameters to check a row was found
// if parameters are provided, and a row was found, the parameters are filled with the content of the row
    bool readRow() {
      if (m_last_step_result == SQLITE_ROW) return true;
      if (!assertStatement("readRow")) return false;
      if (!allBindingValuesAvailable() ) {
        const char *query = sqlite3_sql(m_statement);
        esyslog("tvscraper: ERROR in cSql::readRow, not all query parameters provided, m_cur_q %d m_num_q %d query %s", m_cur_q, m_num_q, query?query:"NULL");
      }
      return false;
    }
    template<typename T, typename... Args>
    bool readRow(T &&arg1, Args&&... args) {
      if (!readRow() ) return false;
      *this >> std::forward<T>(arg1);
      return readRow(std::forward<Args>(args)...);
    }
    bool valueInitial(int col = -1) {
// true if no value was written to the cell
// if false, don't skip the column, so we can read the value
// if true, skip the column
      if (!preCheckRead(col) ) return true;
      if (col != -1) return sqlite3_column_type(m_statement, col) == SQLITE_NULL;
      if (sqlite3_column_type(m_statement, m_cur_col) != SQLITE_NULL) return false; // keep m_cur_col, so we can read the value
      m_cur_col++; // it is initial, so we dont look at the value any more -> skip
      return true;
    }
    cSql & skipCol() {
      if (preCheckRead() ) m_cur_col++;
      return *this;
    }
    cSql & operator>>(std::string *str) {
      if (preCheckRead() ) {
        if (str) *str = charPointerToString(sqlite3_column_text(m_statement, m_cur_col++));
        else m_cur_col++;
      }
      return *this;
    }
    cSql & operator>>(std::string &str) {
      if (preCheckRead() ) {
        str = charPointerToString(sqlite3_column_text(m_statement, m_cur_col++));
      }
      return *this;
    }
    cSql & operator>>(const char *&s) {
// The pointers returned are valid until sqlite3_step() or sqlite3_reset() or sqlite3_finalize() is called.
      if (preCheckRead() ) {
        s = reinterpret_cast<const char *>(sqlite3_column_text(m_statement, m_cur_col++));
      }
      return *this;
    }
    const char *getCharS(int col = -1) {
// The pointers returned are valid until sqlite3_step() or sqlite3_reset() or sqlite3_finalize() is called.
      if (col == -1)
        return preCheckRead()?reinterpret_cast<const char *>(sqlite3_column_text(m_statement, m_cur_col++)):NULL;
      return preCheckRead(col)?reinterpret_cast<const char *>(sqlite3_column_text(m_statement, col)):NULL;
    }
    std::string_view getStringView(int col = -1) {
      if (col == -1)
        return preCheckRead()?charPointerToStringView(sqlite3_column_text(m_statement, m_cur_col++)):string_view();
      return preCheckRead(col)?charPointerToStringView(sqlite3_column_text(m_statement, col)):string_view();
    }
// The pointers returned are valid until sqlite3_step() or sqlite3_reset() or sqlite3_finalize() is called.
      ///< s (char *) results will be valit until the prepareBind is called on this cSql,
      ///<            or sqlite3_step() or sqlite3_reset()       or this cSql is destroyed
    cSql & operator>>(int *i) {
      if (preCheckRead() ) {
        if (i) *i = static_cast<int>(sqlite3_column_int64(m_statement, m_cur_col++));
        else m_cur_col++;
      }
      return *this;
    }
    cSql & operator>>(unsigned int &i) {
      if (preCheckRead() ) i = static_cast<unsigned int>(sqlite3_column_int64(m_statement, m_cur_col++));
      return *this;
    }
    cSql & operator>>(int &i) {
      if (preCheckRead() ) i = static_cast<int>(sqlite3_column_int64(m_statement, m_cur_col++));
      return *this;
    }
    cSql & operator>>(long int &i) {
      if (preCheckRead() ) i = static_cast<long int>(sqlite3_column_int64(m_statement, m_cur_col++));
      return *this;
    }
    cSql & operator>>(long long int &i) {
      if (preCheckRead() ) i = static_cast<long long int>(sqlite3_column_int64(m_statement, m_cur_col++));
      return *this;
    }
    int getInt(int col = -1) {
      if (col == -1)
        return preCheckRead()?static_cast<int>(sqlite3_column_int64(m_statement, m_cur_col++)):0;
      return preCheckRead(col)?static_cast<int>(sqlite3_column_int64(m_statement, col)):0;
    }
    sqlite3_int64 getInt64(int col = -1) {
      if (col == -1)
        return preCheckRead()?sqlite3_column_int64(m_statement, m_cur_col++):0;
      return preCheckRead(col)?sqlite3_column_int64(m_statement, col):0;
    }
    cSql & operator>>(bool *b) {
      if (preCheckRead() ) {
        if (b) *b = sqlite3_column_int(m_statement, m_cur_col++);
        else m_cur_col++;
      }
      return *this;
    }
    cSql & operator>>(bool &b) {
      if (preCheckRead() ) b = sqlite3_column_int(m_statement, m_cur_col++);
      return *this;
    }
    cSql & operator>>(float *f) {
      if (preCheckRead() ) {
        if (f) *f = static_cast<float>(sqlite3_column_double(m_statement, m_cur_col++));
        else m_cur_col++;
      }
      return *this;
    }
    cSql & operator>>(float &f) {
      if (preCheckRead() ) f = static_cast<float>(sqlite3_column_double(m_statement, m_cur_col++));
      return *this;
    }
    cSql & operator>>(double &f) {
      if (preCheckRead() ) f = static_cast<double>(sqlite3_column_double(m_statement, m_cur_col++));
      return *this;
    }
    ~cSql() { finalize(); }

  private:
    bool allBindingValuesAvailable() const { return m_cur_q >= m_num_q; }
    bool assertStatement(const char *context) {
      if (m_statement) return true;
      esyslog("tvscraper: ERROR in cSql::%s, m_statement = NULL", context?context:"unknown");
      return false;
    }
    bool preCheckBind() {
//      if (m_cur_q == -1) prepareDelayed();
      if (!assertStatement("preCheckBind")) return false;
      if (allBindingValuesAvailable() ) {
        const char *query = sqlite3_sql(m_statement);
        esyslog("tvscraper: ERROR in cSql::preCheckBind, too many query parameters provided, m_cur_q %d m_num_q %d query %s", m_cur_q, m_num_q, query?query:"NULL");
        return false;
      }
      return true;
    }
    bool preCheckRead(int col = -1) {
      if (col == -1) col = m_cur_col;
      else m_explicit_col = true;
      if (m_last_step_result == SQLITE_ROW && col < m_num_cols) return true;
      if (!assertStatement("preCheckRead")) return false;
      const char *query = sqlite3_sql(m_statement);
      if (!allBindingValuesAvailable() )
        esyslog("tvscraper: ERROR in cSql::preCheckRead, not all query parameters provided, m_cur_q %d m_num_q %d query %s", m_cur_q, m_num_q, query?query:"NULL");
      if (col >= m_num_cols)
        esyslog("tvscraper: ERROR in cSql::preCheckRead, too many values requested, col %d m_num_cols %d query %s", col, m_num_cols, query?query:"NULL");
      return false;
    }
    void stepInt();
    bool assertAllValuesRequested(const char *context) {
      if (m_explicit_col) return true;  // this disables this check
      bool fail = m_last_step_result == SQLITE_ROW && m_cur_col < m_num_cols && m_cur_col != 0;
      if (!fail) return true;
      const char *query = sqlite3_sql(m_statement);
      esyslog("tvscraper: ERROR in %s, not all values requested, m_cur_col %d m_num_cols %d query %s",
                 context?context:"cSql::assertAllValuesRequested", m_cur_col, m_num_cols, query?query:"NULL");
      return false;
    }
    void finalize() {
      if (!m_statement) return;
      const char *query = sqlite3_sql(m_statement);
      if (!allBindingValuesAvailable() )
        esyslog("tvscraper: ERROR in cSql::finalize, not all query parameters provided, m_cur_q %d m_num_q %d query %s", m_cur_q, m_num_q, query?query:"NULL");
      if (m_last_step_result == -10) esyslog("tvscraper: ERROR in cSql::finalize, statement not executed query %s", query?query:"NULL");
      assertAllValuesRequested("cSql::finalize");
      sqlite3_finalize(m_statement);
      m_statement = NULL;
    }
    const cTVScraperDB *m_db;  // will be provided in constructor (mandatory, no constructur without DB)
    sqlite3_stmt *m_statement = NULL;
    int m_last_step_result;
    bool m_auto_step = false;
    int m_cur_q = -1;  // The leftmost SQL parameter has an index of 1; -1: prepareDelayed was not called
    int m_num_q;
    int m_cur_col;
    int m_num_cols;
    bool m_explicit_col = false;
};

class cTVScraperDB {
  friend class cSql;
private:
    sqlite3 *db;
    string dbPathPhys;
    string dbPathMem;
    bool inMem;
// low level methods for sql
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
    cTVScraperDB(const cTVScraperDB &) = delete;
    cTVScraperDB &operator= (const cTVScraperDB &) = delete;
    cTVScraperDB(void);
    virtual ~cTVScraperDB(void);
    bool Connect(void);
    void BackupToDisc(void);
// allow sql statments from outside this class
// see also class cSql
    template<typename... Args>
    void exec(const char *query, const Args&... args) const {
// just execute the sql statement, with the given parameters
      cSql sql(this, query, args...);
    }
    template<typename... Args>
    int queryInt(const char *query, const Args&... args) const {
// return 0 if the requested entry does not exist in database
// if the requested entry exists in database, but no value was written to the cell: return -1
      cSql sql(this, query, args...);
      if (!sql.readRow() ) return 0;
      if ( sql.valueInitial() ) return -1;
      return sql.getInt();
    }
    template<typename... Args>
    sqlite3_int64 queryInt64(const char *query, const Args&... args) const {
// return 0 if the requested entry does not exist in database or no value was written to the cell
      cSql sql(this, query, args...);
      return sql.getInt64();
    }
    template<typename... Args>
    std::string queryString(const char *query, const Args&... args) const {
// return "" if the requested entry does not exist in database
      cSql sql(this, query, args...);
      return std::string(sql.getStringView());
    }
// methods to make specific db changes
    void ClearOutdated() const;
    bool CheckMovieOutdatedEvents(int movieID, int season_number, int episode_number) const;
    bool CheckMovieOutdatedRecordings(int movieID, int season_number, int episode_number) const;
    void DeleteMovie(int movieID, string movieDir) const;
    int DeleteMovie(int movieID) const;
    void DeleteSeries(int seriesID, const string &movieDir, const string &seriesDir) const;
    int DeleteSeries(int seriesID) const;
    void InsertTv(int tvID, const string &name, const string &originalName, const string &overview, const string &firstAired, const string &networks, const string &genres, float popularity, float vote_average, int vote_count, const string &posterUrl, const string &fanartUrl, const string &IMDB_ID, const string &status, const set<int> &EpisodeRunTimes, const string &createdBy);
    void InsertTvEpisodeRunTimes(int tvID, const set<int> &EpisodeRunTimes);
    void InsertTv_s_e(int tvID, int season_number, int episode_number, int episode_absolute_number, int episode_id, const string &episode_name, const string &airDate, float vote_average, int vote_count, const string &episode_overview, const string &episode_guest_stars, const string &episode_director, const string &episode_writer, const string &episode_IMDB_ID, const string &episode_still_path, int episode_run_time);
    string GetEpisodeStillPath(int tvID, int seasonNumber, int episodeNumber) const;
    void TvSetEpisodesUpdated(int tvID);
    void TvSetNumberOfEpisodes(int tvID, int LastSeason, int NumberOfEpisodes);
    bool TvGetNumberOfEpisodes(int tvID, int &LastSeason, int &NumberOfEpisodes);
    void InsertEvent(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number);
    void DeleteEventOrRec(csEventOrRecording *sEventOrRecording);
    void InsertActor(int seriesID, const string &name, const string &role, const string &path);
    void InsertMovie(int movieID, const string &title, const string &original_title, const string &tagline, const string &overview, bool adult, int collection_id, const string &collection_name, int budget, int revenue, const string &genres, const string &homepage, const string &release_date, int runtime, float popularity, float vote_average, int vote_count, const string &productionCountries, const string &posterUrl, const string &fanartUrl, const string &IMDB_ID);
    void InsertMovieDirectorWriter(int movieID, const string &director, const string &writer);

    void InsertMovieActor(int movieID, int actorID, const string &name, const string &role, bool hasImage);
    void InsertTvActor(int tvID, int actorID, const string &name, const string &role, bool hasImage);
    void InsertTvEpisodeActor(int episodeID, int actorID, const string &name, const string &role, bool hasImage);
    bool MovieExists(int movieID);
    bool TvExists(int tvID);
    bool SearchTvEpisode(int tvID, const string &episode_search_name, int &season_number, int &episode_number);
    string GetStillPathTvEpisode(int tvID, int season_number, int episode_number);
    int InsertRecording2(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number);
    int SetRecording(csEventOrRecording *sEventOrRecording);
    void ClearRecordings2(void);
    bool CheckStartScrapping(int minimumDistance);
    bool GetMovieTvID(csEventOrRecording *sEventOrRecording, int &movie_tv_id, int &season_number, int &episode_number, int *runtime = NULL) const;
    bool GetMovieTvID(const cEvent *event, int &movie_tv_id, int &season_number, int &episode_number) const;
    bool GetMovieTvID(const cRecording *recording, int &movie_tv_id, int &season_number, int &episode_number) const;
    int GetMovieCollectionID(int movieID) const;
    std::string GetEpisodeName(int tvID, int seasonNumber, int episodeNumber) const;
    string GetDescriptionTv(int tvID);
    string GetDescriptionTv(int tvID, int seasonNumber, int episodeNumber);
    string GetDescriptionMovie(int movieID);
    bool GetMovie(int movieID, string &title, string &original_title, string &tagline, string &overview, bool &adult, int &collection_id, string &collection_name, int &budget, int &revenue, string &genres, string &homepage, string &release_date, int &runtime, float &popularity, float &vote_average);
    string GetMovieTagline(int movieID);
    int GetMovieRuntime(int movieID) const;
    bool GetTv(int tvID, string &name, string &overview, string &firstAired, string &networks, string &genres, float &popularity, float &vote_average, string &status);
    bool GetTv(int tvID, time_t &lastUpdated, string &status);
//    bool GetTvVote(int tvID, float &vote_average, int &vote_count);
    bool GetTvEpisode(int tvID, int seasonNumber, int episodeNumber, int &episodeID, string &name, string &airDate, float &vote_average, string &overview, string &episodeGuestStars);
    bool episodeNameUpdateRequired(int tvID, int langId);
    bool GetFromCache(const string &movieNameCache, csEventOrRecording *sEventOrRecording, sMovieOrTv &movieOrTv, bool baseNameEquShortText = false);
    void InsertCache(const string &movieNameCache, csEventOrRecording *sEventOrRecording, sMovieOrTv &movieOrTv, bool baseNameEquShortText = false);
    void DeleteOutdatedCache() const;
    int DeleteFromCache(const char *movieNameCache); // return number of deleted entries
    void insertTvMedia (int tvID, const string &path, eMediaType mediaType);
    void insertTvMediaSeasonPoster (int tvID, const string &path, eMediaType mediaType, int season);
    bool existsTvMedia (int tvID, const string &path);
    void deleteTvMedia (int tvID, bool movie = false, bool keepSeasonPoster = true) const;
    void AddActorDownload (int tvID, bool movie, int actorId, const string &actorPath);
    int findUnusedActorNumber (int seriesID);
    void DeleteActorDownload (int tvID, bool movie) const;
    int GetRuntime(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number);
    vector<int> getSimilarTvShows(int tv_id) const;
    void setSimilar(int tv_id1, int tv_id2);
};

#endif //__TVSCRAPER_TVSCRAPPERDB_H

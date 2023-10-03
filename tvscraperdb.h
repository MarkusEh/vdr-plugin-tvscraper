#ifndef __TVSCRAPER_TVSCRAPERDB_H
#define __TVSCRAPER_TVSCRAPERDB_H
#include <sqlite3.h>

// make sure that the lifetime of any string object you use to create cStringRef is long enough!
// for details (what is long enough), see cSql comment
class cStringRef {
  friend class cSql;
  public:
// The C++ Standard does guarantee that the lifetime of string literals is the lifetime of the program.
// -> implicit conversion of string literals is save
    template<std::size_t N>
    cStringRef(const char (&s)[N]): m_sv(s, N-1) {}
    explicit cStringRef(const char* s): m_sv(charPointerToStringView(s)) {}
    explicit cStringRef(const std::string_view &sv): m_sv(sv) {}
    explicit cStringRef(const std::string &s): m_sv(s) {}
  private:
    std::string_view m_sv;
};

class cTVScraperDB;
//typedef const char* cCharS;
class cSql {
// note for the query parameter: ============================================
//   the parameter type is cStringRef
//   string literals will implicitly converted to cStringRef, so you can just use string literals
//   for others: use cStringRef(.) to explicitly convert to cSringRef, after you checked that
//   the lifetime is long enough (should be longer than lifetime of the cSql instance)

//  execute sql statement   ==================================================
// for that, just create an instance and provide the paramters.
// the statement will be executed, as soon as all parameters are available
//   example 1
// cSql stmt(db, "delete movies where movie_id = ?", movieId);
//   example 2
// cSql stmt(db);
// stmt.finalizePrepareBindStep("delete movies where movie_id = ?", movieId);
// stmt.finalizePrepareBindStep("delete series where series = ?", seriesId);

//  execute sql statement, and read one row ==================================
// start as described in "execute sql statement"
// stmt.readRow() will return "true" if a row was found
// you can read the values with the same stmt.readRow() call,
// or later on with methods returning the values of given columns
// (starting with 0):
//     int getInt(int column), sqlite3_int64 getInt64(int column),
//     const char *getCharS(int column), std::string_view getStringView(int column)
//  use valueInitial(int column) to find out whether a value was written to this cell
//   example 1
// cSql stmt(db, "select movie_name from movies where movie_id = ?", movieId);
// if (!stmt.readRow(movieName) ) cout << "movie id not found\n";
// else cout << "movie name: " << movieName << "\n";
//   example 2
// cSql stmt(db, "select movie_name from movies where movie_id = ?", movieId);
// if (!stmt.readRow() ) cout << "movie id not found\n";
// else cout << "movie name: " << stmt.getCharS(0) << "\n";
//  RESTRICTIONS / BE CAREFULL!!!
// char* and string_view in result values:
//   invalidated by the next call to finalizePrepareBindStep, resetBindStep,resetBindStep,  resetStep, or destruction of the cSql instance

//  execute sql statement, and read several rows ==================================
// cSql has a forward iterator, and can be used in for loops
//   example
// for (cSql &stmt: cSql(db, "select movie_name from movies where movie_id = ?", movieId) {
//   cout << "movie name: " << stmt.getCharS(0) << "\n";
// }
//  RESTRICTIONS / BE CAREFULL!!!
// char* and string_view in result values:
//   valid only in this loop, invalid in the next iteration of the loop (invalidated by step).
//   also invalid after the loop ends / outside the loop.
// for the parameters, until C++23: make sure to not use
//   (char*, strings and string_view) temporaries as binding parameters
// for the parameters, since C++23: no restriction

//  execute sql statement, and write several rows =================================
//  example 1
// cSql stmt(db, "INSERT OR REPLACE INTO tv_episode_run_time (tv_id, episode_run_time) VALUES (?, ?);");
// for (const int &episodeRunTime: EpisodeRunTimes)
//   stmt.resetBindStep(tvID, episodeRunTime);
//  example 2
// cSql stmt(db"INSERT INTO best_numbers (number) VALUES (?);");
// for (int i: <vector<int>>{4,2}) stmt.resetBindStep(i);

//  RESTRICTIONS / BE CAREFULL!!!  :   Details for the parameters:
// char*, strings and string_view provided: the refernces must be valid until the object is destroyed.
// this shoudn't be an issue for requesting 0 or 1 rows,
//   as step is directly called after the binding parameters are available
//   after that, the binding parameters should not be needed any more
// for for(:) loops: until C++23: make sure to not use temporaries as binding parameters
// see https://en.cppreference.com/w/cpp/language/range-for
// If range-expression returns a temporary, its lifetime is extended until the end of the loop.
// until C++23: Lifetimes of all temporaries within range-expression are not extended
// since C++23: Lifetimes of all temporaries within range-expression are extended if they would otherwise be destroyed at the end of range-expression
// From the sqlite3 documentation (note: SQLITE_STATIC is passed):
//  SQLITE_STATIC may be passsed to indicate that the application remains responsible for disposing of the object.
//  In this case, the object and the provided pointer to it must remain valid until either
//    - the prepared statement is finalized or
//    - the same SQL parameter is bound to something else, whichever occurs sooner
// remark: we assume it is only needed until the call to step ...


  public:
    cSql(const cSql&) = delete;
    cSql &operator= (const cSql &) = delete;
    cSql(const cTVScraperDB *db): m_db(db) {}
    template<typename... Args>
    cSql(const cTVScraperDB *db, cStringRef query, Args&&... args): m_db(db), m_query(query.m_sv) {
      if (!m_db || m_query.empty() )
        esyslog("tvscraper: ERROR in cSql::cSql, m_db %s, query %.*s", m_db?"Avaliable":"Null", static_cast<int>(m_query.length()), m_query.data() );
      else prepareBindStep(std::forward<Args>(args)...);
    }
//  class iterator: public std::iterator<std::forward_iterator_tag, cSql, int, cSql*, cSql &>
    class iterator {
        cSql *m_sql = nullptr;
      public:
        explicit iterator(cSql *sql = nullptr): m_sql(sql) {}
        iterator& operator++() {
          if (!m_sql) return *this;
          m_sql->assertRvalLval(); // this needs to be checked in case of second call to stepInt()
          m_sql->stepInt();
          if (!m_sql->readRow() ) m_sql = nullptr;
          return *this;
        }
        bool operator!=(iterator other) const { return m_sql != other.m_sql; }
        cSql &operator*() const { return *m_sql; }
      };
//  class int_iterator: public std::iterator<std::forward_iterator_tag, int, int, int*, int>
    class int_iterator {
        cSql::iterator m_sql_iterator;
        std::vector<int>::const_iterator m_ints;
        bool m_sql;
      public:
        explicit int_iterator(cSql::iterator sql_iterator): m_sql_iterator(sql_iterator), m_sql(true) { }
        explicit int_iterator(std::vector<int>::const_iterator ints): m_ints(ints), m_sql(false) { }
        int_iterator& operator++() {
          if (m_sql) ++m_sql_iterator;
          else ++m_ints;
          return *this;
        }
        bool operator!=(int_iterator other) const {
          if (m_sql) return m_sql_iterator != other.m_sql_iterator;
          else return m_ints != other.m_ints;
        }
        int operator*() const {
          if (m_sql) return (*m_sql_iterator).getInt(0);
          else return *m_ints;
        }
      };
    iterator begin() {
      if (m_cur_row > 0 && !resetStep() ) return end();
      if (readRow() ) return iterator(this);
      return end();
    }
    iterator end() { return iterator(); }

// =======================================================================
//   resetBindStep:
//     pre-requisite:
//       - query was provided, e.g. during creation of the instance
//       - All bind parameters must be provided in this call
//     what it does:
//       - if there is already a prepared statment: reset statement
//       - otherwise, create statment
//       - bind parameters and step.
// =======================================================================

    template<typename... Args>
    cSql & resetBindStep(Args&&... args) {
      if (!m_db || m_query.empty() ) {
        esyslog("tvscraper: ERROR in cSql::resetBindStep, m_db %s, query %.*s", m_db?"Avaliable":"Null", static_cast<int>(m_query.length()), m_query.data());
        setFailed();
        return *this;
      }
      if (m_statement) {
        sqlite3_reset(m_statement);
        sqlite3_clear_bindings(m_statement);
      } else {
// no "old" m_statement
        if (!prepareInt()) return *this;
      }
      if (bind0(std::forward<Args>(args)...)) stepFirstRow();
      return *this;
    }

// =======================================================================
//   finalizePrepareBindStep:
//     - if query differs from old query, finalize old query.
//     - if query is same as old query, reset statement (better performance then finalize)
//     - if all bind parameters are provided, set bind parameters and step.
// =======================================================================

    template<typename... Args>
    cSql & finalizePrepareBindStep(cStringRef query, Args&&... args) {
      m_query = query.m_sv;
      if (!m_db || m_query.empty() ) {
        esyslog("tvscraper: ERROR in cSql::finalizePrepareBindStep, m_db %s, query %.*s", m_db?"Avaliable":"Null", static_cast<int>(m_query.length()), m_query.data() );
        setFailed();
        return *this;
      }
      if (!m_statement) return prepareBindStep(std::forward<Args>(args)...);
      const char *old_query = sqlite3_sql(m_statement);
      if (!old_query || m_query.compare(old_query) != 0) {
        finalize();  // if this cSql was used, close anything still open. Reset m_statement to NULL
        return prepareBindStep(std::forward<Args>(args)...);
      }
      if (config.enableDebug) esyslog("tvscraper: INFO in cSql::finalizePrepareBindStep, reset query %.*s", static_cast<int>(m_query.length()), m_query.data());
      return resetBindStep(std::forward<Args>(args)...);
    }
  private:
    template<typename... Args>
    cSql & prepareBindStep(Args&&... args) {
// check m_db != nullptr && m_query != empty() before calling this!
// also, there must be no m_statement
      if (sizeof...(Args) == 0 && m_query.find("?") != std::string_view::npos) return *this; // missing bind values -> delayed prepare
      if (!prepareInt()) return *this;
      if (bind0(std::forward<Args>(args)...)) stepFirstRow();
      return *this;
    }
    bool prepareInt();
// bind parameters. Note: All parameters must be available, this is checked
    template<typename... Args>
    bool bind0(Args&&... args) {
// return false in case of errors.
// otherwise, return true -> all parameters are bound
      if (m_num_q != sizeof...(Args)) {
// not all parameters given. this is an error. We need all parameters, to directly call step,
// so temporaries in parameters are still valid
        const char *query = sqlite3_sql(m_statement);
        esyslog("tvscraper: ERROR in cSql::bind0, wrong number of query parameters provided, required %d provided %d query %s", m_num_q, (int)sizeof...(Args), query?query:"NULL");
        setFailed();
        return false;
      }
      bindParameters(1, std::forward<Args>(args)...); // The leftmost SQL parameter has an index of 1
      if (m_statement) return true;
      return false;
    }
    void bindParameters(int col) {
      if (col - 1 == m_num_q) return;
// The leftmost SQL parameter has an index of 1
// in case of 0 parameters, this is called with col 1
// this should not happen, as it was already checked in bind0 ...
      const char *query = sqlite3_sql(m_statement);
      esyslog("tvscraper: ERROR in cSql::bindParameters, internal error, col %d m_num_q %d query %s", col, m_num_q, query?query:"NULL");
      setFailed();
    }
    template<typename T, typename... Args>
    void bindParameters(int col, T &&arg1, Args&&... args) {
      bind(col, std::forward<T>(arg1));
      bindParameters(++col, std::forward<Args>(args)...);
    }
    void bind(int col, long long int i) { sqlite3_bind_int64(m_statement, col, static_cast<sqlite3_int64>(i)); }
    void bind(int col, long int i) { sqlite3_bind_int64(m_statement, col, static_cast<sqlite3_int64>(i)); }
    void bind(int col, int i) { sqlite3_bind_int64(m_statement, col, static_cast<sqlite3_int64>(i)); }
    void bind(int col, unsigned long long int i) { sqlite3_bind_int64(m_statement, col, static_cast<sqlite3_int64>(i)); }
    void bind(int col, unsigned long int i) { sqlite3_bind_int64(m_statement, col, static_cast<sqlite3_int64>(i)); }
    void bind(int col, unsigned int i) { sqlite3_bind_int64(m_statement, col, static_cast<sqlite3_int64>(i)); }
    void bind(int col, double f) { sqlite3_bind_double(m_statement, col, f); }
    void bind(int col, const char* const&s) { m_lval = true; if(s) sqlite3_bind_text(m_statement, col, s, -1, SQLITE_STATIC); else sqlite3_bind_null(m_statement, col); }
    void bind(int col, const cStringRef &sref) { sqlite3_bind_text(m_statement, col, sref.m_sv.data(), sref.m_sv.length(), SQLITE_STATIC); }
    void bind(int col, const std::string_view &str) { m_lval = true; sqlite3_bind_text(m_statement, col, str.data(), str.length(), SQLITE_STATIC); }
    void bind(int col, const std::string &str) { m_lval = true; sqlite3_bind_text(m_statement, col, str.data(), str.length(), SQLITE_STATIC); }
    void bind(int col, const char* const&&s) { m_rval = true; if(s) sqlite3_bind_text(m_statement, col, s, -1, SQLITE_STATIC); else sqlite3_bind_null(m_statement, col); }
    void bind(int col, const std::string_view &&str) { m_rval = true; sqlite3_bind_text(m_statement, col, str.data(), str.length(), SQLITE_STATIC); }
    void bind(int col, const std::string &&str) { m_rval = true; sqlite3_bind_text(m_statement, col, str.data(), str.length(), SQLITE_STATIC); }
    void assertRvalLval();
  public:

// =======================================================================
//   resetStep:
//     pre-requisite:
//       - query and all bind parameters were provided, e.g. during creation of the instance
//       - the char* and string_view bind parameter values must still be valid !!!!
//     what it does:
//       - reset statement and and step.
// =======================================================================

    bool resetStep() {
      if (!assertStatement("reset")) return false;
      sqlite3_reset(m_statement);
      assertRvalLval(); // this needs to be checked in case of second call to step()
      stepFirstRow();
      return true;
    }

// =======================================================================
//   readRow:
//     pre-requisite:
//       - query and all bind parameters were provided, e.g. during creation of the instance
//     what it does:
//       - return true if a row was found in last step
//       - return false, otherwise
//       - if called with parameters, and a row was found in last step:
//           one parameter for each col must be provided
//           the parameters are filled with the content of the row
// =======================================================================

    template<typename... Args>
    bool readRow(Args&&... args) {
      if (m_last_step_result == SQLITE_ROW) {
        if (sizeof...(Args) == 0) return true;
        if (m_num_cols != sizeof...(Args)) {
          const char *query = sqlite3_sql(m_statement);
          esyslog("tvscraper: ERROR in cSql::readRow, wrong number of values requested, requested %d available %d query %s",
                   (int)sizeof...(Args), m_num_cols, query?query:"NULL");
        }
        readRow_int0(0, std::forward<Args>(args)...);
        return true;
      }
// there is no row. -> false
      assertStatement("readRow");  // report an error, if there is no m_statement
      return false;
    }
  private:
// readRow_int0:
    void readRow_int0(int col) {};
    template<typename T, typename... Args>
    void readRow_int0(int col, T &&arg1, Args&&... args) {
      readRow_int(col, std::forward<T>(arg1));
      readRow_int0(++col, std::forward<Args>(args)...);
    }
    void readRow_int(int col, std::string* const&str) {
      if (str) *str = charPointerToString(sqlite3_column_text(m_statement, col));
    }
    void readRow_int(int col, std::string &str) {
      str = charPointerToString(sqlite3_column_text(m_statement, col));
    }
    void readRow_int(int col, std::string_view &str) {
      str = charPointerToStringView(sqlite3_column_text(m_statement, col));
    }
// The pointers returned are valid until sqlite3_step() or sqlite3_reset() or sqlite3_finalize() is called.
    void readRow_int(int col, const char* &s) {
      s = reinterpret_cast<const char*>(sqlite3_column_text(m_statement, col));
    }
    void readRow_int(int col, int *i) {
      if (i) *i = static_cast<int>(sqlite3_column_int64(m_statement, col));
    }
    void readRow_int(int col, int &i) { i = static_cast<int>(sqlite3_column_int64(m_statement, col));}
    void readRow_int(int col, long int &i) { i = static_cast<long int>(sqlite3_column_int64(m_statement, col));}
    void readRow_int(int col, long long int &i) { i = static_cast<long long int>(sqlite3_column_int64(m_statement, col));}
    void readRow_int(int col, unsigned int &i) { i = static_cast<unsigned int>(sqlite3_column_int64(m_statement, col));}
    void readRow_int(int col, unsigned long int &i) { i = static_cast<unsigned long int>(sqlite3_column_int64(m_statement, col));}
    void readRow_int(int col, unsigned long long int &i) { i = static_cast<unsigned long long int>(sqlite3_column_int64(m_statement, col));}
    void readRow_int(int col, bool *b) {
      if (b) *b = sqlite3_column_int(m_statement, col);
    }
    void readRow_int(int col, bool &b) { b = sqlite3_column_int(m_statement, col);}
    void readRow_int(int col, float *f) {
      if (f) *f = static_cast<float>(sqlite3_column_double(m_statement, col));
    }
    void readRow_int(int col, float &f) { f = static_cast<float>(sqlite3_column_double(m_statement, col));}
    void readRow_int(int col, double &f) { f = sqlite3_column_double(m_statement, col);}
// explicitly read column col
  public:
    bool valueInitial(int col) {
// true if no value was written to the cell
      if (!preCheckRead(col) ) return true;
      return sqlite3_column_type(m_statement, col) == SQLITE_NULL;
    }
    const char *getCharS(int col) {
// The pointers returned are valid until sqlite3_step() or sqlite3_reset() or sqlite3_finalize() is called.
      return preCheckRead(col)?reinterpret_cast<const char *>(sqlite3_column_text(m_statement, col)):NULL;
    }
    std::string_view getStringView(int col) {
      return preCheckRead(col)?charPointerToStringView(sqlite3_column_text(m_statement, col)):string_view();
    }
// The pointers returned are valid until sqlite3_step() or sqlite3_reset() or sqlite3_finalize() is called.
      ///< s (char *) results will be valit until finalizePrepareBindStep or reset is called on this cSql,
      ///<            or this cSql is destroyed
    int getInt(int col) {
      return preCheckRead(col)?static_cast<int>(sqlite3_column_int64(m_statement, col)):0;
    }
    sqlite3_int64 getInt64(int col) {
      return preCheckRead(col)?sqlite3_column_int64(m_statement, col):0;
    }
    ~cSql() { finalize(); }

  private:
    bool assertStatement(const char *context) {
      if (m_statement) return true;
      esyslog("tvscraper: ERROR in cSql::%s, m_statement = NULL", context?context:"unknown");
      return false;
    }
    bool preCheckRead(int col) {
      if (m_last_step_result == SQLITE_ROW && col < m_num_cols) return true;
      if (!assertStatement("preCheckRead")) return false;
      const char *query = sqlite3_sql(m_statement);
      if (col >= m_num_cols)
        esyslog("tvscraper: ERROR in cSql::preCheckRead, too many values requested, col %d m_num_cols %d query %s", col, m_num_cols, query?query:"NULL");
      return false;
    }
    void stepFirstRow() {
// call this to get the first row
// for subsequent rows, call stepInt()
      m_cur_row = -1;
      m_last_step_result = -10;
      stepInt();
    }
    void stepInt();
    void setFailed() {
      sqlite3_finalize(m_statement);
      m_statement = nullptr;
      m_last_step_result = -10;
      m_cur_row = -1;
    }
    void finalize() {
      if (!m_statement) return;
      const char *query = sqlite3_sql(m_statement);
      if (m_last_step_result == -10) esyslog("tvscraper: ERROR in cSql::finalize, statement not executed query %s", query?query:"NULL");
      sqlite3_finalize(m_statement);
      m_cur_row = -1;
      m_last_step_result = -10;
      m_statement = nullptr;
    }
//    cSql(cSql &&) = default; // needs secial implementation
//    cSql &operator= (cSql &&) = default; // needs secial implementation
    const cTVScraperDB *m_db;  // will be provided in constructor (mandatory, no constructur without DB)
    sqlite3_stmt *m_statement = NULL;
    std::string_view m_query;
    int m_last_step_result = -10;
    int m_cur_row = -1; // +1 each time step is called
    int m_num_q;
    int m_num_cols;
    bool m_rval = false;
    bool m_lval = false;
};
class cSqlInt {
// only an integer is requested. Simplify loops
  public:
    template<typename... Args>
    cSqlInt(const cTVScraperDB *db, cStringRef query, Args&&... args):
      m_sql(db, query, std::forward<Args>(args)...) { }
    cSql::int_iterator begin() { return cSql::int_iterator(m_sql.begin() ); }
    const cSql::int_iterator end() { return cSql::int_iterator(m_sql.end() ); }
  private:
    cSql m_sql;
};

class cSqlGetSimilarTvShows {
// iterator over all similar tv shows
  public:
    cSqlGetSimilarTvShows(const cTVScraperDB *db, int tv_id);
    cSql::int_iterator begin() {
      return m_ints.empty()?cSql::int_iterator(m_sql.begin() ):cSql::int_iterator(m_ints.begin()); }
    const cSql::int_iterator end() {
      return m_ints.empty()?cSql::int_iterator(m_sql.end() ):cSql::int_iterator(m_ints.end()); }
  private:
    cSql m_sql;
    std::vector<int> m_ints;
};

using namespace std; 

// --- cTVScraperDB --------------------------------------------------------

class cTVScraperDB {
  friend class cSql;
private:
    sqlite3 *db;
    string dbPathPhys;
    string dbPathMem;
    bool inMem;
// low level methods for sql
    int printSqlite3Errmsg(std::string_view query) const;
// manipulate tables
    bool TableExists(const char *table);
    bool TableColumnExists(const char *table, const char *column);
    void AddCulumnIfNotExists(const char *table, const char *column, const char *type);
// others
    int LoadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave);
    bool CreateTables(void);
    void WriteRecordingInfo(const cRecording *recording, int movie_tv_id, int season_number, int episode_number);
public:
    cMeasureTime m_cache_time;
    cMeasureTime m_cache_episode_search_time;
    cMeasureTime m_cache_update_episode_time;
    cMeasureTime m_cache_update_similar_time;
    cSql m_stmt_cache1;
    cSql m_stmt_cache2;

    cTVScraperDB(const cTVScraperDB &) = delete;
    cTVScraperDB &operator= (const cTVScraperDB &) = delete;
    cTVScraperDB(void);
    virtual ~cTVScraperDB(void);
    bool Connect(void);
    void BackupToDisc(void);
// allow sql statments from outside this class
// see also class cSql
    template<typename... Args>
    void exec(const char *query, Args&&... args) const {
// just execute the sql statement, with the given parameters
      cSql sql(this, cStringRef(query), std::forward<Args>(args)...);
    }
    template<typename... Args>
    int queryInt(const char *query, Args&&... args) const {
// return 0 if the requested entry does not exist in database
// if the requested entry exists in database, but no value was written to the cell: return -1
      cSql sql(this, cStringRef(query), std::forward<Args>(args)...);
      if (!sql.readRow() ) return 0;
      if ( sql.valueInitial(0) ) return -1;
      return sql.getInt(0);
    }
    template<typename... Args>
    sqlite3_int64 queryInt64(const char *query, Args&&... args) const {
// return 0 if the requested entry does not exist in database or no value was written to the cell
      cSql sql(this, cStringRef(query), std::forward<Args>(args)...);
      return sql.readRow()?sql.getInt64(0):0;
    }
    template<typename... Args>
    std::string queryString(const char *query, Args&&... args) const {
// return "" if the requested entry does not exist in database
      cSql sql(this, cStringRef(query), std::forward<Args>(args)...);
      return sql.readRow()?std::string(sql.getStringView(0)):"";
    }
// methods to make specific db changes
    void ClearOutdated() const;
    bool CheckMovieOutdatedEvents(int movieID, int season_number, int episode_number) const;
    bool CheckMovieOutdatedRecordings(int movieID, int season_number, int episode_number) const;
    void DeleteMovie(int movieID, string movieDir) const;
    int DeleteMovie(int movieID) const;
    void DeleteSeries(int seriesID, const string &movieDir, const string &seriesDir) const;
    int DeleteSeries(int seriesID) const;
    void DeleteSeriesCache(int seriesID) const;
    void InsertTv(int tvID, const char *name, const char *originalName, const char *overview, const char *firstAired, const char *networks, const string &genres, float popularity, float vote_average, int vote_count, const char *posterUrl, const char *fanartUrl, const char *IMDB_ID, const char *status, const set<int> &EpisodeRunTimes, const char *createdBy, const char *languages);
    void InsertTvEpisodeRunTimes(int tvID, const set<int> &EpisodeRunTimes);
    void TvSetEpisodesUpdated(int tvID);
    void TvSetNumberOfEpisodes(int tvID, int LastSeason, int NumberOfEpisodes);
    bool TvGetNumberOfEpisodes(int tvID, int &LastSeason, int &NumberOfEpisodes);
    void InsertEvent(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number);
    void DeleteEventOrRec(csEventOrRecording *sEventOrRecording);
    void InsertActor(int seriesID, const char *name, const char *role, const char *path);
    void InsertMovie(int movieID, const char *title, const char *original_title, const char *tagline, const char *overview, bool adult, int collection_id, const char *collection_name, int budget, int revenue, const char *genres, const char *homepage, const char *release_date, int runtime, float popularity, float vote_average, int vote_count, const char *productionCountries, const char *posterUrl, const char *fanartUrl, const char *IMDB_ID);
    bool MovieExists(int movieID);
    bool TvExists(int tvID);
    int InsertRecording2(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number);
    int SetRecording(csEventOrRecording *sEventOrRecording);
    void ClearRecordings2(void);
    bool CheckStartScrapping(int minimumDistance);
    bool GetMovieTvID(const cEvent *event, int &movie_tv_id, int &season_number, int &episode_number, int *runtime = nullptr) const;
    bool GetMovieTvID(const cRecording *recording, int &movie_tv_id, int &season_number, int &episode_number, int *runtime = nullptr) const;
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
    void InsertCache(std::string_view movieNameCache, csEventOrRecording *sEventOrRecording, sMovieOrTv &movieOrTv, bool baseNameEquShortText = false);
    void DeleteOutdatedCache() const;
    int DeleteFromCache(const char *movieNameCache); // return number of deleted entries
    void insertTvMedia (int tvID, const char *path, eMediaType mediaType);
    void insertTvMediaSeasonPoster (int tvID, const char *path, eMediaType mediaType, int season);
    bool existsTvMedia (int tvID, const char *path);
    void deleteTvMedia (int tvID, bool movie = false, bool keepSeasonPoster = true) const;
    cSql addActorDownload () const { return cSql(this, "INSERT OR REPLACE INTO actor_download (movie_id, is_movie, actor_id, actor_path) VALUES (?, ?, ?, ?);");}
    void addActorDownload (int tvID, bool movie, int actorId, const char *actorPath, cSql *stmt = NULL);
    int findUnusedActorNumber (int seriesID);
    void DeleteActorDownload (int tvID, bool movie) const;
    int GetRuntime(csEventOrRecording *sEventOrRecording, int movie_tv_id, int season_number, int episode_number);
    void setSimilar(int tv_id1, int tv_id2);
};

#endif //__TVSCRAPER_TVSCRAPPERDB_H

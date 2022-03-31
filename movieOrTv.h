#include <cstddef>
#include <string>

enum ecMovieOrTvType {
    theTvdbSeries,
    theMoviedbSeries,
    theMoviedbMovie,
};

class cMovieOrTv {

public:
  virtual ~cMovieOrTv() {};
  virtual void DeleteMediaAndDb() = 0;
  virtual bool IsUsed() { return m_db->CheckMovieOutdatedEvents(dbID(), m_seasonNumber, m_episodeNumber) || m_db->CheckMovieOutdatedRecordings(dbID(), m_seasonNumber, m_episodeNumber); }
  void DeleteIfUnused() { if(!IsUsed()) DeleteMediaAndDb(); }
  virtual string getEpisodeName() { return "";}
  virtual std::size_t searchEpisode(const string &tvSearchEpisodeString) { return 0;}
// fill vdr service interface
  void clearScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv);
  virtual void getScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv, cImageServer *imageServer) = 0;
  void AddActors(std::vector<cActor> &actors, const char *sql, int id, const char *pathPart, int width = 421, int height = 632);
  virtual vector<cActor> GetActors() = 0;
// static methods
  static cMovieOrTv *getMovieOrTv(cTVScraperDB *db, int id, ecMovieOrTvType type);
  static cMovieOrTv *getMovieOrTv(cTVScraperDB *db, const sMovieOrTv &movieOrTv);
  static cMovieOrTv *getMovieOrTv(cTVScraperDB *db, csEventOrRecording *sEventOrRecording);
  static std::size_t searchEpisode(cTVScraperDB *db, sMovieOrTv &movieOrTv, const string &tvSearchEpisodeString);
  static void DeleteAllIfUnused(cTVScraperDB *db);
  static void DeleteAllIfUnused(const string &folder, ecMovieOrTvType type, cTVScraperDB *db);
protected:
  cTVScraperDB *m_db;
  int m_id;
  int m_seasonNumber;
  int m_episodeNumber;
  
  cMovieOrTv(cTVScraperDB *db, int id, int seasonNumber, int episodeNumber): m_db(db), m_id(id), m_seasonNumber(seasonNumber), m_episodeNumber(episodeNumber) {}
  virtual int dbID() = 0;
  virtual string bannerBaseUrl() = 0;
private:
};
// movie *********************************
class cMovieMoviedb : public cMovieOrTv {

public:
  cMovieMoviedb(cTVScraperDB *db, int id): cMovieOrTv(db, id, -100, 0) {}
  virtual void DeleteMediaAndDb();
  static void DeleteAllIfUnused(cTVScraperDB *db);
// fill vdr service interface
  virtual void getScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv, cImageServer *imageServer);
  virtual vector<cActor> GetActors();
protected:
  virtual int dbID() { return m_id; }
  virtual string bannerBaseUrl() { return "http://image.tmdb.org/t/p/w780/"; }
private:
};

// tv   *********************************
class cTv : public cMovieOrTv {

public:
  virtual void DeleteMediaAndDb() = 0;
  virtual string getEpisodeName() { return m_db->GetEpisodeName(dbID(), m_seasonNumber, m_episodeNumber);}
  virtual std::size_t searchEpisode(const string &tvSearchEpisodeString);
// fill vdr service interface
  virtual void getScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv, cImageServer *imageServer);
  virtual vector<cActor> GetActors() = 0;
protected:
  cTv(cTVScraperDB *db, int id, int seasonNumber, int episodeNumber): cMovieOrTv(db, id, seasonNumber,  episodeNumber) {}
  virtual int dbID() = 0;
  virtual string bannerBaseUrl() = 0;
private:
  std::size_t searchEpisode_int(const string &tvSearchEpisodeString);
  std::vector<cActor> getGuestStars(const char *str);
};

// tv, the moviedb ********************************************

class cTvMoviedb : public cTv {

public:
  cTvMoviedb(cTVScraperDB *db, int id, int seasonNumber=0, int episodeNumber=0): cTv(db, id, seasonNumber,  episodeNumber) {}
  virtual void DeleteMediaAndDb();
  virtual vector<cActor> GetActors();
protected:
  virtual int dbID() { return m_id; }
  virtual string bannerBaseUrl() { return "http://image.tmdb.org/t/p/w780/"; }
private:
};

// tv, the tvdb    *************************
class cTvTvdb : public cTv {

public:
  cTvTvdb(cTVScraperDB *db, int id, int seasonNumber=0, int episodeNumber=0): cTv(db, id, seasonNumber,  episodeNumber) {}
  virtual void DeleteMediaAndDb();
  virtual vector<cActor> GetActors();
protected:
  virtual int dbID() { return m_id * -1; }
  virtual string bannerBaseUrl() { return "https://thetvdb.com/banners/"; }
private:
};

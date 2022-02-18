#include <cstddef>

enum ecMovieOrTvType {
    theTvdbSeries,
    theMoviedbSeries,
    theMoviedbMovie,
};

class cMovieOrTv {

public:
  virtual ~cMovieOrTv() {};
  static cMovieOrTv *getMovieOrTv(int id, ecMovieOrTvType type);
  virtual void DeleteMediaAndDb(cTVScraperDB *db) = 0;
  virtual bool IsUsed(cTVScraperDB *db) = 0;
  void DeleteIfUnused(cTVScraperDB *db) { if(!IsUsed(db)) DeleteMediaAndDb(db); }
  static void DeleteAllIfUnused(cTVScraperDB *db);
  static void DeleteAllIfUnused(const string &folder, ecMovieOrTvType type, cTVScraperDB *db);
protected:
  int m_id;
  cMovieOrTv(int id): m_id(id) {}
private:
};
// movie
class cMovieMoviedb : public cMovieOrTv {

public:
  cMovieMoviedb(int id): cMovieOrTv(id) {}
  virtual void DeleteMediaAndDb(cTVScraperDB *db);
  virtual bool IsUsed(cTVScraperDB *db);
  static void DeleteAllIfUnused(cTVScraperDB *db);
protected:
private:
};
// tv

class cTv : public cMovieOrTv {

public:
  virtual void DeleteMediaAndDb(cTVScraperDB *db) = 0;
  virtual bool IsUsed(cTVScraperDB *db) = 0;
protected:
  cTv(int id): cMovieOrTv(id) {}
private:
};

// tv, the moviedb

class cTvMoviedb : public cTv {

public:
  cTvMoviedb(int id): cTv(id) {}
  virtual void DeleteMediaAndDb(cTVScraperDB *db);
  virtual bool IsUsed(cTVScraperDB *db);
protected:
private:
};
// tv, the tvdb

class cTvTvdb : public cTv {

public:
  cTvTvdb(int id): cTv(id) {}
  virtual void DeleteMediaAndDb(cTVScraperDB *db);
  virtual bool IsUsed(cTVScraperDB *db);
protected:
private:
};

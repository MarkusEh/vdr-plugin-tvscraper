#include <cstddef>
#include <string>
#include "services.h"
#include "images.h"

enum class ecMovieOrTvType {
  theTvdbSeries,
  theMoviedbSeries,
  theMoviedbMovie,
};

class cMovieOrTv {

public:
  virtual ~cMovieOrTv() {};
  virtual int dbID() = 0;
  int getSeason() { return m_seasonNumber; }
  int getEpisode() { return m_episodeNumber; }
  virtual void DeleteMediaAndDb() = 0;
  virtual bool IsUsed() { return m_db->CheckMovieOutdatedEvents(dbID(), m_seasonNumber, m_episodeNumber) || m_db->CheckMovieOutdatedRecordings(dbID(), m_seasonNumber, m_episodeNumber); }
  void DeleteIfUnused() { if(!IsUsed()) DeleteMediaAndDb(); }
  virtual string getEpisodeName() { return "";}
  virtual int searchEpisode(const string &tvSearchEpisodeString, const string &baseNameOrTitle) { return 1000;}
  virtual int searchEpisode(const string &tvSearchEpisodeString) { return 1000;}
// fill vdr service interface
  virtual tvType getType() const = 0;
  void clearScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv);
  virtual void getScraperOverview(cGetScraperOverview *scraperOverview) = 0;
  virtual void getScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv) = 0;
// Actors
  void AddActors(std::vector<cActor> &actors, const char *sql, int id, const char *pathPart, int width = 421, int height = 632);
  virtual vector<cActor> GetActors() = 0;
// Media
  std::vector<cTvMedia> getBanners();
  virtual vector<cTvMedia> getImages(eOrientation orientation);
  void copyImagesToRecordingFolder(const cRecording *recording);
  eImageLevel getSingleImageBestLO(cImageLevelsInt level, cOrientationsInt orientations, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  eImageLevel getSingleImageBestL(cImageLevelsInt level, eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImage(eImageLevel level, eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL) = 0;
// static methods
  static cMovieOrTv *getMovieOrTv(cTVScraperDB *db, int id, ecMovieOrTvType type);
  static cMovieOrTv *getMovieOrTv(cTVScraperDB *db, const sMovieOrTv &movieOrTv);
  static cMovieOrTv *getMovieOrTv(cTVScraperDB *db, csEventOrRecording *sEventOrRecording, int *runtime=NULL);
  static int searchEpisode(cTVScraperDB *db, sMovieOrTv &movieOrTv, const string &tvSearchEpisodeString, const string &baseNameOrTitle);
  static void DeleteAllIfUnused(cTVScraperDB *db);
  static void DeleteAllIfUnused(const string &folder, ecMovieOrTvType type, cTVScraperDB *db);
protected:
  cTVScraperDB *m_db;
  int m_id;
  int m_seasonNumber;
  int m_episodeNumber;
  
  cMovieOrTv(cTVScraperDB *db, int id, int seasonNumber, int episodeNumber): m_db(db), m_id(id), m_seasonNumber(seasonNumber), m_episodeNumber(episodeNumber) {}
  virtual string bannerBaseUrl() = 0;
// Media
  bool checkFullPath(const string        &sFullPath, string *relPath, string *fullPath, int *width, int *height, int i_width, int i_height);
  bool checkFullPath(const stringstream &ssFullPath, string *relPath, string *fullPath, int *width, int *height, int i_width, int i_height);
  bool checkRelPath (const stringstream &ssRelPath,  string *relPath, string *fullPath, int *width, int *height, int i_width, int i_height);
private:
};
// movie *********************************
class cMovieMoviedb : public cMovieOrTv {

public:
  cMovieMoviedb(cTVScraperDB *db, int id): cMovieOrTv(db, id, -100, 0) {}
  virtual int dbID() { return m_id; }
  virtual void DeleteMediaAndDb();
  static void DeleteAllIfUnused(cTVScraperDB *db);
// fill vdr service interface
  virtual tvType getType() const { return tMovie; }
  virtual void getScraperOverview(cGetScraperOverview *scraperOverview);
  virtual void getScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv);
  virtual vector<cActor> GetActors();
protected:
// media
  virtual string bannerBaseUrl() { return "http://image.tmdb.org/t/p/w780/"; }
  virtual bool getSingleImage(eImageLevel level, eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  bool getSingleImageMovie(     eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  bool getSingleImageCollection(eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
private:
  int m_collectionId = -1;
};

// tv   *********************************
class cTv : public cMovieOrTv {

public:
  virtual int dbID() = 0;
  virtual void DeleteMediaAndDb() = 0;
  virtual string getEpisodeName() { return m_db->GetEpisodeName(dbID(), m_seasonNumber, m_episodeNumber);}
  virtual int searchEpisode(const string &tvSearchEpisodeString);
  virtual int searchEpisode(const string &tvSearchEpisodeString, const string &baseNameOrTitle);
// fill vdr service interface
  virtual tvType getType() const { return tSeries; }
  virtual void getScraperOverview(cGetScraperOverview *scraperOverview);
  virtual void getScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv);
  virtual vector<cActor> GetActors() = 0;
// images
  virtual bool getSingleImage(eImageLevel level, eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageEpisode(  eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL) = 0;
  virtual bool getSingleImageSeason(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL) = 0;
  virtual bool getSingleImageTvShow(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL) = 0;
  virtual bool getSingleImageAnySeason(eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL) = 0;
protected:
  cTv(cTVScraperDB *db, int id, int seasonNumber, int episodeNumber): cMovieOrTv(db, id, seasonNumber,  episodeNumber) {}
  virtual string bannerBaseUrl() = 0;
private:
  int searchEpisode_int(string tvSearchEpisodeString);
  std::vector<cActor> getGuestStars(const char *str);
};

// tv, the moviedb ********************************************

class cTvMoviedb : public cTv {

public:
  cTvMoviedb(cTVScraperDB *db, int id, int seasonNumber=0, int episodeNumber=0): cTv(db, id, seasonNumber,  episodeNumber) {}
  virtual int dbID() { return m_id; }
  virtual void DeleteMediaAndDb();
  virtual vector<cActor> GetActors();
protected:
  virtual string bannerBaseUrl() { return "http://image.tmdb.org/t/p/w780/"; }
  virtual bool getSingleImageEpisode(  eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageSeason(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageTvShow(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageAnySeason(eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
private:
};

// tv, the tvdb    *************************
class cTvTvdb : public cTv {

public:
  cTvTvdb(cTVScraperDB *db, int id, int seasonNumber=0, int episodeNumber=0): cTv(db, id, seasonNumber,  episodeNumber) {}
  virtual int dbID() { return m_id * -1; }
  virtual void DeleteMediaAndDb();
  virtual vector<cActor> GetActors();
// Media
  virtual vector<cTvMedia> getImages(eOrientation orientation);
protected:
  virtual string bannerBaseUrl() { return "https://thetvdb.com/banners/"; }
  virtual bool getSingleImageEpisode(  eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageSeason(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageTvShow(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageAnySeason(eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
private:
};

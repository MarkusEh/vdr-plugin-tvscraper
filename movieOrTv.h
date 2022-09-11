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
  friend class cMovieMoviedb;
  friend class cTv;
public:
  virtual ~cMovieOrTv() {};
  virtual bool operator==(const cMovieOrTv &sec) const = 0;
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
  void copyImagesToRecordingFolder(const std::string &recordingFileName);
  virtual void DownloadImages(cMovieDBScraper *moviedbScraper, cTVDBScraper *tvdbScraper, const std::string &recordingFileName) = 0;
  eImageLevel getSingleImageBestLO(cImageLevelsInt level, cOrientationsInt orientations, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  eImageLevel getSingleImageBestL(cImageLevelsInt level, eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImage(eImageLevel level, eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL) = 0;
// static methods
  static cMovieOrTv *getMovieOrTv(const cTVScraperDB *db, int id, ecMovieOrTvType type);
  static cMovieOrTv *getMovieOrTv(const cTVScraperDB *db, const sMovieOrTv &movieOrTv);
  static cMovieOrTv *getMovieOrTv(const cTVScraperDB *db, csEventOrRecording *sEventOrRecording, int *runtime=NULL);
  static cMovieOrTv *getMovieOrTv(const cTVScraperDB *db, const cEvent *event);
  static cMovieOrTv *getMovieOrTv(const cTVScraperDB *db, const cRecording *recording);
  static int searchEpisode(const cTVScraperDB *db, sMovieOrTv &movieOrTv, const string &tvSearchEpisodeString, const string &baseNameOrTitle);
  static void CleanupTv_media(const cTVScraperDB *db);
  static void DeleteAllIfUnused(const cTVScraperDB *db);
  static void DeleteAllIfUnused(const string &folder, ecMovieOrTvType type, const cTVScraperDB *db);
protected:
  const cTVScraperDB *m_db;
  int m_id;
  int m_seasonNumber;
  int m_episodeNumber;
  
  cMovieOrTv(const cTVScraperDB *db, int id, int seasonNumber, int episodeNumber): m_db(db), m_id(id), m_seasonNumber(seasonNumber), m_episodeNumber(episodeNumber) {}
  virtual std::string imageUrl(const char *imageUrl) = 0;
// Media
  bool checkFullPath(const string        &sFullPath, string *relPath, string *fullPath, int *width, int *height, int i_width, int i_height);
  bool checkFullPath(const stringstream &ssFullPath, string *relPath, string *fullPath, int *width, int *height, int i_width, int i_height);
  bool checkRelPath (const stringstream &ssRelPath,  string *relPath, string *fullPath, int *width, int *height, int i_width, int i_height);
private:
};
// movie *********************************
class cMovieMoviedb : public cMovieOrTv {

public:
  cMovieMoviedb(const cTVScraperDB *db, int id): cMovieOrTv(db, id, -100, 0) {}
  virtual int dbID() { return m_id; }
  virtual bool operator==(const cMovieOrTv &sec) const { return getType() == sec.getType() && m_id == sec.m_id; }
  virtual void DeleteMediaAndDb();
  static void DeleteAllIfUnused(const cTVScraperDB *db);
// fill vdr service interface
  virtual tvType getType() const { return tMovie; }
  virtual void getScraperOverview(cGetScraperOverview *scraperOverview);
  virtual void getScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv);
  virtual vector<cActor> GetActors();
  virtual void DownloadImages(cMovieDBScraper *moviedbScraper, cTVDBScraper *tvdbScraper, const std::string &recordingFileName);
protected:
// media
  virtual std::string imageUrl(const char *imageUrl) { return std::string("http://image.tmdb.org/t/p/w780/") + imageUrl; };
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
  virtual bool operator==(const cMovieOrTv &sec) const { return getType() == sec.getType() && m_id == sec.m_id && m_seasonNumber == sec.m_seasonNumber && m_episodeNumber == sec.m_episodeNumber; }
  virtual bool IsUsed();
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
  cTv(const cTVScraperDB *db, int id, int seasonNumber, int episodeNumber): cMovieOrTv(db, id, seasonNumber,  episodeNumber) {}
  virtual std::string imageUrl(const char *imageUrl) = 0;
private:
  std::vector<cActor> getGuestStars(const char *str);
};

// tv, the moviedb ********************************************

class cTvMoviedb : public cTv {

public:
  cTvMoviedb(const cTVScraperDB *db, int id, int seasonNumber=0, int episodeNumber=0): cTv(db, id, seasonNumber,  episodeNumber) {}
  virtual int dbID() { return m_id; }
  virtual void DeleteMediaAndDb();
  virtual vector<cActor> GetActors();
  virtual void DownloadImages(cMovieDBScraper *moviedbScraper, cTVDBScraper *tvdbScraper, const std::string &recordingFileName);
protected:
  virtual std::string imageUrl(const char *imageUrl) { return std::string("http://image.tmdb.org/t/p/w780/") + imageUrl; };
  virtual bool getSingleImageEpisode(  eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageSeason(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageTvShow(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageAnySeason(eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
private:
};

// tv, the tvdb    *************************
class cTvTvdb : public cTv {

public:
  cTvTvdb(const cTVScraperDB *db, int id, int seasonNumber=0, int episodeNumber=0): cTv(db, id, seasonNumber,  episodeNumber) {}
  virtual int dbID() { return m_id * -1; }
  virtual void DeleteMediaAndDb();
  virtual vector<cActor> GetActors();
// Media
  virtual vector<cTvMedia> getImages(eOrientation orientation);
  virtual void DownloadImages(cMovieDBScraper *moviedbScraper, cTVDBScraper *tvdbScraper, const std::string &recordingFileName);
protected:
  virtual std::string imageUrl(const char *imageUrl);
  virtual bool getSingleImageEpisode(  eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageSeason(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageTvShow(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageAnySeason(eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
private:
};

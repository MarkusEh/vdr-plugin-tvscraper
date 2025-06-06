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
  friend class cScraperVideoImp;
public:
  virtual ~cMovieOrTv() {};
  virtual bool operator==(const cMovieOrTv &sec) const = 0;
  virtual int dbID() const = 0;
  virtual int langId(const cLanguage *lang) const { return lang?lang->m_id:0; }
  int getSeason() const { return m_seasonNumber; }
  int getEpisode() const { return m_episodeNumber; }
  virtual void DeleteMediaAndDb() = 0;
  virtual bool IsUsed() { return m_db->CheckMovieOutdatedEvents(dbID(), m_seasonNumber, m_episodeNumber) || m_db->CheckMovieOutdatedRecordings(dbID(), m_seasonNumber, m_episodeNumber); }
  void DeleteIfUnused() { if(!IsUsed()) DeleteMediaAndDb(); }
  virtual string getEpisodeName() { return "";}
  virtual int searchEpisode(cSv tvSearchEpisodeString, cSv baseNameOrTitle, const cYears &years, const cLanguage *lang, const char *shortText, cSv description) { return 1000;}
  virtual int searchEpisode(cSv tvSearchEpisodeString, const cYears &years, const cLanguage *lang, int season_guess, int episode_guess) { return 1000;}
// fill vdr service interface
  virtual tvType getType() const = 0;
  virtual bool getOverview(std::string *title, std::string *episodeName, std::string *releaseDate, std::string *imdbId, int *collectionId, std::string *collectionName = NULL) = 0;
  virtual std::string getCollectionName() { return ""; }
// Actors
  void AddActors(std::vector<cActor> &actors, const char *sql, int id, const char *pathPart, bool fullPath = true, int width = 421, int height = 632);
  virtual vector<cActor> GetActors(bool fullPath = true) = 0;
  virtual void AddGuestActors(std::vector<cActor> &actors, bool fullPath) = 0;
// Media
  std::vector<cTvMedia> getBanners();
  virtual vector<cTvMedia> getImages(eOrientation orientation, int maxImages = 3, bool fullPath = true);
  eImageLevel getSingleImageBestLO(cImageLevelsInt level, cOrientationsInt orientations, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  eImageLevel getSingleImageBestL(cImageLevelsInt level, eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImage(eImageLevel level, eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL) = 0;
// Media: Download, copy to rec folder, ...
  virtual iExtMovieTvDb *getExtMovieTvDb(cMovieDbMovieScraper *movieDbMovieScraper, cMovieDbTvScraper *movieDbTvScraper, cTvDbTvScraper *tvDbTvScraper) const = 0;
  virtual void DownloadImages(cMovieDbMovieScraper *movieDbMovieScraper, cMovieDbTvScraper *movieDbTvScraper, cTvDbTvScraper *tvDbTvScraper, const std::string &recordingFileName);
  bool copyImagesToRecordingFolder(const std::string &recordingFileName);

// static methods
  static cMovieOrTv *getMovieOrTv(const cTVScraperDB *db, int id, ecMovieOrTvType type);
  static cMovieOrTv *getMovieOrTv(const cTVScraperDB *db, const sMovieOrTv &movieOrTv);
  static cMovieOrTv *getMovieOrTv(const cTVScraperDB *db, const cEvent *event, const cRecording *recording, int *runtime=nullptr, int *duration_deviation=nullptr);
  static cMovieOrTv *getMovieOrTv(const cTVScraperDB *db, const cEvent *event, int *runtime=nullptr);
  static cMovieOrTv *getMovieOrTv(const cTVScraperDB *db, const cRecording *recording, int *runtime=nullptr, int *duration_deviation=nullptr);
  static int searchEpisode(const cTVScraperDB *db, sMovieOrTv &movieOrTv, iExtMovieTvDb *extMovieTvDb, cSv tvSearchEpisodeString, cSv baseNameOrTitle, const cYears &years, const cLanguage *lang, const char *shortText, cSv description);
  static void CleanupTv_media(const cTVScraperDB *db);
  static void DeleteAllIfUnused(const cTVScraperDB *db);
  static void DeleteAllIfUnused(const string &folder, ecMovieOrTvType type, const cTVScraperDB *db);
protected:
  const cTVScraperDB *m_db;
  int m_id;
  int m_seasonNumber;
  int m_episodeNumber;
  
  cMovieOrTv(const cTVScraperDB *db, int id, int seasonNumber, int episodeNumber): m_db(db), m_id(id), m_seasonNumber(seasonNumber), m_episodeNumber(episodeNumber) {}
private:
  int m_collectionId = -2; // -2: not checked
};
// movie *********************************
class cMovieMoviedb : public cMovieOrTv {

public:
  cMovieMoviedb(const cTVScraperDB *db, int id): cMovieOrTv(db, id, -100, 0) {}
  virtual int dbID() const { return m_id; }
  virtual bool operator==(const cMovieOrTv &sec) const { return getType() == sec.getType() && m_id == sec.m_id; }
  virtual void DeleteMediaAndDb();
  static void DeleteAllIfUnused(const cTVScraperDB *db);
// fill vdr service interface
  virtual tvType getType() const { return tMovie; }
  virtual bool getOverview(std::string *title, std::string *episodeName, std::string *releaseDate, std::string *imdbId, int *collectionId, std::string *collectionName = NULL);
  virtual std::string getCollectionName();
  virtual vector<cActor> GetActors(bool fullPath = true);
  virtual void AddGuestActors(std::vector<cActor> &actors, bool fullPath) {}  // add nothing, no guest stars in movies
  virtual iExtMovieTvDb *getExtMovieTvDb(cMovieDbMovieScraper *movieDbMovieScraper, cMovieDbTvScraper *movieDbTvScraper, cTvDbTvScraper *tvDbTvScraper) const { return movieDbMovieScraper; }
protected:
// media
  virtual bool getSingleImage(eImageLevel level, eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  bool getSingleImageMovie(     eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  bool getSingleImageCollection(eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
private:
};

// tv   *********************************
class cTv : public cMovieOrTv {

public:
  virtual int dbID() const = 0;
  virtual bool operator==(const cMovieOrTv &sec) const { return getType() == sec.getType() && m_id == sec.m_id && m_seasonNumber == sec.m_seasonNumber && m_episodeNumber == sec.m_episodeNumber; }
  virtual bool IsUsed();
  virtual void DeleteMediaAndDb() = 0;
  virtual string getEpisodeName() { return m_db->GetEpisodeName(dbID(), m_seasonNumber, m_episodeNumber);}
  virtual int searchEpisode(cSv tvSearchEpisodeString, const cYears &years, const cLanguage *lang, int season_guess, int episode_guess);
  virtual int searchEpisode(cSv tvSearchEpisodeString, cSv baseNameOrTitle, const cYears &years, const cLanguage *lang, const char *shortText, cSv description);
// fill vdr service interface
  virtual tvType getType() const { return tSeries; }
  virtual bool getOverview(std::string *title, std::string *episodeName, std::string *releaseDate, std::string *imdbId, int *collectionId, std::string *collectionName = NULL);
  virtual vector<cActor> GetActors(bool fullPath = true) = 0;
  virtual void AddGuestActors(std::vector<cActor> &actors, bool fullPath) = 0;
// images
  virtual bool getSingleImage(eImageLevel level, eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageEpisode(  eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL) = 0;
  virtual bool getSingleImageSeason(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL) = 0;
  virtual bool getSingleImageTvShow(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL) = 0;
  virtual bool getSingleImageAnySeason(eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL) = 0;
  virtual iExtMovieTvDb *getExtMovieTvDb(cMovieDbMovieScraper *movieDbMovieScraper, cMovieDbTvScraper *movieDbTvScraper, cTvDbTvScraper *tvDbTvScraper) const = 0;
protected:
  cTv(const cTVScraperDB *db, int id, int seasonNumber, int episodeNumber): cMovieOrTv(db, id, seasonNumber,  episodeNumber) {}
//private:
//  void addGuestStars(std::vector<cActor> &result, const char *str);
};

// tv, the moviedb ********************************************

class cTvMoviedb : public cTv {

public:
  cTvMoviedb(const cTVScraperDB *db, int id, int seasonNumber=0, int episodeNumber=0): cTv(db, id, seasonNumber,  episodeNumber) {}
  virtual int dbID() const { return m_id; }
  virtual void DeleteMediaAndDb();
  virtual vector<cActor> GetActors(bool fullPath = true);
  virtual void AddGuestActors(std::vector<cActor> &actors, bool fullPath);
  virtual iExtMovieTvDb *getExtMovieTvDb(cMovieDbMovieScraper *movieDbMovieScraper, cMovieDbTvScraper *movieDbTvScraper, cTvDbTvScraper *tvDbTvScraper) const { return movieDbTvScraper; }
protected:
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
  virtual int dbID() const { return -m_id; }
  virtual int langId(const cLanguage *lang) const { return lang?config.GetLanguageThetvdb(lang)->m_id:0; }
  virtual void DeleteMediaAndDb();
  virtual vector<cActor> GetActors(bool fullPath = true);
  virtual void AddGuestActors(std::vector<cActor> &actors, bool fullPath);
// Media
  virtual vector<cTvMedia> getImages(eOrientation orientation, int maxImages = 3, bool fullPath = true);
  virtual iExtMovieTvDb *getExtMovieTvDb(cMovieDbMovieScraper *movieDbMovieScraper, cMovieDbTvScraper *movieDbTvScraper, cTvDbTvScraper *tvDbTvScraper) const { return tvDbTvScraper; }
protected:
  virtual bool getSingleImageEpisode(  eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageSeason(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageTvShow(   eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
  virtual bool getSingleImageAnySeason(eOrientation orientation, string *relPath=NULL, string *fullPath=NULL, int *width=NULL, int *height=NULL);
private:
};

#include "services.h"
class cCharacterImp: public cCharacter {
  public:
    cCharacterImp(eCharacterType type, const std::string &personName, const std::string &characterName = "", const cTvMedia &image = cTvMedia() ):
      cCharacter(),
      m_type(type),
      m_personName(personName),
      m_characterName(characterName),
      m_image(image) {}
    virtual eCharacterType getType() { return m_type; }
    virtual const std::string &getPersonName() { return m_personName; }
    virtual const std::string &getCharacterName() { return m_characterName; }
    virtual const cTvMedia &getImage() { return m_image; }
    virtual ~cCharacterImp() {}
  private:
    const eCharacterType m_type;
    const std::string m_personName;     // "real name" of the person
    const std::string m_characterName;  // name of character in video
    const cTvMedia m_image;
};

class cScraperVideoImp: public cScraperVideo_v01 {
  public:
    cScraperVideoImp(const cEvent *event, const cRecording *recording, cTVScraperDB *db);
    virtual ~cScraperVideoImp();

// with the following methods you can request the "IDs of the identified object"
    virtual tvType getVideoType(); // if this is tNone, nothing was identified by scraper. Still, some information (image, duration deviation ...) might be available
    virtual int getDbId();
    virtual int getEpisodeNumber();
    virtual int getSeasonNumber();

    virtual bool getOverview(std::string *title, std::string *episodeName, std::string *releaseDate, int *runtime, std::string *imdbId, int *collectionId, std::string *collectionName); // return false if no scraper data are available

    virtual cTvMedia getImage(cImageLevels imageLevels = cImageLevels(), cOrientations imageOrientations = cOrientations(), bool fullPath = true);
    virtual std::vector<cTvMedia> getImages(eOrientation orientation, int maxImages = 3, bool fullPath = true);
    virtual std::vector<std::unique_ptr<cCharacter>> getCharacters(bool fullPath = true);
    virtual int getDurationDeviation();
    virtual int getHD();  // 0: SD. 1: HD. 2: UHD (note: might be changed in future. But: the higher, the better)
    virtual int getLanguage();
    virtual bool getMovieOrTv(std::string *title, std::string *originalTitle, std::string *tagline, std::string *overview, std::vector<std::string> *genres, std::string *homepage, std::string *releaseDate, bool *adult, int *runtime, float *popularity, float *voteAverage, int *voteCount, std::vector<std::string> *productionCountries, std::string *imdbId, int *budget, int *revenue, int *collectionId, std::string *collectionName, std::string *status, std::vector<std::string> *networks, int *lastSeason);

// episode specific ======================================
    virtual bool getEpisode(std::string *name, std::string *overview, int *absoluteNumber, std::string *firstAired, int *runtime, float *voteAverage, int *voteCount, std::string *imdbId);
// new methods in cScraperVideo_v01:
    bool has_changed(const cRecording *recording, int &runtime);
  private:
    cTvMedia getEpgImage(bool fullPath);
    tChannelID m_channelID;
    tEventID m_eventID = 0;
    int m_recordingID = -1;
    cTVScraperDB *m_db;
    int m_duration_deviation = -4; // -4: not checked
    int m_runtime = -2;  // runtime of episode / movie. Most reliable. -2: not checked. -1, 0: checked, but not available
    cMovieOrTv *m_movieOrTv = nullptr;
    bool isEpisodeIdentified();
};

cScraperVideoImp::cScraperVideoImp(const cEvent *event, const cRecording *recording, cTVScraperDB *db) {
  if (event && recording) {
    esyslog("tvscraper: ERROR calling vdr service interface, call->event && call->recording are provided. Please set one of these parameters to NULL");
    return;
  }
  m_db = db;
  m_movieOrTv = cMovieOrTv::getMovieOrTv(db, event, recording, &m_runtime, &m_duration_deviation);
  if (m_duration_deviation == -2) m_duration_deviation = 0;
// for duration_deviation:
// -2 : no data available -> in this case, we just return 0
  if (event) {
    m_channelID = event->ChannelID();
    m_eventID = event->EventID();
  }
  else if (recording) {
    m_recordingID = recording->Id();
    if (recording->Info() ) m_channelID = recording->Info()->ChannelID();
  }

}
cScraperVideoImp::~cScraperVideoImp() {
  if (m_movieOrTv) delete m_movieOrTv;
}

cTvMedia cScraperVideoImp::getEpgImage(bool fullPath) {
  if (m_recordingID >= 0) {
    LOCK_RECORDINGS_READ;
    return ::getEpgImage(nullptr, Recordings->GetById(m_recordingID), fullPath);
  }
  if (m_eventID != 0 & m_channelID.Valid() ) {
    LOCK_SCHEDULES_READ;
    const cSchedule *schedule = Schedules->GetSchedule(m_channelID);
    if (schedule) {
#if APIVERSNUM >= 20502
      return ::getEpgImage(schedule->GetEventById(m_eventID), nullptr, fullPath);
#else
      return ::getEpgImage(schedule->GetEvent    (m_eventID), nullptr, fullPath);
#endif
    }
  }
  return cTvMedia();
}

tvType cScraperVideoImp::getVideoType() {
// if this is tNone, nothing was identified by scraper. Still, some information (image, duration deviation ...) might be available
  if (!m_movieOrTv) return tNone;
  return m_movieOrTv->getType();
}
int cScraperVideoImp::getDbId() {
  if (!m_movieOrTv) return 0;
  return m_movieOrTv->dbID();
}

bool cScraperVideoImp::getOverview(std::string *title, std::string *episodeName, std::string *releaseDate, int *runtime, std::string *imdbId, int *collectionId, std::string *collectionName) {
  if (runtime) *runtime = m_runtime>0?m_runtime:0;
  if (m_movieOrTv) return m_movieOrTv->getOverview(title, episodeName, releaseDate, imdbId, collectionId, collectionName);
  return false;
}

int cScraperVideoImp::getDurationDeviation() {
  if (m_duration_deviation >= 0) return m_duration_deviation;
// for duration_deviation:
// -1 : currently no data available, but should be available later (recording length unkown as recording is ongoing or destination of cut/copy/move)
// -2 : no data available
// -3 : no data in recordings2 (i.e. no data in this cache)

  LOCK_RECORDINGS_READ;
  const cRecording* recording = Recordings->GetById(m_recordingID);
  if (!recording) { m_duration_deviation = 0; return 0; }
  csRecording sRecording(recording);

// note: m_runtime <=0 is considered as no runtime information by sRecording->durationDeviation(m_runtime)
  m_duration_deviation = sRecording.durationDeviation(m_runtime);
  if (m_duration_deviation >= 0) m_db->SetDurationDeviation(recording, m_duration_deviation);
  else m_duration_deviation = 0;

  return m_duration_deviation;
}

bool orientationsIncludesLandscape(cOrientationsInt imageOrientations) {
  for (eOrientation orientation = imageOrientations.popFirst(); orientation != eOrientation::none; orientation = imageOrientations.pop() )
    if (orientation == eOrientation::landscape) return true;
  return false;
}

cTvMedia cScraperVideoImp::getImage(cImageLevels imageLevels, cOrientations imageOrientations, bool fullPath)
{
  cTvMedia image;
  if (!m_movieOrTv) return orientationsIncludesLandscape(imageOrientations)?getEpgImage(fullPath):image;

  if (fullPath)
    m_movieOrTv->getSingleImageBestLO(imageLevels, imageOrientations, NULL, &image.path, &image.width, &image.height);
  else
    m_movieOrTv->getSingleImageBestLO(imageLevels, imageOrientations, &image.path, NULL, &image.width, &image.height);
  if (image.path.empty() && orientationsIncludesLandscape(imageOrientations)) return getEpgImage(fullPath);
  return image;
}

std::vector<cTvMedia> cScraperVideoImp::getImages(eOrientation orientation, int maxImages, bool fullPath) {
  std::vector<cTvMedia> result;
  if (!m_movieOrTv) return result;
  return m_movieOrTv->getImages(orientation, maxImages, fullPath);
}

int cScraperVideoImp::getHD() {
  if (m_channelID.Valid()) return config.ChannelHD(m_channelID);
  return -1;
}
int cScraperVideoImp::getLanguage() {
  if (m_channelID.Valid()) return config.GetLanguage_n(m_channelID);
  return -1;
}

std::vector<std::unique_ptr<cCharacter>> cScraperVideoImp::getCharacters(bool fullPath) {
  std::vector<std::unique_ptr<cCharacter>> result;
  if (!m_movieOrTv) return result;
  std::vector<cActor> guestActors;
  m_movieOrTv->AddGuestActors(guestActors, fullPath);
  for (const auto &actor: guestActors)
    result.push_back(std::make_unique<cCharacterImp>(eCharacterType::guestStar, actor.name, actor.role, actor.actorThumb));
  for (const auto &actor: m_movieOrTv->GetActors(fullPath))
    result.push_back(std::make_unique<cCharacterImp>(eCharacterType::actor, actor.name, actor.role, actor.actorThumb));
  if (m_movieOrTv->getType() != tMovie && !isEpisodeIdentified() ) return result;
  cSql stmt(m_db);
  if (m_movieOrTv->getType() == tMovie) {
    stmt.finalizePrepareBindStep("select movie_director, movie_writer from movie_runtime2 where movie_id = ?", m_movieOrTv->dbID());
  } else {
    const char *sql_e = "select episode_director, episode_writer from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?";
    stmt.finalizePrepareBindStep(cStringRef(sql_e), m_movieOrTv->dbID(), m_movieOrTv->getSeason(), m_movieOrTv->getEpisode());
  }
  const char *director_ = NULL;
  const char *writer_ = NULL;
  if (!stmt.readRow(director_, writer_)) return result;
  for (const auto director: cSplit(director_, '|', eSplitDelimBeginEnd::required))
    result.push_back(std::make_unique<cCharacterImp>(eCharacterType::director, std::string(director)));
  for (const auto &writer: cSplit(writer_, '|', eSplitDelimBeginEnd::required))
    result.push_back(std::make_unique<cCharacterImp>(eCharacterType::writer, std::string(writer)));
  return result;
}

bool cScraperVideoImp::getMovieOrTv(std::string *title, std::string *originalTitle, std::string *tagline, std::string *overview, std::vector<std::string> *genres, std::string *homepage, std::string *releaseDate, bool *adult, int *runtime, float *popularity, float *voteAverage, int *voteCount, std::vector<std::string> *productionCountries, std::string *imdbId, int *budget, int *revenue, int *collectionId, std::string *collectionName, std::string *status, std::vector<std::string> *networks, int *lastSeason)
{
// only for tMovie: adult, collection*, tagline, budget, revenue, homepage, productionCountries
// only for tSeries: status, networks, lastSeason
// for tSeries runtime is the episode runtime, while the other data are not episode related
  int l_collectionId = 0;
  if (!m_movieOrTv) return false;
  const char *productionCountries_ = NULL;
  const char *genres_ = NULL;
  const char *networks_ = NULL;
  cSql stmt(m_db);
  if (m_movieOrTv->getType() == tMovie) {
    const char *sql_m = "select movie_title, movie_original_title, movie_tagline, movie_overview, " \
      "movie_adult, movie_collection_id, movie_collection_name, " \
      "movie_budget, movie_revenue, movie_genres, movie_homepage, " \
      "movie_release_date, movie_popularity, movie_vote_average, movie_vote_count, " \
      "movie_production_countries, movie_IMDB_ID from movies3 where movie_id = ?";
    stmt.finalizePrepareBindStep(cStringRef(sql_m), m_movieOrTv->dbID());
    if (!stmt.readRow(title, originalTitle, tagline, overview, adult, l_collectionId, collectionName, budget, revenue, genres_, homepage, releaseDate, popularity, voteAverage, voteCount, productionCountries_, imdbId) ) return false;
    m_movieOrTv->m_collectionId = l_collectionId;
    if (status) *status = "";
    if (lastSeason) *lastSeason = 0;
  } else {
    if (m_movieOrTv->getType() == tSeries) {
      const char *sql_s = "select tv_name, tv_original_name, tv_overview, tv_first_air_date, " \
        "tv_networks, tv_genres, tv_popularity, tv_vote_average, tv_vote_count, " \
        "tv_IMDB_ID, tv_status, tv_last_season " \
        "from tv2 where tv_id = ?";
      stmt.finalizePrepareBindStep(cStringRef(sql_s), m_movieOrTv->dbID());
      if (!stmt.readRow(title, originalTitle, overview, releaseDate, networks_, genres_, popularity, voteAverage, voteCount, imdbId, status, lastSeason) ) return false;
      if (tagline) *tagline = "";
      if (homepage) *homepage = "";
      if (adult) *adult = false;
      if (budget) *budget = 0;
      if (revenue) *revenue = 0;
      if (collectionName) *collectionName = "";
    } else return false;
  }
  if (runtime) *runtime = m_runtime>0?m_runtime:0;
  if (collectionId) *collectionId = l_collectionId;
  if (genres) { genres->clear(); stringToVector(*genres, genres_); }
  if (productionCountries) { productionCountries->clear(); stringToVector(*productionCountries, productionCountries_); }
  if (networks) { networks->clear(); stringToVector(*networks, networks_); }
  return true;
}

// episode specific information  ===============================================
// only for tvShows / series!!!

int cScraperVideoImp::getEpisodeNumber() {
  if (!m_movieOrTv || m_movieOrTv->getType() != tSeries) return 0;
  return m_movieOrTv->getEpisode();
}
int cScraperVideoImp::getSeasonNumber() {
  if (!m_movieOrTv || m_movieOrTv->getType() != tSeries) return 0;
  return m_movieOrTv->getSeason();
}

bool cScraperVideoImp::isEpisodeIdentified() {
  return m_movieOrTv &&
         m_movieOrTv->getType() == tSeries &&
        (m_movieOrTv->getEpisode() != 0 || m_movieOrTv->getSeason() != 0);
}

bool cScraperVideoImp::getEpisode(std::string *name, std::string *overview, int *absoluteNumber, std::string *firstAired, int *runtime, float *voteAverage, int *voteCount, std::string *imdbId) {
  if (!isEpisodeIdentified() ) return false;

  int langInt = m_db->queryInt("SELECT tv_display_language FROM tv2 WHERE tv_id = ?", m_movieOrTv->dbID());
  if (langInt > 0) {
    cSql stmt_s_e(m_db,
      "SELECT episode_id, episode_absolute_number, episode_air_date, " \
      "episode_vote_average, episode_vote_count, episode_overview, " \
      "episode_IMDB_ID " \
      "FROM tv_s_e WHERE tv_id = ? AND season_number = ? AND episode_number = ?",
       m_movieOrTv->dbID(), m_movieOrTv->getSeason(), m_movieOrTv->getEpisode());
    int episode_id;
    if (!stmt_s_e.readRow(episode_id, absoluteNumber, firstAired, voteAverage, voteCount, overview, imdbId) ) return false;
    cSql stmt_name(m_db, "SELECT episode_name FROM tv_s_e_name2 WHERE episode_id = ? AND language_id = ?", episode_id, langInt);
    if (!stmt_name.readRow(name) ) {
      cSql stmt_name2(m_db, "SELECT episode_name FROM tv_s_e_name2 WHERE episode_id = ?", episode_id);
      if (!stmt_name2.readRow(name) ) *name = "no name found";
    }
  } else {
    cSql stmt(m_db,
      "SELECT episode_absolute_number, episode_name, episode_air_date, " \
      "episode_vote_average, episode_vote_count, episode_overview, " \
      "episode_IMDB_ID " \
      "FROM tv_s_e WHERE tv_id = ? AND season_number = ? AND episode_number = ?",
       m_movieOrTv->dbID(), m_movieOrTv->getSeason(), m_movieOrTv->getEpisode());
    if (!stmt.readRow(absoluteNumber, name, firstAired, voteAverage, voteCount, overview, imdbId) ) return false;
  }
  if (runtime) *runtime = m_runtime>0?m_runtime:0;
  return true;
}
bool cScraperVideoImp::has_changed(const cRecording *recording, int &runtime) {
  if (!recording) {
    esyslog("tvscraper: ERROR calling vdr service interface cScraperVideo_v01::has_changed, recording missing");
    return false;
  }
  if (m_recordingID != recording->Id()) {
    if (m_recordingID == -1)
      esyslog("tvscraper: ERROR calling vdr service interface cScraperVideo_v01::has_changed, but this was not created with a recording %s", recording->Name() );
    else
      esyslog("tvscraper: ERROR calling vdr service interface cScraperVideo_v01::has_changed, but this was not created with a different recording %s", recording->Name() );
    return false;
  }
  m_runtime = -2;
  m_duration_deviation = -4;
  int l_movie_tv_id, l_season_number, l_episode_number;
  if (!m_db->GetMovieTvID(recording, l_movie_tv_id, l_season_number, l_episode_number, &m_runtime, &m_duration_deviation)) {
    if (m_duration_deviation == -2) m_duration_deviation = 0;
    if (!m_movieOrTv) return false;   // there is nothing assigned, and this did not change
    delete m_movieOrTv;
    m_movieOrTv = nullptr;
    runtime = 0;
    return true;
  }
  if (m_duration_deviation == -2) m_duration_deviation = 0;
  runtime = m_runtime>0?m_runtime:0;
  if (l_season_number == -100) {
// movie
    if (m_movieOrTv && l_movie_tv_id == m_movieOrTv->dbID()) return false; // nothing changed
    if (m_movieOrTv) delete m_movieOrTv;
    m_movieOrTv = new cMovieMoviedb(m_db, l_movie_tv_id);
    return true;
  }
// series
  if (m_movieOrTv && l_movie_tv_id == m_movieOrTv->dbID() &&
      l_season_number == m_movieOrTv->getSeason() && l_episode_number == m_movieOrTv->getEpisode() ) return false; // nothing changed
  if (m_movieOrTv) delete m_movieOrTv;
  if (l_movie_tv_id > 0) m_movieOrTv = new cTvMoviedb(m_db, l_movie_tv_id, l_season_number, l_episode_number);
        else             m_movieOrTv = new cTvTvdb(m_db,   -l_movie_tv_id, l_season_number, l_episode_number);
  return true;
}

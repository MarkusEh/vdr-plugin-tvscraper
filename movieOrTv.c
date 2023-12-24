#include "movieOrTv.h"
#include <fstream>
#include <iostream>
#include <filesystem>

// implemntation of cMovieOrTv  *********************

void cMovieOrTv::AddActors(std::vector<cActor> &actors, const char *sql, int id, const char *pathPart, bool fullPath, int width, int height) {
// adds the actors found with sql&id to the list of actors
// works for all actors found in themoviedb (movie & tv actors). Not for actors in thetvdb
  cActor actor;
  actor.actorThumb.width = width;
  actor.actorThumb.height = height;
  int actorId;
  int hasImage = 0;
  for (cSql &sql: cSql(m_db, cStringRef(sql), id) ) {
    sql.readRow(actorId, actor.name, actor.role, hasImage);
    if (hasImage) {
      CONCATENATE(path, config.GetBaseDir(), pathPart, actorId, ".jpg");
      if (FileExistsImg(path)) {
        if (fullPath) actor.actorThumb.path = path;
        else actor.actorThumb.path = path + config.GetBaseDirLen();
      } else {
        actor.actorThumb.path = "";
      }
    } else {
      actor.actorThumb.path = "";
    }
    actors.push_back(actor);
  }
}

// images
vector<cTvMedia> cMovieOrTv::getImages(eOrientation orientation, int maxImages, bool fullPath) {
  vector<cTvMedia> images;
  cTvMedia image;
  cImageLevelsInt level;
  if (getType() == tMovie) level = cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection);
  else level = cImageLevelsInt(eImageLevel::tvShowCollection, eImageLevel::seasonMovie, eImageLevel::anySeasonCollection);
  if (fullPath) {
    if (getSingleImageBestL(level, orientation, NULL, &image.path, &image.width, &image.height) != eImageLevel::none)
      images.push_back(image);
  } else {
    if (getSingleImageBestL(level, orientation, &image.path, NULL, &image.width, &image.height) != eImageLevel::none)
      images.push_back(image);
  }
  return images;
}

std::vector<cTvMedia> cMovieOrTv::getBanners() {
  std::vector<cTvMedia> banners;
  cTvMedia media;
  if (eImageLevel::none != getSingleImageBestLO(
    cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection),
    cOrientationsInt(eOrientation::banner, eOrientation::landscape),
    NULL, &media.path, &media.width, &media.height)) banners.push_back(media);
  return banners;
}

void cMovieOrTv::DownloadImages(cMovieDbMovieScraper *movieDbMovieScraper, cMovieDbTvScraper *movieDbTvScraper, cTvDbTvScraper *tvDbTvScraper, const std::string &recordingFileName) {
  getExtMovieTvDb(movieDbMovieScraper, movieDbTvScraper, tvDbTvScraper)->downloadImages(std::abs(m_id), m_seasonNumber, m_episodeNumber);
  copyImagesToRecordingFolder(recordingFileName);
}

bool cMovieOrTv::copyImagesToRecordingFolder(const std::string &recordingFileName) {
// return true if a fanart was copied
  if (recordingFileName.empty() ) return true;
  std::string path;
  cImageLevelsInt level(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection);
  if (getSingleImageBestL(level, eOrientation::portrait, NULL, &path) != eImageLevel::none)
    CopyFileImg(path, recordingFileName + "/poster.jpg" );
  if (getSingleImageBestL(level, eOrientation::landscape, NULL, &path) != eImageLevel::none) {
    CopyFileImg(path, recordingFileName + "/fanart.jpg" );
    return true;
  }
  return false;
}

eImageLevel cMovieOrTv::getSingleImageBestLO(cImageLevelsInt level, cOrientationsInt orientations, string *relPath, string *fullPath, int *width, int *height) {
// see comment of getSingleImageBestL
// check for images in other orientations, if no image is available in the requested one
  for (eOrientation orientation = orientations.popFirst(); orientation != eOrientation::none; orientation = orientations.pop() ) {
    eImageLevel level_found = getSingleImageBestL(level, orientation, relPath, fullPath, width, height);
    if (level_found != eImageLevel::none) return level_found;
  }

  return eImageLevel::none;
}

eImageLevel cMovieOrTv::getSingleImageBestL(cImageLevelsInt level, eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
// return 0 if no image was found (in this case, fullPath = relPath = "", width = height = 0;
// otherwise, return the "level" of the found image
  if (width) *width = 0;
  if (height) *height = 0;
  if (relPath) *relPath = "";
  if (fullPath) *fullPath = "";
  cImageLevelsInt level_c = (getType() == tSeries)?level:level.removeDuplicates(cImageLevelsInt::equalMovies);
  for (eImageLevel lv_to_add = level_c.popFirst(); lv_to_add != eImageLevel::none; lv_to_add = level.pop())
    if (getSingleImage(lv_to_add, orientation, relPath, fullPath, width, height) ) return lv_to_add;
  return eImageLevel::none;
}

bool cMovieMoviedb::getSingleImage(eImageLevel level, eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
// return false if no image with the given level was found
  if (width) *width = 0;
  if (height) *height = 0;
  if (relPath) *relPath = "";
  if (fullPath) *fullPath = "";
  switch (level) {
    case eImageLevel::episodeMovie:
    case eImageLevel::seasonMovie: return getSingleImageMovie(orientation, relPath, fullPath, width, height);
    case eImageLevel::tvShowCollection:
    case eImageLevel::anySeasonCollection: return getSingleImageCollection(orientation, relPath, fullPath, width, height);
    default: return false;
  }
}

bool cTv::getSingleImage(eImageLevel level, eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
// return false if no image with the given level was found
  if (width) *width = 0;
  if (height) *height = 0;
  if (relPath) *relPath = "";
  if (fullPath) *fullPath = "";
  switch (level) {
    case eImageLevel::episodeMovie: return getSingleImageEpisode(orientation, relPath, fullPath, width, height);
    case eImageLevel::seasonMovie: return getSingleImageSeason(orientation, relPath, fullPath, width, height);
    case eImageLevel::tvShowCollection: return getSingleImageTvShow(orientation, relPath, fullPath, width, height);
    case eImageLevel::anySeasonCollection: return getSingleImageAnySeason(orientation, relPath, fullPath, width, height);
    default: return false;
  }
}

// implementation of cMovieMoviedb  *********************
void cMovieMoviedb::DeleteMediaAndDb() {
  m_db->DeleteMovie(m_id);
}

std::string cMovieMoviedb::getCollectionName() {
  const char *sql = "select movie_collection_name from movies3 where movie_id = ?";
  return m_db->queryString(sql, dbID() );
}

bool cMovieMoviedb::getOverview(std::string *title, std::string *episodeName, std::string *releaseDate, int *runtime, std::string *imdbId, int *collectionId, std::string *collectionName) {
// return false if no data are available. In this case, paramters will NOT change
  m_collectionId = 0;
  cUseStmt_select_movies3_overview stmt(m_db);
  stmt.stmt()->resetBindStep(dbID() );
  if (!stmt.stmt()->readRow(title, m_collectionId, collectionName, releaseDate, runtime, imdbId) ) return false;
  if (collectionId) *collectionId = m_collectionId;
  if (episodeName) *episodeName = "";
  return true;
}

std::vector<cActor> cMovieMoviedb::GetActors(bool fullPath) {
  std::vector<cActor> actors;
  const char *sql = "select actors.actor_id, actor_name, actor_role, actor_has_image from actors, actor_movie " \
                     "where actor_movie.actor_id = actors.actor_id and actor_movie.movie_id = ?";
  AddActors(actors, sql,  dbID(), "movies/actors/actor_", fullPath);
  return actors;
}

bool checkPath(const char *path, string *relPath, string *fullPath, int *width, int *height, int i_width, int i_height) {
  if (!FileExistsImg(path) ) return false;
  if (fullPath) *fullPath = path;
  if (relPath) *relPath = path + config.GetBaseDirLen();
  if (width) *width = i_width;
  if (height) *height = i_height;
  return true;
}
bool checkPathC(string *relPath, string *fullPath, int *width, int *height, int i_width, int i_height, const std::string &s1, const char *s2) {
  CONCATENATE(buffer, s1, s2);
  return checkPath(buffer, relPath, fullPath, width, height, i_width, i_height);
}
bool checkPathC(string *relPath, string *fullPath, int *width, int *height, int i_width, int i_height, const char *s1, unsigned int i, const char *s2) {
  CONCATENATE(buffer, s1, i, s2);
  return checkPath(buffer, relPath, fullPath, width, height, i_width, i_height);
}
bool checkPathC(string *relPath, string *fullPath, int *width, int *height, int i_width, int i_height, const std::string &s1, unsigned int i, const char *s2) {
  CONCATENATE(buffer, s1, i, s2);
  return checkPath(buffer, relPath, fullPath, width, height, i_width, i_height);
}

bool getSingleImageMovie(string *relPath, string *fullPath, int *width, int *height, eOrientation orientation, const char *basePath, unsigned int id) {
  switch (orientation) {
    case eOrientation::portrait:
      return checkPathC(relPath, fullPath, width, height, 500, 750, basePath, id, "_poster.jpg");
    case eOrientation::landscape:
      return checkPathC(relPath, fullPath, width, height, 1280,  720, basePath, id, "_backdrop.jpg");
    default: return false;
  }
}
// movie images *****************************
bool cMovieMoviedb::getSingleImageMovie(eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
  return ::getSingleImageMovie(relPath, fullPath, width, height, orientation, config.GetBaseDirMovies().c_str(), m_id);
}

bool cMovieMoviedb::getSingleImageCollection(eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
// movie collection image
  if (m_collectionId <  0) m_collectionId = m_db->GetMovieCollectionID(m_id);
  if (m_collectionId <  0) m_collectionId = 0;
  if (m_collectionId == 0) return false;
  return ::getSingleImageMovie(relPath, fullPath, width, height, orientation, config.GetBaseDirMovieCollections().c_str(), m_collectionId);
}

// implemntation of cTv  *********************
bool cTv::IsUsed() {
  for (int id: cSqlGetSimilarTvShows(m_db, dbID()))
  {
    if (m_db->CheckMovieOutdatedEvents(id, m_seasonNumber, m_episodeNumber) ) return true;
    if (m_db->CheckMovieOutdatedRecordings(id, m_seasonNumber, m_episodeNumber)) return true;
  }
  return config.TV_ShowSelected(dbID());
}

int cTv::searchEpisode(cSv tvSearchEpisodeString, cSv baseNameOrTitle, const cYears &years, const cLanguage *lang, const char *shortText, const char *description) {
// return 1000, if no match was found
// otherwise, distance
  int season_guess = -1; // -1: no season found (still, there might be an episode ..)
  int episode_guess = 0; //  0: nothing found
  if (description) episodeSEp(season_guess, episode_guess, description, "S", "Ep");
  if (episode_guess == 0 && shortText) episodeSEp(season_guess, episode_guess, shortText, "S", "Ep");
  int distance = searchEpisode(tvSearchEpisodeString, years, lang, season_guess, episode_guess);
  if (distance > 500 && episode_guess > 0 && season_guess >= 0 &&
      (season_guess != m_seasonNumber || episode_guess != m_episodeNumber)) {
// pattern "S2 Ep12" found, and no or bad match with episode name
// -> Use pattern
    m_seasonNumber  = season_guess;
    m_episodeNumber = episode_guess;
    cSql sql_year(m_db, "select episode_air_date from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?", dbID(), m_seasonNumber, m_episodeNumber);
    if (!sql_year.readRow()) return 950; // season/episode does not exist in DB. Very weak indikator
    const char *episode_air_date = sql_year.getCharS(0);
    if (!episode_air_date) return 800;   // season/episode does exist in DB. No year information. Weak indikator
    int year_found = years.find2(cYears::yearToInt(cSv(episode_air_date, 4)));
    if (year_found == 2) return 600; // exact match
    if (year_found == 1) return 700; // near match
    return 800; // no year match
  }
  if (distance != 1000) return distance;
// if no episode number found (episode_guess == 0), try episode number as part of title
  if (episode_guess == 0) episode_guess = NumberInLastPartWithPS(baseNameOrTitle);
  if (episode_guess != 0) {
// we have a guess for the pepisode nuimber. Find the season number
    cSql sqlI  (m_db, "select season_number, episode_number from tv_s_e where tv_id = ? and episode_number = ?");
    cSql sqlI_a(m_db, "select season_number, episode_number from tv_s_e where tv_id = ? and episode_absolute_number = ?");
    sqlI.resetBindStep  (dbID(), episode_guess);
    sqlI_a.resetBindStep(dbID(), episode_guess);
    if (!sqlI.readRow() && !sqlI_a.readRow() ) return 1000;  // nothing found without years, so there will be nothing with years ...
    cSql sqlI_y (m_db, "select season_number, episode_number from tv_s_e where tv_id = ? and episode_number = ? and episode_air_date like ?");
    cSql sqlI_ya(m_db, "select season_number, episode_number from tv_s_e where tv_id = ? and episode_absolute_number = ? and episode_air_date like ?");
    for (int year: years) {
      char year_s[] = "    %";
      for (int i = 3; i >= 0; i--) {
        year_s[i] = '0' + year % 10;
        year /= 10;
      }
      if (sqlI.readRow()   &&  sqlI_y.resetBindStep(dbID(), episode_guess, year_s).readRow(m_seasonNumber, m_episodeNumber)) return 700;
      if (sqlI_a.readRow() && sqlI_ya.resetBindStep(dbID(), episode_guess, year_s).readRow(m_seasonNumber, m_episodeNumber)) return 700;
    }
// no match with year found, try without year
// note: this is a very week indicator that the right TV show was choosen. So, even if there is a match, return a high distance (950)
    for (auto &sqlI_l: sqlI) if (sqlI_l.getInt(0) != 0) {
// avoid season 0, this is trailer, ...
      m_seasonNumber  = sqlI_l.getInt(0);
      m_episodeNumber = sqlI_l.getInt(1);
      return 950;
    }
    if (sqlI_a.readRow(m_seasonNumber, m_episodeNumber)) return 950;
    sqlI.resetStep();
    if (sqlI.readRow  (m_seasonNumber, m_episodeNumber)) return 950;
  }
  return 1000;
}

int cTv::searchEpisode(cSv tvSearchEpisodeString_i, const cYears &years, const cLanguage *lang, int season_guess, int episode_guess) {
// search tvSearchEpisodeString_i in db with episode names in language lang
// return 1000 and set m_seasonNumber = m_episodeNumber = 0, if no match was found
// otherwise, set m_seasonNumber and m_episodeNumber and return distance
  bool debug = dbID() == 197649 || dbID() == 197648 || tvSearchEpisodeString_i.length() > 200;
  debug = debug || (tvSearchEpisodeString_i.length() > 0 && tvSearchEpisodeString_i[0] == 0);
  debug = debug || (tvSearchEpisodeString_i.length() > 1 && tvSearchEpisodeString_i[1] == 0);
  debug = dbID() == -248837;
  debug = false;
  if (debug) esyslog("tvscraper:DEBUG cTv::searchEpisode search string_i length %zu, \"%.*s\", dbid %i", tvSearchEpisodeString_i.length(), std::min(100, static_cast<int>(tvSearchEpisodeString_i.length())), tvSearchEpisodeString_i.data(), dbID());
  cNormedString tvSearchEpisodeString(tvSearchEpisodeString_i);
  if (debug) esyslog("tvscraper:DEBUG cTv::searchEpisode search string \"%s\", dbid %i", tvSearchEpisodeString.m_normedString.c_str(), dbID());
  int best_distance = 1000;
  int best_season = 0;
  int best_episode = 0;
  cSql statement_tvdb(m_db, "SELECT tv_s_e_name2.episode_name, tv_s_e.season_number, tv_s_e.episode_number, tv_s_e.episode_air_date FROM tv_s_e, tv_s_e_name2 WHERE tv_s_e_name2.episode_id = tv_s_e.episode_id AND tv_s_e.tv_id = ? AND tv_s_e_name2.language_id = ?;", dbID(), langId(lang) );
  cSql statement_moviedb(m_db, "SELECT tv_s_e_name_moviedb.episode_name, tv_s_e.season_number, tv_s_e.episode_number, tv_s_e.episode_air_date FROM tv_s_e, tv_s_e_name_moviedb WHERE tv_s_e_name_moviedb.episode_id = tv_s_e.episode_id AND tv_s_e.tv_id = ? AND tv_s_e_name_moviedb.language_id = ?;", dbID(), langId(lang) );
  cSql *statement = dbID()<0?&statement_tvdb:&statement_moviedb;
  for (cSql &sqli: *statement) {
    const char *episodeName = NULL;
    const char *episode_air_date = NULL;
    int episode = 0;
    int season = 0;
    sqli.readRow(episodeName, season, episode, episode_air_date);
    if (!episodeName || !*episodeName) continue;
//    don't save normed strings in database.
//      doesn't help for performance (norming one string only takes 5% of sentence_distance time
//      makes changes to the norm algorithm almost impossible
    int distance = tvSearchEpisodeString.sentence_distance(episodeName);
    if (debug && (distance < 600 || (season < 3)) ) esyslog("tvscraper:DEBUG cTv::searchEpisode search string \"%s\" episodeName \"%s\"  season %i episode %i dbid %i, distance %i", tvSearchEpisodeString.m_normedString.c_str(), episodeName, season, episode, dbID(), distance);
    if (season == 0) distance += 20; // avoid season == 0, could be making of, ...
    int f = years.find2(cYears::yearToInt(episode_air_date) );
    if (f == 2) distance = std::max(0, distance-100);
    else if (f == 1) distance = std::max(0, distance-50);
    if (episode_guess > 0) {
      if (season_guess >= 0) {
        if (episode_guess == episode && season_guess == season) distance = std::max(0, distance-150);
      } else {
        if (episode_guess == episode) distance = std::max(0, distance-50);
      }
    }
    if (distance < best_distance) {
      best_distance = distance;
      best_season = season;
      best_episode = episode;
    }
  }
  if (debug) esyslog("tvscraper:DEBUG cTv::searchEpisode search string \"%s\" best_season %i best_episode %i dbid %i, best_distance %i", tvSearchEpisodeString.m_normedString.c_str(), best_season, best_episode, dbID(), best_distance);
  if (best_distance > 650) {  // accept a rather high distance here. We return the distance, so the caller can finally decide to take this episode or not
    m_seasonNumber = 0;
    m_episodeNumber = 0;
    return 1000;
  }
  m_seasonNumber = best_season;
  m_episodeNumber = best_episode;
  return best_distance;
}

bool cTv::getOverview(std::string *title, std::string *episodeName, std::string *releaseDate, int *runtime, std::string *imdbId, int *collectionId, std::string *collectionName) {
// return false if no data are available. In this case, paramters will NOT change
// return runtime only if episode runtime is available. No guess here (from list of episode runtimes)
// we start to collect episode information. Data available from episode will not be requested from TV show
// never available (only for movies): collectionId, collectionName

  bool episodeDataAvailable = false, episodeImdbIdAvailable = false, episodeReleaseDateAvailable = false;
  cUseStmt_select_tv2_overview stmt_s_o(m_db);
  cUseStmt_select_tv_s_e_overview stmt_select_tv_s_e_overview(m_db);
  cUseStmt_select_tv_s_e_name2_tv_s_e stmt_select_tv_s_e_name2_tv_s_e(m_db);
  stmt_s_o.stmt()->resetBindStep(dbID() );
  if ((m_seasonNumber != 0 || m_episodeNumber != 0) &&
    (episodeName != NULL || imdbId != NULL || releaseDate != NULL || runtime != NULL)) {
    cSql *stmt;
    if (stmt_s_o.stmt()->readRow() && !stmt_s_o.stmt()->valueInitial(3) ) {
      int langInt = stmt_s_o.stmt()->getInt(3);
      stmt_select_tv_s_e_name2_tv_s_e.stmt()->resetBindStep(dbID(), m_seasonNumber, m_episodeNumber, langInt);
      stmt = stmt_select_tv_s_e_name2_tv_s_e.stmt();
    } else {
      stmt_select_tv_s_e_overview.stmt()->resetBindStep(dbID(), m_seasonNumber, m_episodeNumber);
      stmt = stmt_select_tv_s_e_overview.stmt();
    }
    if (stmt->readRow(episodeName, releaseDate, runtime, imdbId) ) {
      episodeDataAvailable = true;
      episodeReleaseDateAvailable = releaseDate && !releaseDate->empty();
      episodeImdbIdAvailable = imdbId && !imdbId->empty();
    }
  }

// check: more data requested?
  if (!title && (!imdbId || episodeImdbIdAvailable) && (!releaseDate || episodeReleaseDateAvailable) ) {
    if (!episodeDataAvailable) return false;
    if (collectionId) *collectionId = 0;
    if (collectionName) *collectionName = "";
    return true;
  }
  if (!stmt_s_o.stmt()->readRow() ) {
    if (!episodeDataAvailable) return false;
    if (title) *title = "";
    if (collectionId) *collectionId = 0;
    if (collectionName) *collectionName = "";
    return true;
  }
  if (title) *title = stmt_s_o.stmt()->getStringView(0);
  if (episodeDataAvailable) {
// dont't overide data available from episode
    if (releaseDate && releaseDate->empty() ) *releaseDate = stmt_s_o.stmt()->getStringView(1);
    if (imdbId && imdbId->empty() ) *imdbId = stmt_s_o.stmt()->getStringView(2);
  } else {
// no episode data. Read as much as possible not episode related, and set the others to 0/""
    *releaseDate = stmt_s_o.stmt()->getStringView(1);
    *imdbId = stmt_s_o.stmt()->getStringView(2);
    if (episodeName) *episodeName = "";
    if (runtime) *runtime = 0;
  }
  if (collectionId) *collectionId = 0;
  if (collectionName) *collectionName = "";
  return true;
}

void addGuestStars(std::vector<cActor> &result, const char *str) {
// format of str as defined in thetvdbscraper/tvdbseries.c cTVDBSeries::ParseJson_Character
// |actorName: actoreRoleName|actorName2: actoreRoleName2|...
  if (!str || !*str) return;
  cActor actor;
  actor.actorThumb.path = "";
  actor.actorThumb.width = 0;
  actor.actorThumb.height = 0;
  if (str[0] != '|') { actor.name = str; result.push_back(actor); }
  const char *lDelimPos = str;
  for (const char *rDelimPos = strchr(lDelimPos + 1, '|'); rDelimPos != NULL; rDelimPos = strchr(lDelimPos + 1, '|') ) {
    cSv pers_name = cSv(lDelimPos + 1, rDelimPos - lDelimPos - 1);
    auto del2 = pers_name.find(": ");
    if (del2 == std::string::npos) {
      actor.name = pers_name;
      actor.role = "";
    } else {
      actor.name = pers_name.substr(0, del2);
      actor.role = pers_name.substr(del2 + 2);
    }
    result.push_back(actor);
    lDelimPos = rDelimPos;
  }
}

void cTvTvdb::AddGuestActors(std::vector<cActor> &actors, bool fullPath) {
  const char *guestStars = NULL;
  cSql statement(m_db, "select episode_guest_stars from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?");
  statement.resetBindStep(dbID(), m_seasonNumber, m_episodeNumber);
  if (statement.readRow(guestStars) ) addGuestStars(actors, guestStars);
}

// implemntation of cTvMoviedb  *********************
void cTvMoviedb::DeleteMediaAndDb() {
  m_db->DeleteSeries(m_id);
}

void cTvMoviedb::AddGuestActors(std::vector<cActor> &actors, bool fullPath) {
// actors from guest stars
  const char *sql_guests = "select actors.actor_id, actor_name, actor_role, actor_has_image from actors, actor_tv_episode " \
                            "where actor_tv_episode.actor_id = actors.actor_id and actor_tv_episode.episode_id = ?";
  if (m_seasonNumber != 0 || m_episodeNumber != 0) AddActors(actors, sql_guests,  m_episodeNumber, "movies/actors/actor_", fullPath);
}

std::vector<cActor> cTvMoviedb::GetActors(bool fullPath) {
  std::vector<cActor> actors;
  const char *sql = "select actors.actor_id, actor_name, actor_role, actor_has_image from actors, actor_tv " \
                     "where actor_tv.actor_id = actors.actor_id and actor_tv.tv_id = ?";
  AddActors(actors, sql,  dbID(), "movies/actors/actor_", fullPath);
// actors from guest stars
//  AddGuestActors(actors, false);  (exclude guest stars, so we can list them separately)
  return actors;
}

//images ***************************
bool cTvMoviedb::getSingleImageAnySeason(eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
// Any season image (independent of current season, or if a season was found)
// Orientation: 
// Banner    (will return false, there is no season banner)
// Landscape (will return false, there is no season backdrop / fanart) 
// Portrait  (will return the season poster)
  if (orientation != eOrientation::portrait) return false;
  CONCATENATE(dir_path, config.GetBaseDirMovieTv(), m_id);
  const std::filesystem::path fs_path(dir_path);
  std::error_code ec;
  if (std::filesystem::exists(fs_path) ) {
    for (auto const& dir_entry : std::filesystem::directory_iterator(fs_path, ec)) {
      if (dir_entry.is_directory()) {
        if (checkPathC(relPath, fullPath, width, height, 780, 1108, dir_entry.path(), "/poster.jpg")) return true;
      }
    }
  } // else esyslog("tvscraper:cTvMoviedb::getSingleImageAnySeason ERROR dir %s does not exist", dir_path);

  return false;
}

bool cTvMoviedb::getSingleImageTvShow(eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
// Poster or banner or fanart. Does not depend on episode or season!
// Orientation: 
// Banner    (will return false, there is no banner in themoviedb)
// Landscape (will return backdrop) 
// Portrait  (will return poster)
  if (orientation == eOrientation::banner) return false;
  switch (orientation) {
    case eOrientation::portrait:
      return checkPathC(relPath, fullPath, width, height, 780, 1108, config.GetBaseDirMovieTv(), m_id, "/poster.jpg");
    case eOrientation::landscape:
      return checkPathC(relPath, fullPath, width, height, 1280, 720, config.GetBaseDirMovieTv(), m_id, "/backdrop.jpg");
    default: return false;
  }
}
bool cTvMoviedb::getSingleImageSeason(eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
// Season image
// Orientation: 
// Banner    (will return false, there is no season banner)
// Landscape (will return false, there is no season backdrop / fanart) 
// Portrait  (will return the season poster)
  if (orientation != eOrientation::portrait) return false;
  if (m_seasonNumber == 0 && m_episodeNumber == 0) return false;  // no episode found
  CONCATENATE(path, config.GetBaseDirMovieTv(), m_id, "/", m_seasonNumber, "/poster.jpg");
  return checkPath(path, relPath, fullPath, width, height, 780, 1108);
}

bool cTvMoviedb::getSingleImageEpisode(eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
// TV show: still image
// Orientation: 
// Banner    (will return false, there is no banner for episode still)
// Landscape (will return the still)
// Portrait  (will return false, there is no portrait for episode still)
  if (orientation != eOrientation::landscape) return false;
  if (m_seasonNumber == 0 && m_episodeNumber == 0) return false;  // no episode found
  CONCATENATE(path, config.GetBaseDirMovieTv(), m_id, "/", m_seasonNumber, "/still_", m_episodeNumber, ".jpg");
  return checkPath(path, relPath, fullPath, width, height, 300, 200);
}

// implemntation of cTvTvdb  *********************
void cTvTvdb::DeleteMediaAndDb() {
  m_db->DeleteSeries(-m_id);
}

std::vector<cActor> cTvTvdb::GetActors(bool fullPath) {
  std::vector<cActor> actors;
  cActor actor;
  const char *actorId = NULL;
  cSql sql(m_db, "SELECT actor_number, actor_name, actor_role FROM series_actors WHERE actor_series_id = ?");
  for (cSql &stmt: sql.resetBindStep(m_id) ) {
    stmt.readRow(actorId, actor.name, actor.role);
    actor.actorThumb.width = 300;
    actor.actorThumb.height = 450;
    actor.actorThumb.path = "";
    if (actorId && actorId[0] != '-') {
      CONCATENATE(path, config.GetBaseDirSeries(), m_id, "/actor_", actorId, ".jpg");
      if (FileExistsImg(path)) {
        if (fullPath) actor.actorThumb.path = path;
        else actor.actorThumb.path = path + config.GetBaseDirLen();
      }
    }
    actors.push_back(actor);
  }
  return actors;
}

//images ***************************
vector<cTvMedia> cTvTvdb::getImages(eOrientation orientation, int maxImages, bool fullPath) {
  vector<cTvMedia> images;
  if (orientation != eOrientation::portrait && orientation != eOrientation::landscape) return images;
  CONCATENATE(path0, config.GetBaseDirSeries(), m_id, (orientation == eOrientation::portrait)?"/poster_":"/fanart_");
  for (int i=0; i<maxImages; i++) {
    CONCATENATE(path, path0, i, ".jpg");
    cTvMedia media;
    if (fullPath) {
      if (checkPath(path, NULL, &media.path, &media.width, &media.height, (orientation == eOrientation::portrait)?680:1920, (orientation == eOrientation::portrait)?1000:1080)) images.push_back(media);
    } else {
      if (checkPath(path, &media.path, NULL, &media.width, &media.height, (orientation == eOrientation::portrait)?680:1920, (orientation == eOrientation::portrait)?1000:1080)) images.push_back(media);
    }
  }
  return images;
}

bool cTvTvdb::getSingleImageAnySeason(eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
// For TV show: any season image (independent of current season, or if a season was found)
// Orientation: 
// 3: Banner    (will return false, there is no season banner)
// 2: Landscape (will return false, there is no season backdrop / fanart) 
// 1: Portrait  (will return the season poster)
  if (orientation != eOrientation::portrait) return false;
//  path << config.GetBaseDirSeries() << m_id << "/season_poster_" << m_seasonNumber << ".jpg";
  CONCATENATE(dir_path, config.GetBaseDirSeries(), m_id);

  const std::filesystem::path fs_path(dir_path);
  if (std::filesystem::exists(fs_path) ) {
    std::error_code ec;
    for (auto const& dir_entry : std::filesystem::directory_iterator(fs_path, ec)) {
      if (dir_entry.path().filename().string().find("season_poster_") != std::string::npos) {
        if (checkPath(dir_entry.path().c_str(), relPath, fullPath, width, height, 680, 1000)) return true;
      }
    }
  } else esyslog("tvscraper:cTvTvdb::getSingleImageAnySeason ERROR dir %s does not exist", dir_path);
  return false;
}

bool getSingleImageTvShow(string *relPath, string *fullPath, int *width, int *height, eOrientation orientation, unsigned int id) {
  switch (orientation) {
    case eOrientation::portrait:
      return checkPathC(relPath, fullPath, width, height, 680, 1000, config.GetBaseDirSeries(), id, "/poster_0.jpg");
    case eOrientation::landscape:
      return checkPathC(relPath, fullPath, width, height, 1920, 1080, config.GetBaseDirSeries(), id, "/fanart_0.jpg");
    case eOrientation::banner:
      return checkPathC(relPath, fullPath, width, height, 758, 140, config.GetBaseDirSeries(), id, "/banner.jpg");
    default: return false;
  }
}

bool cTvTvdb::getSingleImageTvShow(eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
// Does not depend on episode or season!
  return ::getSingleImageTvShow(relPath, fullPath, width, height, orientation, m_id);
}

bool cTvTvdb::getSingleImageSeason(eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
// TV show: season image
// Orientation: 
// Banner    (will return false, there is no season banner)
// Landscape (will return false, there is no season backdrop / fanart) 
// Portrait  (will return the season poster)
  if (orientation != eOrientation::portrait) return false;
  if (m_seasonNumber == 0 && m_episodeNumber == 0) return false;  // no episode found
  CONCATENATE(path, config.GetBaseDirSeries(), m_id, "/season_poster_", m_seasonNumber, ".jpg");
  return checkPath(path, relPath, fullPath, width, height, 680, 1000);
}

bool cTvTvdb::getSingleImageEpisode(eOrientation orientation, string *relPath, string *fullPath, int *width, int *height) {
// Still image
// Orientation: 
// Banner    (will return false, there is no banner for episode still)
// Landscape (will return the still)
// Portrait  (will return false, there is no portrait for episode still)
  if (orientation != eOrientation::landscape) return false;
  if (m_seasonNumber == 0 && m_episodeNumber == 0) return false;  // no episode found
  CONCATENATE(path, config.GetBaseDirSeries(), m_id, "/", m_seasonNumber, "/still_", m_episodeNumber, ".jpg");
  return checkPath(path, relPath, fullPath, width, height, 300, 200);
}

// static methods  *********************
// create object ***
cMovieOrTv *cMovieOrTv::getMovieOrTv(const cTVScraperDB *db, int id, ecMovieOrTvType type) {
  switch (type) {
    case ecMovieOrTvType::theMoviedbMovie:  return new cMovieMoviedb(db, id);
    case ecMovieOrTvType::theMoviedbSeries: return new cTvMoviedb(db, id);
    case ecMovieOrTvType::theTvdbSeries:    return new cTvTvdb(db, id);
  }
  return NULL;
}

cMovieOrTv *cMovieOrTv::getMovieOrTv(const cTVScraperDB *db, const sMovieOrTv &movieOrTv) {
  if (movieOrTv.id == 0) return NULL;
  switch (movieOrTv.type) {
    case scrapMovie: return new cMovieMoviedb(db, movieOrTv.id);
    case scrapSeries:
      if (movieOrTv.id > 0) return new cTvMoviedb(db, movieOrTv.id);
            else            return new cTvTvdb(db, movieOrTv.id * -1);
    case scrapNone: return NULL;
  }
  return NULL;
}

cMovieOrTv *cMovieOrTv::getMovieOrTv(const cTVScraperDB *db, const cEvent *event, const cRecording *recording, int *runtime, int *duration_deviation) {
  if (event && recording) {
    esyslog("tvscraper: ERROR cMovieOrTv::getMovieOrTv, event && recording are provided. Please set one of these parameters to NULL");
    return nullptr;
  }
  if (event) return getMovieOrTv(db, event, runtime);
  else return getMovieOrTv(db, recording, runtime, duration_deviation);
}

cMovieOrTv *cMovieOrTv::getMovieOrTv(const cTVScraperDB *db, const cEvent *event, int *runtime) {
  if (!event) return NULL;
  int movie_tv_id, season_number, episode_number;
  if(!db->GetMovieTvID(event, movie_tv_id, season_number, episode_number, runtime)) return NULL;

  if(season_number == -100) return new cMovieMoviedb(db, movie_tv_id);
  if ( movie_tv_id > 0) return new cTvMoviedb(db, movie_tv_id, season_number, episode_number);
        else            return new cTvTvdb(db,   -movie_tv_id, season_number, episode_number);
}

cMovieOrTv *cMovieOrTv::getMovieOrTv(const cTVScraperDB *db, const cRecording *recording, int *runtime, int *duration_deviation) {
  if (!recording) return NULL;
  int movie_tv_id, season_number, episode_number;
  if(!db->GetMovieTvID(recording, movie_tv_id, season_number, episode_number, runtime, duration_deviation)) return NULL;

  if(season_number == -100) return new cMovieMoviedb(db, movie_tv_id);
  if ( movie_tv_id > 0) return new cTvMoviedb(db, movie_tv_id, season_number, episode_number);
        else            return new cTvTvdb(db,   -movie_tv_id, season_number, episode_number);
}

// search episode
int cMovieOrTv::searchEpisode(const cTVScraperDB *db, sMovieOrTv &movieOrTv, iExtMovieTvDb *extMovieTvDb, cSv tvSearchEpisodeString, cSv baseNameOrTitle, const cYears &years, const cLanguage *lang, const char *shortText, const char *description) {
// return 1000 if no episode was found
// otherwise, return distance 0-999 (smaller numbers are better matches)
  bool debug = false;
  debug = movieOrTv.id == -294265;
  if (debug) esyslog("tvscraper:DEBUG cTvMoviedb::searchEpisode search string \"%.*s\"", static_cast<int>(tvSearchEpisodeString.length()), tvSearchEpisodeString.data());
  movieOrTv.season  = 0;
  movieOrTv.episode = 0;
  if (!extMovieTvDb) return 1000;
  int rc = extMovieTvDb->downloadEpisodes(std::abs(movieOrTv.id), lang);
  if (rc != 0 && rc != 1) return 1000; // allow 1 (no episode names in this language), there might be other indicators like (episodeNumber)
  cMovieOrTv *mv =  cMovieOrTv::getMovieOrTv(db, movieOrTv);
  if (!mv) return 1000;
  int distance = mv->searchEpisode(tvSearchEpisodeString, baseNameOrTitle, years, lang, shortText, description);
  
  if (debug) esyslog("tvscraper:DEBUG cTvMoviedb::searchEpisode search string \"%.*s\" season %i episode %i", static_cast<int>(tvSearchEpisodeString.length()), tvSearchEpisodeString.data(), mv->m_seasonNumber, mv->m_episodeNumber);
  if (distance != 1000) {
    movieOrTv.season  = mv->m_seasonNumber;
    movieOrTv.episode = mv->m_episodeNumber;
  }
  delete (mv);
  return distance;
}

// delete unused *****
void cMovieOrTv::CleanupTv_media(const cTVScraperDB *db) {
  const char *sql = "delete from tv_media where media_type != ?";
  db->exec(sql, mediaSeason);
  std::set<int> tv_ids;
  for (cSql &statement: cSql(db, "select tv_id from tv_media") ) tv_ids.insert(statement.getInt(0) );
  for (const int &tv_id2: tv_ids) {
    if (db->CheckMovieOutdatedEvents(tv_id2, 0, 0)) continue;
    if (db->CheckMovieOutdatedRecordings(tv_id2, 0, 0)) continue;
    db->deleteTvMedia (tv_id2, false, false);
  }
}

void deleteOutdatedRecordingImages(const cTVScraperDB *db) {
// check recording. Delete if this does not exist
if (!DirExists(config.GetBaseDirRecordings().c_str() )) return;
std::error_code ec;
for (const std::filesystem::directory_entry& dir_entry:
        std::filesystem::directory_iterator(config.GetBaseDirRecordings(), ec)) 
  {
    std::vector<std::string> parts = getSetFromString<std::string,std::vector<std::string>>(dir_entry.path().filename().string().c_str(), '_');
    if (parts.size() != 3) {
      esyslog("tvscraper, ERROR, deleteOutdatedRecordingImages, parts.size: %zu, filename: %s", parts.size(), dir_entry.path().filename().string().c_str());
      continue;
    }
    time_t eventStartTime = parse_unsigned<time_t>(parts[0]);
    tEventID eventID = parse_unsigned<tEventID>(parts[2]);
    if (eventStartTime == 0 || eventID == 0) {
      esyslog("tvscraper, ERROR, deleteOutdatedRecordingImages, parts[2]: %.*s, filename: %s", static_cast<int>(parts[2].length()), parts[2].data(), dir_entry.path().filename().string().c_str());
      continue;
    }
    const char *sql = "SELECT count(event_id) FROM recordings2 WHERE event_id = ? AND event_start_time = ? AND channel_id = ?";
    bool found = db->queryInt(sql, eventID, eventStartTime, parts[1]) > 0;
//    esyslog("tvscraper, DEBUG, recording eventID %i eventStartTime %lld channel_id %.*s %s", (int)eventID, (long long)eventStartTime, static_cast<int>(parts[1].length()), parts[1].data(), found?"found":"not found");
    if (!found) DeleteFile(config.GetBaseDirRecordings() + dir_entry.path().filename().string());
  }
}

void deleteOutdatedEpgImages() {
// check event start time. Delete if older than yesterday
if (!DirExists(config.GetBaseDirEpg().c_str() )) return;
std::error_code ec;
for (const std::filesystem::directory_entry& dir_entry:
        std::filesystem::directory_iterator(config.GetBaseDirEpg(), ec) )
  {
    if (!dir_entry.is_directory() ) continue;
    if (dir_entry.path().filename().string().find_first_not_of("0123456789") != std::string::npos) continue;
    if (atoll(dir_entry.path().filename().string().c_str() ) + 24*60*60 < time(0) ) DeleteAll(config.GetBaseDirEpg() + dir_entry.path().filename().string());
  }
}
void cMovieOrTv::DeleteAllIfUnused(const cTVScraperDB *db) {
// check all movies in db
  CleanupTv_media(db);
  for (cSql &stmt: cSql(db, "select movie_id from movies3;") ) {
    cMovieMoviedb movieMoviedb(db, stmt.getInt(0) );
    movieMoviedb.DeleteIfUnused();
  }

// check all tv shows in db
  for (cSql &stmt: cSql(db, "select tv_id from tv2;")) {
    int tvID = stmt.getInt(0);
    if (tvID > 0) { cTvMoviedb tvMoviedb(db, tvID); tvMoviedb.DeleteIfUnused(); }
       else        { cTvTvdb tvTvdb(db, tvID * -1); tvTvdb.DeleteIfUnused(); }
  }

  DeleteAllIfUnused(config.GetBaseDirMovieTv(), ecMovieOrTvType::theMoviedbSeries, db);
  DeleteAllIfUnused(config.GetBaseDirSeries(), ecMovieOrTvType::theTvdbSeries, db);
  cMovieMoviedb::DeleteAllIfUnused(db);
// delete all outdated events
  db->ClearOutdated();
// delete outdated images from external EPG
  deleteOutdatedEpgImages();
  deleteOutdatedRecordingImages(db);
}

void cMovieOrTv::DeleteAllIfUnused(const string &folder, ecMovieOrTvType type, const cTVScraperDB *db) {
// check for all subfolders in folder. If a subfolder has only digits, delete the movie/tv with this number
std::error_code ec;
for (const std::filesystem::directory_entry& dir_entry:
        std::filesystem::directory_iterator(folder, ec))
  {
    if (! dir_entry.is_directory() ) continue;
    if (dir_entry.path().filename().string().find_first_not_of("0123456789") != std::string::npos) continue;
    cMovieOrTv *movieOrTv = getMovieOrTv(db, atoi(dir_entry.path().filename().string().c_str() ), type);
    if(movieOrTv) {
      movieOrTv->DeleteIfUnused();
      delete(movieOrTv);
    }
  }
}

void cMovieMoviedb::DeleteAllIfUnused(const cTVScraperDB *db) {
// check for all files in folder. If a file has the pattern for movie backdrop or poster, check the movie with this number
std::error_code ec;
for (const std::filesystem::directory_entry& dir_entry : 
        std::filesystem::directory_iterator(config.GetBaseDirMovies(), ec) )
  {
    if (! dir_entry.is_regular_file() ) continue;
    size_t pos = dir_entry.path().filename().string().find_first_not_of("0123456789");
    if (dir_entry.path().filename().string()[pos] != '_') continue;
    cMovieMoviedb movieMoviedb(db, atoi(dir_entry.path().filename().string().c_str() ));
    movieMoviedb.DeleteIfUnused();
  }
}


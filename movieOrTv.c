#include "movieOrTv.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <charconv>

// implemntation of cMovieOrTv  *********************


void cMovieOrTv::clearScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv) {
  cTvMedia tvMedia;
  tvMedia.path = "";
  tvMedia.width = 0;
  tvMedia.height = 0;
  scraperMovieOrTv->found = false;
  scraperMovieOrTv->movie = false;
  scraperMovieOrTv->title = "";
  scraperMovieOrTv->originalTitle = "";
  scraperMovieOrTv->tagline = "";
  scraperMovieOrTv->overview = "";
  scraperMovieOrTv->genres.clear();
  scraperMovieOrTv->homepage = "";
  scraperMovieOrTv->releaseDate = "";
  scraperMovieOrTv->adult = false;
  scraperMovieOrTv->runtimes.clear();
  scraperMovieOrTv->popularity = 0.;
  scraperMovieOrTv->voteAverage = 0.;
  scraperMovieOrTv->voteCount = 0;
  scraperMovieOrTv->productionCountries.clear();
  scraperMovieOrTv->actors.clear();
  scraperMovieOrTv->IMDB_ID = "";
  scraperMovieOrTv->posterUrl = "";
  scraperMovieOrTv->fanartUrl = "";
  scraperMovieOrTv->posters.clear();
  scraperMovieOrTv->banners.clear();
  scraperMovieOrTv->fanarts.clear();
  scraperMovieOrTv->budget = 0;
  scraperMovieOrTv->revenue = 0;
  scraperMovieOrTv->collectionId = 0;
  scraperMovieOrTv->collectionName = "";
  scraperMovieOrTv->collectionPoster = tvMedia;
  scraperMovieOrTv->collectionFanart = tvMedia;
  scraperMovieOrTv->director.clear();
  scraperMovieOrTv->writer.clear();
  scraperMovieOrTv->status = "";
  scraperMovieOrTv->networks.clear();
  scraperMovieOrTv->createdBy.clear();
  scraperMovieOrTv->episodeFound = false;
  scraperMovieOrTv->seasonPoster = tvMedia;
  scraperMovieOrTv->episode.number = 0;
  scraperMovieOrTv->episode.season = 0;
  scraperMovieOrTv->episode.absoluteNumber = 0;
  scraperMovieOrTv->episode.name = "";
  scraperMovieOrTv->episode.firstAired = "";
  scraperMovieOrTv->episode.guestStars.clear();
  scraperMovieOrTv->episode.overview = "";
  scraperMovieOrTv->episode.vote_average = 0.;
  scraperMovieOrTv->episode.vote_count = 0;
  scraperMovieOrTv->episode.episodeImage = tvMedia;
  scraperMovieOrTv->episode.episodeImageUrl = "";
  scraperMovieOrTv->episode.director.clear();
  scraperMovieOrTv->episode.writer.clear();
  scraperMovieOrTv->episode.IMDB_ID = "";
}

void cMovieOrTv::AddActors(std::vector<cActor> &actors, const char *sql, int id, const char *pathPart, bool fullPath, int width, int height) {
// adds the actors found with sql&id to the list of actors
// works for all actors found in themoviedb (movie & tv actors). Not for actors in thetvdb
  cActor actor;
  actor.actorThumb.width = width;
  actor.actorThumb.height = height;
  const char *actorId;
  int hasImage;
  for (sqlite3_stmt *statement = m_db->QueryPrepare(sql, "i", id);
       m_db->QueryStep(statement, "sSSi", &actorId, &actor.name, &actor.role, &hasImage); ) {
    if (hasImage) {
      CONCAT(path, "ssss", config.GetBaseDir().c_str(), pathPart, actorId, ".jpg");
      if (FileExists(path)) {
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

bool cMovieOrTv::copyImagesToRecordingFolder(const std::string &recordingFileName) {
// return true if a fanart was copied
  if (recordingFileName.empty() ) return true;
  string path;
  cImageLevelsInt level(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection);
  if (getSingleImageBestL(level, eOrientation::portrait, NULL, &path) != eImageLevel::none)
    CopyFile(path, recordingFileName + "/poster.jpg" );
  if (getSingleImageBestL(level, eOrientation::landscape, NULL, &path) != eImageLevel::none) {
    CopyFile(path, recordingFileName + "/fanart.jpg" );
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

void cMovieMoviedb::DownloadImages(cMovieDBScraper *moviedbScraper, cTVDBScraper *tvdbScraper, const std::string &recordingFileName) {
  moviedbScraper->DownloadMedia(m_id);
  moviedbScraper->DownloadActors(m_id, true);
  copyImagesToRecordingFolder(recordingFileName);
}

void cTvMoviedb::DownloadImages(cMovieDBScraper *moviedbScraper, cTVDBScraper *tvdbScraper, const std::string &recordingFileName) {
  moviedbScraper->DownloadMediaTv(m_id);
  moviedbScraper->DownloadActors(m_id, false);
  string episodeStillPath = m_db->GetEpisodeStillPath(m_id, m_seasonNumber, m_episodeNumber);
  if (!episodeStillPath.empty() )
    moviedbScraper->StoreStill(m_id, m_seasonNumber, m_episodeNumber, episodeStillPath);
  copyImagesToRecordingFolder(recordingFileName);
}

void cTvTvdb::DownloadImages(cMovieDBScraper *moviedbScraper, cTVDBScraper *tvdbScraper, const std::string &recordingFileName) {
  tvdbScraper->StoreActors(m_id);
  tvdbScraper->DownloadMedia(m_id);
  string episodeStillPath = m_db->GetEpisodeStillPath(dbID(), m_seasonNumber, m_episodeNumber);
  if (!episodeStillPath.empty() )
    tvdbScraper->StoreStill(m_id, m_seasonNumber, m_episodeNumber, episodeStillPath);
  copyImagesToRecordingFolder(recordingFileName);
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
  stringstream base;
  base << config.GetBaseDirMovies() << m_id;
  DeleteFile(base.str() + "_backdrop.jpg");
  DeleteFile(base.str() + "_poster.jpg");
  m_db->DeleteMovie(m_id);
} 

std::string cMovieMoviedb::getCollectionName() {
  const char *sql = "select movie_collection_name from movies3 where movie_id = ?";
  return m_db->QueryString(sql, "i", dbID() );
}

void cMovieMoviedb::getScraperOverview(cGetScraperOverview *scraperOverview) {
  scraperOverview->m_videoType = eVideoType::movie;
  scraperOverview->m_dbid = dbID();
  const char *title = NULL;
  const char *collectionName = NULL;
  const char *IMDB_ID = NULL;
  const char *releaseDate = NULL;
  const char sql[] = "select movie_title, movie_collection_id, movie_collection_name, " \
    "movie_release_date, movie_runtime, movie_IMDB_ID from movies3 where movie_id = ?";
  sqlite3_stmt *statement = m_db->QueryPrepare(sql, "i", dbID() );
  if (!m_db->QueryStep(statement, "sissis",
      &title, &scraperOverview->m_collectionId, &collectionName,
      &releaseDate, &scraperOverview->m_runtime, &IMDB_ID)) {
    scraperOverview->m_videoType = eVideoType::none;
    return;
  }
  if (scraperOverview->m_title && title)   *scraperOverview->m_title = title;
  if (scraperOverview->m_IMDB_ID && IMDB_ID) *scraperOverview->m_IMDB_ID = IMDB_ID;
  if (scraperOverview->m_collectionName && collectionName) *scraperOverview->m_collectionName = collectionName;
  if (scraperOverview->m_releaseDate && releaseDate) *scraperOverview->m_releaseDate = releaseDate;

  sqlite3_finalize(statement);
  if (scraperOverview->m_image) {
// get image (landscape, if possible)
    m_collectionId = scraperOverview->m_collectionId > 0?scraperOverview->m_collectionId:0;
    getSingleImageBestLO(scraperOverview->m_imageLevels, scraperOverview->m_imageOrientations, &(scraperOverview->m_image->path), NULL, &(scraperOverview->m_image->width), &(scraperOverview->m_image->height) );
  }
}

bool cMovieMoviedb::getOverview(std::string *title, std::string *episodeName, std::string *releaseDate, int *runtime, std::string *imdbId, int *collectionId, std::string *collectionName) {
// return false if no data are available. In this case, paramters will NOT change
  m_collectionId = 0;
  if (collectionId) *collectionId = 0;
  if (runtime) *runtime = 0;
  if (episodeName) *episodeName = "";
  const char *title_ = NULL;
  const char *collectionName_ = NULL;
  const char *IMDB_ID_ = NULL;
  const char *releaseDate_ = NULL;
  int runtime_;
  const char sql[] = "select movie_title, movie_collection_id, movie_collection_name, " \
    "movie_release_date, movie_runtime, movie_IMDB_ID from movies3 where movie_id = ?";
  sqlite3_stmt *statement = m_db->QueryPrepare(sql, "i", dbID() );
  if (!m_db->QueryStep(statement, "sissis",
      &title_, &m_collectionId, &collectionName_, &releaseDate_, &runtime_, &IMDB_ID_)) {
    return false;
  }
  if (title)       *title = charPointerToString(title_);
  if (imdbId)      *imdbId = charPointerToString(IMDB_ID_);
  if (releaseDate) *releaseDate = charPointerToString(releaseDate_);
  if (collectionName) *collectionName = charPointerToString(collectionName_);
  sqlite3_finalize(statement);
  if (collectionId) *collectionId = m_collectionId;
  if (runtime)      *runtime = runtime_;
  return true;
}

void cMovieMoviedb::getScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv) {
  scraperMovieOrTv->movie = true;
  const char *genres;
  const char *productionCountries;
  const char *posterUrl;
  const char *fanartUrl;
  int runtime;
  const char sql[] = "select movie_title, movie_original_title, movie_tagline, movie_overview, " \
    "movie_adult, movie_collection_id, movie_collection_name, " \
    "movie_budget, movie_revenue, movie_genres, movie_homepage, " \
    "movie_release_date, movie_runtime, movie_popularity, movie_vote_average, movie_vote_count, " \
    "movie_production_countries, movie_posterUrl, movie_fanartUrl, movie_IMDB_ID from movies3 where movie_id = ?";
  sqlite3_stmt *statement = m_db->QueryPrepare(sql, "i", dbID() );
  scraperMovieOrTv->found = m_db->QueryStep(statement, "SSSSbiSiisSSiffisssS",
      &scraperMovieOrTv->title, &scraperMovieOrTv->originalTitle, &scraperMovieOrTv->tagline, &scraperMovieOrTv->overview,
      &scraperMovieOrTv->adult, &scraperMovieOrTv->collectionId, &scraperMovieOrTv->collectionName,
      &scraperMovieOrTv->budget, &scraperMovieOrTv->revenue, &genres, &scraperMovieOrTv->homepage, 
      &scraperMovieOrTv->releaseDate, &runtime,
      &scraperMovieOrTv->popularity, &scraperMovieOrTv->voteAverage, &scraperMovieOrTv->voteCount,
      &productionCountries, &posterUrl, &fanartUrl, &scraperMovieOrTv->IMDB_ID);
  if (!scraperMovieOrTv->found) return;
  if (runtime) scraperMovieOrTv->runtimes.push_back(runtime);
  stringToVector(scraperMovieOrTv->genres, genres);
  stringToVector(scraperMovieOrTv->productionCountries, productionCountries);

  if (scraperMovieOrTv->httpImagePaths) {
    if (posterUrl && *posterUrl) scraperMovieOrTv->posterUrl = imageUrl(posterUrl);
    if (fanartUrl && *fanartUrl) scraperMovieOrTv->fanartUrl = imageUrl(fanartUrl);
  }
  sqlite3_finalize(statement);

  scraperMovieOrTv->actors = GetActors();
  if (scraperMovieOrTv->media) {
    scraperMovieOrTv->posters = getImages(eOrientation::portrait);
    cTvMedia fanart;
    if (eImageLevel::none != getSingleImageBestL(
      cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection),
      eOrientation::landscape, NULL, &fanart.path, &fanart.width, &fanart.height))
        scraperMovieOrTv->fanarts.push_back(fanart);
    if (scraperMovieOrTv->collectionId) {
      m_collectionId = scraperMovieOrTv->collectionId;
      getSingleImage(eImageLevel::tvShowCollection, eOrientation::portrait,  NULL, &scraperMovieOrTv->collectionPoster.path, &scraperMovieOrTv->collectionPoster.width, &scraperMovieOrTv->collectionPoster.height);
      getSingleImage(eImageLevel::tvShowCollection, eOrientation::landscape, NULL, &scraperMovieOrTv->collectionFanart.path, &scraperMovieOrTv->collectionFanart.width, &scraperMovieOrTv->collectionFanart.height);
    }
  }
// director, writer
  char *director;
  char *writer;
  statement = m_db->QueryPrepare("select movie_director, movie_writer from movie_runtime2 where movie_id = ?", "i", m_id);
  if (m_db->QueryStep(statement, "ss", &director, &writer) ) {
    stringToVector(scraperMovieOrTv->director, director);
    stringToVector(scraperMovieOrTv->writer, writer);
    sqlite3_finalize(statement);
  }
}

std::vector<cActor> cMovieMoviedb::GetActors(bool fullPath) {
  std::vector<cActor> actors;
  const char sql[] = "select actors.actor_id, actor_name, actor_role, actor_has_image from actors, actor_movie " \
                     "where actor_movie.actor_id = actors.actor_id and actor_movie.movie_id = ?";
  AddActors(actors, sql,  dbID(), "movies/actors/actor_", fullPath);
  return actors;
}

bool checkRelPath(string &relPath, int *width, int *height, int i_width, int i_height) {
  if (!FileExistsRelPath(relPath.c_str() ) ) { relPath = ""; return false; }
  if (width) *width = i_width;
  if (height) *height = i_height;
  return true;
}

bool checkPath(const char *path, string *relPath, string *fullPath, int *width, int *height, int i_width, int i_height) {
  if (!FileExists(path) ) return false;
  if (fullPath) *fullPath = path;
  if (relPath) *relPath = path + config.GetBaseDirLen();
  if (width) *width = i_width;
  if (height) *height = i_height;
  return true;
}
bool checkPathC(string *relPath, string *fullPath, int *width, int *height, int i_width, int i_height, const char *fmt, ...) {
  if (!fmt) return false;
  va_list vl;
  va_start(vl, fmt);
  char buffer[concat::vsprint(NULL, fmt, vl) + 1];
  va_end(vl);
  va_start(vl, fmt);
  concat::vsprint(buffer, fmt, vl);
  va_end(vl);
  return checkPath(buffer, relPath, fullPath, width, height, i_width, i_height);
}

bool getSingleImageMovie(string *relPath, string *fullPath, int *width, int *height, eOrientation orientation, const char *basePath, unsigned int id) {
  switch (orientation) {
    case eOrientation::portrait:
      return checkPathC(relPath, fullPath, width, height, 500, 750, "sus", basePath, id, "_poster.jpg");
    case eOrientation::landscape:
      return checkPathC(relPath, fullPath, width, height, 1280,  720, "sus", basePath, id, "_backdrop.jpg");
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
  for (const int &id:m_db->getSimilarTvShows(dbID()) ) {
    if (m_db->CheckMovieOutdatedEvents(id, m_seasonNumber, m_episodeNumber) ) return true;
    if (m_db->CheckMovieOutdatedRecordings(id, m_seasonNumber, m_episodeNumber)) return true;
  }
  return false;
}

int cTv::searchEpisode(string_view tvSearchEpisodeString, string_view baseNameOrTitle, const vector<int> &years, const cLanguage *lang) {
// return 1000, if no match was found
// otherwise, distance
  int distance = searchEpisode(tvSearchEpisodeString, years, lang);
  if (distance != 1000) return distance;
// no match with episode name found, try episode number as part of title
  const char *sql    = "select season_number, episode_number from tv_s_e where tv_id = ? and episode_number = ?";
  const char *sql_a  = "select season_number, episode_number from tv_s_e where tv_id = ? and episode_absolute_number = ?";
  const char *sql_y  = "select season_number, episode_number from tv_s_e where tv_id = ? and episode_number = ? and episode_air_date like ?";
  const char *sql_ya = "select season_number, episode_number from tv_s_e where tv_id = ? and episode_absolute_number = ? and episode_air_date like ?";
  int episodeNumber = NumberInLastPartWithPS(baseNameOrTitle);
  if (episodeNumber != 0) {
    for (const int &year: years) {
      int year_i = year;
      char year_s[] = "    #";
      for (int i = 3; i >= 0; i--) {
        year_s[i] = year_i % 10;
        year_i /= 10;
      }
      if (m_db->QueryLine(sql_y , "iis", "ii", dbID(), episodeNumber, year_s, &m_seasonNumber, &m_episodeNumber)) return 700;
      if (m_db->QueryLine(sql_ya, "iis", "ii", dbID(), episodeNumber, year_s, &m_seasonNumber, &m_episodeNumber)) return 700;
    }
// no match with year found, try without year
// note: this is a very week indicator that the right TV show was choosen. So, even if there is a match, return a high distance (950)
    if (m_db->QueryLine(sql  , "ii", "ii", dbID(), episodeNumber, &m_seasonNumber, &m_episodeNumber)) return 950;
    if (m_db->QueryLine(sql_a, "ii", "ii", dbID(), episodeNumber, &m_seasonNumber, &m_episodeNumber)) return 950;
  }
  return 1000;
}

int cTv::searchEpisode(string_view tvSearchEpisodeString_i, const vector<int> &years, const cLanguage *lang) {
// return 1000, if no match was found
// otherwise, distance
  bool debug = dbID() == 197649 || dbID() == 197648 || tvSearchEpisodeString_i.length() > 200;
  debug = debug || (tvSearchEpisodeString_i.length() > 0 && tvSearchEpisodeString_i[0] == 0);
  debug = debug || (tvSearchEpisodeString_i.length() > 1 && tvSearchEpisodeString_i[1] == 0);
  debug = false;
  if (debug) esyslog("tvscraper:DEBUG cTv::searchEpisode search string_i length %zu, \"%.*s\", dbid %i", tvSearchEpisodeString_i.length(), std::min(100, static_cast<int>(tvSearchEpisodeString_i.length())), tvSearchEpisodeString_i.data(), dbID());
  std::string tvSearchEpisodeString = normString(tvSearchEpisodeString_i);
  if (debug) esyslog("tvscraper:DEBUG cTv::searchEpisode search string \"%s\", dbid %i", tvSearchEpisodeString.c_str(), dbID());
  int best_distance = 1000;
  int best_season = 0;
  int best_episode = 0;
  char *episodeName = NULL;
  int distance;
  int episode;
  int season;
  bool isDefaultLang = config.isDefaultLanguage(lang);
  const char sqld[] = "select episode_name, season_number, episode_number from tv_s_e  where tv_id =?";
  const char sqll[] = "select tv_s_e_name.episode_name, tv_s_e.season_number, tv_s_e.episode_number from tv_s_e, tv_s_e_name where tv_s_e_name.episode_id = tv_s_e.episode_id and tv_s_e.tv_id = ? and tv_s_e_name.language_id = ?;";
  sqlite3_stmt *stmt;
  if (!isDefaultLang) stmt = m_db->QueryPrepare(sqll, "ii", dbID(), lang->m_id);
  else stmt = m_db->QueryPrepare(sqld, "i", dbID());
  for( ; m_db->QueryStep(stmt, "sii", &episodeName, &season, &episode); ) {
    if (!episodeName) continue;
    if (!isDefaultLang) distance = sentence_distance_normed_strings(tvSearchEpisodeString, episodeName);
    else distance = sentence_distance_normed_strings(tvSearchEpisodeString, normString(episodeName));
    if (debug && (distance < 600 || (season < 3 && episode == 13)) ) esyslog("tvscraper:DEBUG cTv::searchEpisode search string \"%s\" episodeName \"%s\"  season %i episode %i dbid %i, distance %i", tvSearchEpisodeString.c_str(), episodeName, season, episode, dbID(), distance);
    if (season == 0) distance += 10; // avoid season == 0, could be making of, ...
    if (distance < best_distance) {
      best_distance = distance;
      best_season = season;
      best_episode = episode;
    }
  }
  if (debug) esyslog("tvscraper:DEBUG cTv::searchEpisode search string \"%s\" best_season %i best_episode %i dbid %i, best_distance %i", tvSearchEpisodeString.c_str(), best_season, best_episode, dbID(), best_distance);
  if (best_distance > 700) {  // accept a rather high distance here. We return the distance, so the caller can finally decide to take this episode or not
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
  if (collectionId) *collectionId = 0;
  if (collectionName) *collectionName = "";
  if (runtime) *runtime = 0;
  bool episodeDataAvailable = false, episodeImdbIdAvailable = false, episodeReleaseDateAvailable = false;
  if ((m_seasonNumber != 0 || m_episodeNumber != 0) &&
    (episodeName != NULL || imdbId != NULL || releaseDate != NULL || runtime != NULL)) {
    const char *episodeName_ = NULL;
    const char *episodeAirDate_ = NULL;
    const char *episodeIMDB_ID_ = NULL;
    int runtime_ = 0;
    char sql_e[] = "select episode_name, episode_air_date, episode_run_time, episode_IMDB_ID " \
      "from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?";
    sqlite3_stmt *statement = m_db->QueryPrepare(sql_e, "iii", dbID(), m_seasonNumber, m_episodeNumber);
    if (m_db->QueryStep(statement, "ssis", &episodeName_, &episodeAirDate_, &runtime_, &episodeIMDB_ID_)) {
      episodeDataAvailable = true;
      episodeReleaseDateAvailable = episodeAirDate_ && *episodeAirDate_;
      episodeImdbIdAvailable = episodeIMDB_ID_ && *episodeIMDB_ID_;
      if (episodeName) *episodeName = charPointerToString(episodeName_);
      if (releaseDate) *releaseDate = charPointerToString(episodeAirDate_);
      if (imdbId) *imdbId = charPointerToString(episodeIMDB_ID_);
      sqlite3_finalize(statement);
      if (runtime) *runtime = runtime_;
    }
  }

  if (title == NULL && (imdbId == NULL || episodeImdbIdAvailable) && (releaseDate == NULL || episodeReleaseDateAvailable) ) {
    if (!episodeDataAvailable) return false;
    if (collectionId) *collectionId = 0;
    return true;
  }
  const char *title_ = NULL;
  const char *IMDB_ID_ = NULL;
  const char *firstAirDate_ = NULL;
  const char sql[] = "select tv_name, tv_first_air_date, tv_IMDB_ID from tv2 where tv_id = ?";
  sqlite3_stmt *statement = m_db->QueryPrepare(sql, "i", dbID() );
  if (!m_db->QueryStep(statement, "sss", &title_, &firstAirDate_, &IMDB_ID_)) {
    if (!episodeDataAvailable) return false;
    if (title) *title = "";
    if (collectionId) *collectionId = 0;
    return true;
  }
  if (title) *title = charPointerToString(title_);
  if (!episodeDataAvailable) {
    if (imdbId) *imdbId = charPointerToString(IMDB_ID_);
    if (releaseDate) *releaseDate = charPointerToString(firstAirDate_);
    if (episodeName) *episodeName = "";
  }
  sqlite3_finalize(statement);
  return true;
}

void cTv::getScraperOverview(cGetScraperOverview *scraperOverview) {
  scraperOverview->m_videoType = eVideoType::tvShow;
  scraperOverview->m_dbid = dbID();
  scraperOverview->m_seasonNumber = m_seasonNumber;
  scraperOverview->m_episodeNumber = m_episodeNumber;
  if (scraperOverview->m_IMDB_ID) *scraperOverview->m_IMDB_ID = "";
  if (scraperOverview->m_releaseDate) *scraperOverview->m_releaseDate = "";
// we start to collect episode information. Data available from episode will not be requested from TV show
  if (m_seasonNumber != 0 || m_episodeNumber != 0)
// episode name, IMDB_ID, episodeAirDate
    if (scraperOverview->m_episodeName != NULL || scraperOverview->m_IMDB_ID != NULL || scraperOverview->m_releaseDate != NULL) {

    const char *episodeName = NULL;
    const char *episodeIMDB_ID = NULL;
    const char *episodeAirDate = NULL;
    char sql_e[] = "select episode_name, episode_air_date, episode_IMDB_ID " \
      "from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?";
    sqlite3_stmt *statement = m_db->QueryPrepare(sql_e, "iii", dbID(), m_seasonNumber, m_episodeNumber);
    if (m_db->QueryStep(statement, "sss", &episodeName, &episodeAirDate, &episodeIMDB_ID)) {
      if (scraperOverview->m_episodeName && episodeName) *scraperOverview->m_episodeName = episodeName;
      if (scraperOverview->m_IMDB_ID && episodeIMDB_ID) *scraperOverview->m_IMDB_ID = episodeIMDB_ID;
      if (scraperOverview->m_releaseDate && episodeAirDate) *scraperOverview->m_releaseDate = episodeAirDate;
      sqlite3_finalize(statement);
    }
  }

  const char *title = NULL;
  const char *IMDB_ID = NULL;
  const char *firstAirDate = NULL;
  const char sql[] = "select tv_name, tv_first_air_date, tv_IMDB_ID from tv2 where tv_id = ?";
  sqlite3_stmt *statement = m_db->QueryPrepare(sql, "i", dbID() );
  if (!m_db->QueryStep(statement, "sss", &title, &firstAirDate, &IMDB_ID)) {
    scraperOverview->m_videoType = eVideoType::none;
    return;
  }
  if (scraperOverview->m_title && title) *scraperOverview->m_title = title;
  if (scraperOverview->m_IMDB_ID && scraperOverview->m_IMDB_ID->empty() && IMDB_ID) *scraperOverview->m_IMDB_ID = IMDB_ID;
  if (scraperOverview->m_releaseDate && scraperOverview->m_releaseDate->empty() && firstAirDate) *scraperOverview->m_releaseDate = firstAirDate;
  sqlite3_finalize(statement);

// image
  if (scraperOverview->m_image)
    getSingleImageBestLO(scraperOverview->m_imageLevels, scraperOverview->m_imageOrientations, &(scraperOverview->m_image->path), NULL, &(scraperOverview->m_image->width), &(scraperOverview->m_image->height) );

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
    std::string_view pers_name = std::string_view(lDelimPos + 1, rDelimPos - lDelimPos - 1);
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
void cTv::getScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv) {
  scraperMovieOrTv->movie = false;
  scraperMovieOrTv->episodeFound = (m_seasonNumber != 0 || m_episodeNumber != 0);
  const char *posterUrl;
  if (scraperMovieOrTv->episodeFound && scraperMovieOrTv->httpImagePaths) {
    const char sql_sp[] = "select media_path from tv_media where tv_id = ? and media_number = ? and media_type = ?";
    sqlite3_stmt *statement = m_db->QueryPrepare(sql_sp, "iii", dbID(), m_seasonNumber, mediaSeason);
    if (m_db->QueryStep(statement, "s", &posterUrl)) {
      if (posterUrl && *posterUrl) scraperMovieOrTv->posterUrl = imageUrl(posterUrl);
      sqlite3_finalize(statement);
    }
  }
  const char *networks;
  const char *genres;
  const char *createdBy;
  const char *fanartUrl;
  const char sql[] = "select tv_name, tv_original_name, tv_overview, tv_first_air_date, " \
    "tv_networks, tv_genres, tv_popularity, tv_vote_average, tv_vote_count, " \
    "tv_posterUrl, tv_fanartUrl, tv_IMDB_ID, tv_status, tv_created_by " \
    "from tv2 where tv_id = ?";
  sqlite3_stmt *statement = m_db->QueryPrepare(sql, "i", dbID() );
  scraperMovieOrTv->found = m_db->QueryStep(statement, "SSSSssffissSSs",
      &scraperMovieOrTv->title, &scraperMovieOrTv->originalTitle, &scraperMovieOrTv->overview,
      &scraperMovieOrTv->releaseDate, &networks, &genres,
      &scraperMovieOrTv->popularity, &scraperMovieOrTv->voteAverage, &scraperMovieOrTv->voteCount,
      &posterUrl, &fanartUrl, &scraperMovieOrTv->IMDB_ID, &scraperMovieOrTv->status, &createdBy);
  if (!scraperMovieOrTv->found) return;
  stringToVector(scraperMovieOrTv->networks, networks);
  stringToVector(scraperMovieOrTv->genres, genres);
  stringToVector(scraperMovieOrTv->createdBy, createdBy);

  if (scraperMovieOrTv->httpImagePaths) {
    if (scraperMovieOrTv->posterUrl.empty() && 
        posterUrl && *posterUrl) scraperMovieOrTv->posterUrl = imageUrl(posterUrl);
    if (fanartUrl && *fanartUrl) scraperMovieOrTv->fanartUrl = imageUrl(fanartUrl);
  }
  sqlite3_finalize(statement);
// if no poster was found, use first season poster
  if (scraperMovieOrTv->httpImagePaths && scraperMovieOrTv->posterUrl.empty() ) {
    const char sql_spa[] =
      "select media_path from tv_media where tv_id = ? and media_number >= 0 and media_type = ?";
    for (sqlite3_stmt *statement = m_db->QueryPrepare(sql_spa, "ii", dbID(), mediaSeason);
         m_db->QueryStep(statement, "s", &posterUrl);) 
      if (posterUrl && *posterUrl) {
        scraperMovieOrTv->posterUrl = imageUrl(posterUrl);
        sqlite3_finalize(statement);
        break;
      }
   }
// runtimes
  int runtime;
  for (statement = m_db->QueryPrepare(
    "select episode_run_time from tv_episode_run_time where tv_id = ?", "i", dbID() );
    m_db->QueryStep(statement, "i", &runtime);) {
    if (runtime > 0) scraperMovieOrTv->runtimes.push_back(runtime);
  }

  scraperMovieOrTv->actors = GetActors();
  if (scraperMovieOrTv->media) {
    scraperMovieOrTv->posters = getImages(eOrientation::portrait);
    scraperMovieOrTv->fanarts = getImages(eOrientation::landscape);
    scraperMovieOrTv->banners = getBanners();
  }

// episode details
  if (!scraperMovieOrTv->episodeFound) return;

  if (scraperMovieOrTv->media) 
    getSingleImage(eImageLevel::seasonMovie, eOrientation::portrait, NULL, &scraperMovieOrTv->seasonPoster.path, &scraperMovieOrTv->seasonPoster.width, &scraperMovieOrTv->seasonPoster.height);

  scraperMovieOrTv->episode.season = m_seasonNumber;
  scraperMovieOrTv->episode.number = m_episodeNumber;
  char *director;
  char *writer;
  char *guestStars;
  char *episodeImageUrl;
  const char *sql_e = "select episode_absolute_number, episode_name, episode_air_date, " \
    "episode_vote_average, episode_vote_count, episode_overview, " \
    "episode_guest_stars, episode_director, episode_writer, episode_IMDB_ID, episode_still_path " \
    "from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?";
  statement = m_db->QueryPrepare(sql_e, "iii", dbID(), m_seasonNumber, m_episodeNumber);
  if (m_db->QueryStep(statement, "iSSfiSsssSs", 
     &scraperMovieOrTv->episode.absoluteNumber, &scraperMovieOrTv->episode.name,
     &scraperMovieOrTv->episode.firstAired,
     &scraperMovieOrTv->episode.vote_average, &scraperMovieOrTv->episode.vote_count, 
     &scraperMovieOrTv->episode.overview, &guestStars, &director, &writer,
     &scraperMovieOrTv->episode.IMDB_ID, &episodeImageUrl) ) {
    stringToVector(scraperMovieOrTv->episode.director, director);
    stringToVector(scraperMovieOrTv->episode.writer, writer);
    addGuestStars(scraperMovieOrTv->episode.guestStars, guestStars);
    if (episodeImageUrl && *episodeImageUrl && scraperMovieOrTv->httpImagePaths) {
      scraperMovieOrTv->episode.episodeImageUrl = imageUrl(episodeImageUrl);
    }
    sqlite3_finalize(statement);
  }
  if (scraperMovieOrTv->media)
    getSingleImage(eImageLevel::episodeMovie, eOrientation::landscape, NULL, &scraperMovieOrTv->episode.episodeImage.path, &scraperMovieOrTv->episode.episodeImage.width, &scraperMovieOrTv->episode.episodeImage.height);
}

void cTvTvdb::AddGuestActors(std::vector<cActor> &actors, bool fullPath) {
  char *guestStars;
  const char *sql = "select episode_guest_stars from tv_s_e where tv_id = ? and season_number = ? and episode_number = ?";
  sqlite3_stmt *statement = m_db->QueryPrepare(sql, "iii", dbID(), m_seasonNumber, m_episodeNumber);
  if (m_db->QueryStep(statement, "s", &guestStars) ) {
    addGuestStars(actors, guestStars);
    sqlite3_finalize(statement);
  }
}

// implemntation of cTvMoviedb  *********************
void cTvMoviedb::DeleteMediaAndDb() {
  stringstream folder;
  folder << config.GetBaseDirMovieTv() << m_id;
  DeleteAll(folder.str() );
  m_db->DeleteSeries(m_id);
}

void cTvMoviedb::AddGuestActors(std::vector<cActor> &actors, bool fullPath) {
// actors from guest stars
  const char sql_guests[] = "select actors.actor_id, actor_name, actor_role, actor_has_image from actors, actor_tv_episode " \
                            "where actor_tv_episode.actor_id = actors.actor_id and actor_tv_episode.episode_id = ?";
  if (m_seasonNumber != 0 || m_episodeNumber != 0) AddActors(actors, sql_guests,  m_episodeNumber, "movies/actors/actor_", fullPath);
}

std::vector<cActor> cTvMoviedb::GetActors(bool fullPath) {
  std::vector<cActor> actors;
  const char sql[] = "select actors.actor_id, actor_name, actor_role, actor_has_image from actors, actor_tv " \
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
  CONCAT(dir_path, "su", config.GetBaseDirMovieTv().c_str(), m_id);
  const std::filesystem::path fs_path(dir_path);
  if (std::filesystem::exists(fs_path) ) {
    for (auto const& dir_entry : std::filesystem::directory_iterator{fs_path}) {
      if (dir_entry.is_directory()) {
        if (checkPathC(relPath, fullPath, width, height, 780, 1108, "ss", dir_entry.path().c_str(), "/poster.jpg")) return true;
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
      return checkPathC(relPath, fullPath, width, height, 780, 1108, "sus", config.GetBaseDirMovieTv().c_str(), m_id, "/poster.jpg");
    case eOrientation::landscape:
      return checkPathC(relPath, fullPath, width, height, 1280, 720, "sus", config.GetBaseDirMovieTv().c_str(), m_id, "/backdrop.jpg");
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
  CONCAT(path, "susus", config.GetBaseDirMovieTv().c_str(), m_id, "/", m_seasonNumber, "/poster.jpg");
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
  CONCAT(path, "sususus", config.GetBaseDirMovieTv().c_str(), m_id, "/", m_seasonNumber, "/still_", m_episodeNumber, ".jpg");
  return checkPath(path, relPath, fullPath, width, height, 300, 200);
}

// implemntation of cTvTvdb  *********************
void cTvTvdb::DeleteMediaAndDb() {
  CONCAT(folder, "su", config.GetBaseDirSeries().c_str(), m_id);
  DeleteAll(folder);
  m_db->DeleteSeries(m_id * -1);
}

std::vector<cActor> cTvTvdb::GetActors(bool fullPath) {
  std::vector<cActor> actors;
  cActor actor;
  const char *actorId;
  const char sql[] = "SELECT actor_number, actor_name, actor_role FROM series_actors WHERE actor_series_id = ?";
  for (sqlite3_stmt *statement = m_db->QueryPrepare(sql, "i", m_id);
       m_db->QueryStep(statement, "sSS", &actorId, &actor.name, &actor.role); ) {
    actor.actorThumb.width = 300;
    actor.actorThumb.height = 450;
    actor.actorThumb.path = "";
    if (actorId && actorId[0] != '-') {
      CONCAT(path, "susss", config.GetBaseDirSeries().c_str(), m_id, "/actor_", actorId, ".jpg");
      if (FileExists(path)) {
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
  CONCAT(path0, "sus", config.GetBaseDirSeries().c_str(), m_id, (orientation == eOrientation::portrait)?"/poster_":"/fanart_");
  for (int i=0; i<maxImages; i++) {
    CONCAT(path, "sus", path0, i, ".jpg");
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
  CONCAT(dir_path, "su", config.GetBaseDirSeries().c_str(), m_id);

  const std::filesystem::path fs_path(dir_path);
  if (std::filesystem::exists(fs_path) ) {
    for (auto const& dir_entry : std::filesystem::directory_iterator{fs_path}) {
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
      return checkPathC(relPath, fullPath, width, height, 680, 1000, "sus", config.GetBaseDirSeries().c_str(), id, "/poster_0.jpg");
    case eOrientation::landscape:
      return checkPathC(relPath, fullPath, width, height, 1920, 1080, "sus", config.GetBaseDirSeries().c_str(), id, "/fanart_0.jpg");
    case eOrientation::banner:
      return checkPathC(relPath, fullPath, width, height, 758, 140, "sus", config.GetBaseDirSeries().c_str(), id, "/banner.jpg");
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
  CONCAT(path, "susus", config.GetBaseDirSeries().c_str(), m_id, "/season_poster_", m_seasonNumber, ".jpg");
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
  CONCAT(path, "sususus", config.GetBaseDirSeries().c_str(), m_id, "/", m_seasonNumber, "/still_", m_episodeNumber, ".jpg");
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

cMovieOrTv *cMovieOrTv::getMovieOrTv(const cTVScraperDB *db, csEventOrRecording *sEventOrRecording, int *runtime) {
  if (sEventOrRecording == NULL) return NULL;
  int movie_tv_id, season_number, episode_number;
  if(!db->GetMovieTvID(sEventOrRecording, movie_tv_id, season_number, episode_number, runtime)) return NULL;

  if(season_number == -100) return new cMovieMoviedb(db, movie_tv_id);
  if ( movie_tv_id > 0) return new cTvMoviedb(db, movie_tv_id, season_number, episode_number);
        else            return new cTvTvdb(db, -1*movie_tv_id, season_number, episode_number);
}

cMovieOrTv *cMovieOrTv::getMovieOrTv(const cTVScraperDB *db, const cEvent *event) {
  if (!event) return NULL;
  int movie_tv_id, season_number, episode_number;
  if(!db->GetMovieTvID(event, movie_tv_id, season_number, episode_number)) return NULL;

  if(season_number == -100) return new cMovieMoviedb(db, movie_tv_id);
  if ( movie_tv_id > 0) return new cTvMoviedb(db, movie_tv_id, season_number, episode_number);
        else            return new cTvTvdb(db, -1*movie_tv_id, season_number, episode_number);
}

cMovieOrTv *cMovieOrTv::getMovieOrTv(const cTVScraperDB *db, const cRecording *recording) {
  if (!recording) return NULL;
  int movie_tv_id, season_number, episode_number;
  if(!db->GetMovieTvID(recording, movie_tv_id, season_number, episode_number)) return NULL;

  if(season_number == -100) return new cMovieMoviedb(db, movie_tv_id);
  if ( movie_tv_id > 0) return new cTvMoviedb(db, movie_tv_id, season_number, episode_number);
        else            return new cTvTvdb(db, -1*movie_tv_id, season_number, episode_number);
}

// search episode
int cMovieOrTv::searchEpisode(const cTVScraperDB *db, sMovieOrTv &movieOrTv, string_view tvSearchEpisodeString, string_view baseNameOrTitle, const vector<int> &years, const cLanguage *lang) {
  bool debug = false;
  movieOrTv.season  = 0;
  movieOrTv.episode = 0;
  cMovieOrTv *mv =  cMovieOrTv::getMovieOrTv(db, movieOrTv);
  if (!mv) return 1000;
  int distance = mv->searchEpisode(tvSearchEpisodeString, baseNameOrTitle, years, lang);
  
  if (debug) esyslog("tvscraper:DEBUG cTvMoviedb::earchEpisode search string \"%.*s\" season %i episode %i", static_cast<int>(tvSearchEpisodeString.length()), tvSearchEpisodeString.data(), mv->m_seasonNumber, mv->m_episodeNumber);
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
  db->execSql(sql, "i", mediaSeason);
  const char *sql2 = "select tv_id from tv_media";
  int tv_id;
  std::set<int> tv_ids;
  for (sqlite3_stmt *statement = db->QueryPrepare(sql2, ""); db->QueryStep(statement, "i", &tv_id);)
    tv_ids.insert(tv_id);
  for (const int &tv_id2: tv_ids) {
    if (db->CheckMovieOutdatedEvents(tv_id2, 0, 0)) continue;
    if (db->CheckMovieOutdatedRecordings(tv_id2, 0, 0)) continue;
    db->deleteTvMedia (tv_id2, false, false);
  }
}

void deleteOutdatedRecordingImages(const cTVScraperDB *db) {
// check recording. Delete if this does not exist
if (!CheckDirExists(config.GetBaseDirRecordings().c_str() )) return;
for (const std::filesystem::directory_entry& dir_entry : 
        std::filesystem::directory_iterator{config.GetBaseDirRecordings() }) 
  {
    std::vector<std::string> parts = getSetFromString<std::string,std::vector<std::string>>(dir_entry.path().filename().string().c_str(), '_');
    if (parts.size() != 3) {
      esyslog("tvscraper, ERROR, deleteOutdatedRecordingImages, parts.size: %zu, filename: %s", parts.size(), dir_entry.path().filename().string().c_str());
      continue;
    }
    tEventID eventID = 0;
    time_t eventStartTime = 0;
    auto result = std::from_chars(parts[0].data(), parts[0].data() + parts[0].length(), eventStartTime);
    if (result.ec == std::errc::invalid_argument) {
      esyslog("tvscraper, ERROR, deleteOutdatedRecordingImages, parts[0]: %.*s, filename: %s", static_cast<int>(parts[0].length()), parts[0].data(), dir_entry.path().filename().string().c_str());
      continue;
    }
    result = std::from_chars(parts[2].data(), parts[2].data() + parts[2].length() - 4, eventID);
    if (result.ec == std::errc::invalid_argument) {
      esyslog("tvscraper, ERROR, deleteOutdatedRecordingImages, parts[2]: %.*s, filename: %s", static_cast<int>(parts[2].length()), parts[2].data(), dir_entry.path().filename().string().c_str());
      continue;
    }
    const char *sql = "SELECT count(event_id) FROM recordings2 WHERE event_id = ? AND event_start_time = ? AND channel_id = ?";
    bool found = db->QueryInt(sql, "its", (int)eventID, eventStartTime, parts[1].c_str() ) > 0;
//    esyslog("tvscraper, DEBUG, recording eventID %i eventStartTime %lld channel_id %.*s %s", (int)eventID, (long long)eventStartTime, static_cast<int>(parts[1].length()), parts[1].data(), found?"found":"not found");
    if (!found) DeleteFile(config.GetBaseDirRecordings() + dir_entry.path().filename().string());
  }
}

void deleteOutdatedEpgImages() {
// check event start time. Delete if older than yesterday
if (!CheckDirExists(config.GetBaseDirEpg().c_str() )) return;
for (const std::filesystem::directory_entry& dir_entry : 
        std::filesystem::directory_iterator{config.GetBaseDirEpg() }) 
  {
    if (!dir_entry.is_directory() ) continue;
    if (dir_entry.path().filename().string().find_first_not_of("0123456789") != std::string::npos) continue;
    if (atoll(dir_entry.path().filename().string().c_str() ) + 24*60*60 < time(0) ) DeleteAll(config.GetBaseDirEpg() + dir_entry.path().filename().string());
  }
}
void cMovieOrTv::DeleteAllIfUnused(const cTVScraperDB *db) {
// check all movies in db
  CleanupTv_media(db);
  int movie_id;
  for (sqlite3_stmt *statement = db->GetAllMovies();
       db->QueryStep(statement, "i", &movie_id);) {
    cMovieMoviedb movieMoviedb(db, movie_id);
    movieMoviedb.DeleteIfUnused();
  }

// check all tv shows in db
  int tvID;
  for (sqlite3_stmt *statement = db->QueryPrepare("select tv_id from tv2;", "");
       db->QueryStep(statement, "i", &tvID);) {
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
for (const std::filesystem::directory_entry& dir_entry : 
        std::filesystem::directory_iterator{folder}) 
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
for (const std::filesystem::directory_entry& dir_entry : 
        std::filesystem::directory_iterator{config.GetBaseDirMovies() }) 
  {
    if (! dir_entry.is_regular_file() ) continue;
    size_t pos = dir_entry.path().filename().string().find_first_not_of("0123456789");
    if (dir_entry.path().filename().string()[pos] != '_') continue;
    cMovieMoviedb movieMoviedb(db, atoi(dir_entry.path().filename().string().c_str() ));
    movieMoviedb.DeleteIfUnused();
  }
}


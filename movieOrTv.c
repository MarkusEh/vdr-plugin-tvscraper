#include "movieOrTv.h"
#include <fstream>
#include <iostream>
#include <filesystem>

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

void cMovieOrTv::AddActors(std::vector<cActor> &actors, const char *sql, int id, const char *pathPart, int width, int height) {
// adds the acors found with sql&id to the list of actors
// works for all actos found in themoviedb (movie & tv actors). Not for actors in thetvdb
  cActor actor;
  const char *actorId;
  int hasImage;
  const string basePath = config.GetBaseDir() + pathPart;
  for (sqlite3_stmt *statement = m_db->QueryPrepare(sql, "i", id);
       m_db->QueryStep(statement, "sSSi", &actorId, &actor.name, &actor.role, &hasImage); ) {
    if (hasImage) {
      actor.actorThumb.width = width;
      actor.actorThumb.height = height;
      actor.actorThumb.path = basePath + actorId + ".jpg";
    } else {
      actor.actorThumb.width = width;
      actor.actorThumb.height = height;
      actor.actorThumb.path = "";
    }
    actors.push_back(actor);
  }
}

// implemntation of cMovieMoviedb  *********************
void cMovieMoviedb::DeleteMediaAndDb() {
  stringstream base;
  base << config.GetBaseDir() << "/movies/" << m_id;
  DeleteFile(base.str() + "_backdrop.jpg");
  DeleteFile(base.str() + "_poster.jpg");
  m_db->DeleteMovie(m_id);
} 
string cMovieMoviedb::getPosterUrl() {
  string path = m_db->getMoviePosterUrl(dbID() );
  if (path.empty() ) return path;
  return bannerBaseUrl() + path;
}

string cMovieMoviedb::getFanartUrl() {
  string path = m_db->getMovieFanartUrl(dbID() );
  if (path.empty() ) return path;
  return bannerBaseUrl() + path;
}
void cMovieMoviedb::getScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv, cImageServer *imageServer) {
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
    if (posterUrl && *posterUrl) scraperMovieOrTv->posterUrl = bannerBaseUrl() + posterUrl;
    if (fanartUrl && *fanartUrl) scraperMovieOrTv->fanartUrl = bannerBaseUrl() + fanartUrl;
  }
  sqlite3_finalize(statement);

  scraperMovieOrTv->actors = GetActors();
  if (scraperMovieOrTv->media) {
    scraperMovieOrTv->posters = imageServer->GetPosters(m_id, scrapMovie);
    cTvMedia fanart = imageServer->GetMovieFanart(m_id);
    if (fanart.width != 0) scraperMovieOrTv->fanarts.push_back(fanart);
    if (scraperMovieOrTv->collectionId) {
      scraperMovieOrTv->collectionPoster = imageServer->GetCollectionPoster(scraperMovieOrTv->collectionId);
      scraperMovieOrTv->collectionFanart = imageServer->GetCollectionFanart(scraperMovieOrTv->collectionId);
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

std::vector<cActor> cMovieMoviedb::GetActors() {
  std::vector<cActor> actors;
  const char sql[] = "select actors.actor_id, actor_name, actor_role, actor_has_image from actors, actor_movie " \
                     "where actor_movie.actor_id = actors.actor_id and actor_movie.movie_id = ?";
  AddActors(actors, sql,  dbID(), "/movies/actors/actor_");
  return actors;
}

// implemntation of cTv  *********************
string cTv::getPosterUrl() {
  if (m_seasonNumber != 0 || m_episodeNumber !=0) {
// is there a poster for this season?
    for (vector<string> &media: m_db->GetTvMedia(dbID(), mediaSeason, false) )
      if (media.size() == 2 && atoi(media[1].c_str()) == m_seasonNumber && !media[0].empty() ) return bannerBaseUrl() + media[0];
  }
// no season information available, or no season poster
  string path = m_db->getTvPosterUrl(dbID() );
  if (!path.empty() ) return bannerBaseUrl() + path;
// no poster for tv show, no poster for this season ... try poster for any season
  for (vector<string> &media: m_db->GetTvMedia(dbID(), mediaSeason, false) )
    if (media.size() == 2 && !media[0].empty() ) return bannerBaseUrl() + media[0];
  return ""; // no poster found
}

string cTv::getFanartUrl() {
  string path = m_db->getTvFanartUrl(dbID() );
  if (path.empty() ) return path;
  return bannerBaseUrl() + path;
}

string cTv::getStillUrl() {
  string path = m_db->GetEpisodeStillPath(dbID(), m_seasonNumber, m_episodeNumber);
  if (path.empty() ) return path;
  return bannerBaseUrl() + path;
}

std::size_t cTv::searchEpisode(const string &tvSearchEpisodeString) {
// return 0, if no match was found
// otherwise, number of matching characters & set m_seasonNumber and m_episodeNumber
   std::size_t nmatch = searchEpisode_int(tvSearchEpisodeString);
   if (nmatch > 0) return nmatch;
   if (!isdigit(tvSearchEpisodeString[0]) ) return nmatch;
   std::size_t found_blank = tvSearchEpisodeString.find(' ');
   if (found_blank == std::string::npos || found_blank > 7 || found_blank + 1 >= tvSearchEpisodeString.length() ) return nmatch;
   return searchEpisode_int(tvSearchEpisodeString.substr(found_blank + 1) );
}

std::size_t cTv::searchEpisode_int(const string &tvSearchEpisodeString) {
// return 0, if no match was found
// otherwise, number of matching characters & set m_seasonNumber and m_episodeNumber
  m_seasonNumber = 0;
  m_episodeNumber = 0;
  std::size_t min_search_string_length = 4;
  std::size_t found_blank = tvSearchEpisodeString.find(' ');
  if (found_blank !=std::string::npos && found_blank < min_search_string_length) min_search_string_length += found_blank;
  if (tvSearchEpisodeString.length() < min_search_string_length) return 0;
  bool found;
  found = m_db->SearchTvEpisode(dbID(), tvSearchEpisodeString, m_seasonNumber, m_episodeNumber);
  if (found) return tvSearchEpisodeString.length();
  found = m_db->SearchTvEpisode(dbID(), tvSearchEpisodeString + "%", m_seasonNumber, m_episodeNumber);
  if (found) return tvSearchEpisodeString.length();
  if (tvSearchEpisodeString.length() > 7) {
    found = m_db->SearchTvEpisode(dbID(), "%" + tvSearchEpisodeString + "%", m_seasonNumber, m_episodeNumber);
    if (found) return tvSearchEpisodeString.length();
  }
  std::size_t il, im, ih;
  il = 0;
  ih = tvSearchEpisodeString.length();
  im = ih;
  while(ih - il > 1) {
    im = (ih - il)/2 + il;
    if(im < min_search_string_length) {
      if (ih <= min_search_string_length) return 0;
      im = min_search_string_length;
    }
    found = m_db->SearchTvEpisode(dbID(), ((im > 7)?"%":"") + tvSearchEpisodeString.substr(0, im) + "%", m_seasonNumber, m_episodeNumber);
    if (!found) ih = im;
      else il = im;
  }
  if (found) {
// sanity check: do we have a sufficient match?
    if (im > min(getEpisodeName().length(), tvSearchEpisodeString.length() ) * 60 / 100) return im;
    m_seasonNumber = 0;
    m_episodeNumber = 0;
    return 0;
  }
  found = m_db->SearchTvEpisode(dbID(), ((im -1 > 7)?"%":"") + tvSearchEpisodeString.substr(0, im - 1) + "%", m_seasonNumber, m_episodeNumber);
  if (found) {
// sanity check: do we have a sufficient match?
    if (im -1 > min(getEpisodeName().length(), tvSearchEpisodeString.length() ) * 60 / 100) return im-1;
  }
  m_seasonNumber = 0;
  m_episodeNumber = 0;
  return 0;
}

void cTv::getScraperMovieOrTv(cScraperMovieOrTv *scraperMovieOrTv, cImageServer *imageServer) {
  scraperMovieOrTv->movie = false;
  scraperMovieOrTv->episodeFound = (m_seasonNumber != 0 || m_episodeNumber != 0);
  const char *posterUrl;
  if (scraperMovieOrTv->episodeFound && scraperMovieOrTv->httpImagePaths) {
    const char sql_sp[] = "select media_path from tv_media where tv_id = ? and media_number = ? and and media_type = ?";
    sqlite3_stmt *statement = m_db->QueryPrepare(sql_sp, "iii", dbID(), m_seasonNumber, mediaSeason);
    if (m_db->QueryStep(statement, "s", &posterUrl)) {
      if (posterUrl && *posterUrl) scraperMovieOrTv->posterUrl = bannerBaseUrl() + posterUrl;
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
        posterUrl && *posterUrl) scraperMovieOrTv->posterUrl = bannerBaseUrl() + posterUrl;
    if (fanartUrl && *fanartUrl) scraperMovieOrTv->fanartUrl = bannerBaseUrl() + fanartUrl;
  }
  sqlite3_finalize(statement);
// if no poster was found, use first season poster
  if (scraperMovieOrTv->httpImagePaths && scraperMovieOrTv->posterUrl.empty() ) {
    const char sql_spa[] =
      "select media_path from tv_media where tv_id = ? and media_number >= 0 and and media_type = ?";
    for (sqlite3_stmt *statement = m_db->QueryPrepare(sql_spa, "ii", dbID(), mediaSeason);
         m_db->QueryStep(statement, "s", &posterUrl);) 
      if (posterUrl && *posterUrl) {
        scraperMovieOrTv->posterUrl = bannerBaseUrl() + posterUrl;
        sqlite3_finalize(statement);
        break;
      }
   }
// runtimes
  int runtime;
  for (statement = m_db->QueryPrepare(
    "select episode_run_time from tv_episode_run_time where tv_id = ?", "i", dbID() );
    m_db->QueryStep(statement, "i", &runtime);) {
    scraperMovieOrTv->runtimes.push_back(runtime);
  }

  scraperMovieOrTv->actors = GetActors();
  if (scraperMovieOrTv->media) {
    scraperMovieOrTv->posters = imageServer->GetPosters(dbID(), scrapSeries);
    scraperMovieOrTv->fanarts = imageServer->GetSeriesFanarts(dbID(), m_seasonNumber, m_episodeNumber);
    cTvMedia media;
    if (imageServer->GetBanner(media, dbID() ) ) scraperMovieOrTv->banners.push_back(media);
  }

// episode details
  if (!scraperMovieOrTv->episodeFound) return;

  if (scraperMovieOrTv->media) 
    scraperMovieOrTv->seasonPoster = imageServer->GetPoster(dbID(), m_seasonNumber, m_episodeNumber);

  scraperMovieOrTv->episode.season = m_seasonNumber;
  scraperMovieOrTv->episode.number = m_episodeNumber;
  char *director;
  char *writer;
  char *guestStars;
  char *episodeImageUrl;
  char sql_e[] = "select episode_absolute_number, episode_name, episode_air_date, " \
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
    scraperMovieOrTv->episode.guestStars = getGuestStars(guestStars);
    if (episodeImageUrl && *episodeImageUrl && scraperMovieOrTv->httpImagePaths) scraperMovieOrTv->episode.episodeImageUrl = bannerBaseUrl() + episodeImageUrl;
    sqlite3_finalize(statement);
  }
  if (scraperMovieOrTv->media)
    scraperMovieOrTv->episode.episodeImage = imageServer->GetStill(dbID(), m_seasonNumber, m_episodeNumber);
}

std::vector<cActor> cTv::getGuestStars(const char *str) {
  std::vector<cActor> result;
  if (!str || !*str) return result;
  cActor actor;
  actor.role = "";
  actor.actorThumb.path = "";
  actor.actorThumb.width = 0;
  actor.actorThumb.height = 0;
  if (str[0] != '|') { actor.name = str; result.push_back(actor); return result; }
  const char *lDelimPos = str;
  for (const char *rDelimPos = strchr(lDelimPos + 1, '|'); rDelimPos != NULL; rDelimPos = strchr(lDelimPos + 1, '|') ) {
    actor.name = string(lDelimPos + 1, rDelimPos - lDelimPos - 1);
    result.push_back(actor);
    lDelimPos = rDelimPos;
  }
  return result;
}

// implemntation of cTvMoviedb  *********************
void cTvMoviedb::DeleteMediaAndDb() {
  stringstream folder;
  folder << config.GetBaseDir() << "/movies/tv/" << m_id;
  DeleteAll(folder.str() );
  m_db->DeleteSeries(m_id);
}

std::vector<cActor> cTvMoviedb::GetActors() {
  std::vector<cActor> actors;
  const char sql[] = "select actors.actor_id, actor_name, actor_role, actor_has_image from actors, actor_tv " \
                     "where actor_tv.actor_id = actors.actor_id and actor_tv.tv_id = ?";
  AddActors(actors, sql,  dbID(), "/movies/actors/actor_");
// actors from guest stars
  const char sql_guests[] = "select actors.actor_id, actor_name, actor_role, actor_has_image from actors, actor_tv_episode " \
                            "where actor_tv_episode.actor_id = actors.actor_id and actor_tv_episode.episode_id = ?";
  if (m_seasonNumber != 0 || m_episodeNumber != 0) AddActors(actors, sql_guests,  m_episodeNumber, "/movies/actors/actor_");
  return actors;
}

// implemntation of cTvTvdb  *********************
void cTvTvdb::DeleteMediaAndDb() {
  stringstream folder;
  folder << config.GetBaseDir() << "/series/" << m_id;
  DeleteAll(folder.str() );
  m_db->DeleteSeries(m_id * -1);
}

std::vector<cActor> cTvTvdb::GetActors() {
  std::vector<cActor> actors;
// base path
  stringstream basePath_stringstream;
  basePath_stringstream  << config.GetBaseDir() << "/series/" << m_id << "/actor_";
  const string basePath = basePath_stringstream.str();
// add actors
  cActor actor;
  const char *actorId;
  const char sql[] = "SELECT actor_number, actor_name, actor_role FROM series_actors WHERE actor_series_id = ?";
  for (sqlite3_stmt *statement = m_db->QueryPrepare(sql, "i", m_id);
       m_db->QueryStep(statement, "sSS", &actorId, &actor.name, &actor.role); ) {
    if (actorId && actorId[0] != '-') {
      actor.actorThumb.width = 300;
      actor.actorThumb.height = 450;
      actor.actorThumb.path = basePath + actorId + ".jpg";
    } else {
      actor.actorThumb.width = 300;
      actor.actorThumb.height = 450;
      actor.actorThumb.path = "";
    }
    actors.push_back(actor);
  }
  return actors;
}

// static methods  *********************
// create object ***
cMovieOrTv *cMovieOrTv::getMovieOrTv(cTVScraperDB *db, int id, ecMovieOrTvType type) {
  switch (type) {
    case theMoviedbMovie:  return new cMovieMoviedb(db, id);
    case theMoviedbSeries: return new cTvMoviedb(db, id);
    case theTvdbSeries:    return new cTvTvdb(db, id);
  }
  return NULL;
}

cMovieOrTv *cMovieOrTv::getMovieOrTv(cTVScraperDB *db, const sMovieOrTv &movieOrTv) {
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

cMovieOrTv *cMovieOrTv::getMovieOrTv(cTVScraperDB *db, csEventOrRecording *sEventOrRecording) {
  if (sEventOrRecording == NULL) return NULL;
  int movie_tv_id, season_number, episode_number;
  if(!db->GetMovieTvID(sEventOrRecording, movie_tv_id, season_number, episode_number)) return NULL;

  if(season_number == -100) return new cMovieMoviedb(db, movie_tv_id);

  if ( movie_tv_id > 0) return new cTvMoviedb(db, movie_tv_id, season_number, episode_number);
        else            return new cTvTvdb(db, -1*movie_tv_id, season_number, episode_number);
}

// search episode
std::size_t cMovieOrTv::searchEpisode(cTVScraperDB *db, sMovieOrTv &movieOrTv, const string &tvSearchEpisodeString) {
  movieOrTv.season  = 0;
  movieOrTv.episode = 0;
  cMovieOrTv *mv =  cMovieOrTv::getMovieOrTv(db, movieOrTv);
  if (!mv) return 0;
  std::size_t result = mv->searchEpisode(tvSearchEpisodeString);
  if (result > 0) {
    movieOrTv.season  = mv->m_seasonNumber;
    movieOrTv.episode = mv->m_episodeNumber;
  }
  delete (mv);
  return result;
}

// delete unused *****
void cMovieOrTv::DeleteAllIfUnused(cTVScraperDB *db) {
// check all movies in db
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

  DeleteAllIfUnused(config.GetBaseDir() + "/movies/tv", theMoviedbSeries, db);
  DeleteAllIfUnused(config.GetBaseDir() + "/series", theTvdbSeries, db);
  cMovieMoviedb::DeleteAllIfUnused(db);
// delete all outdated events
  db->ClearOutdated();
}

void cMovieOrTv::DeleteAllIfUnused(const string &folder, ecMovieOrTvType type, cTVScraperDB *db) {
// check for all subfolders in folder. If a subfolder has only digits, delete the movie/tv with this number
for (const std::filesystem::directory_entry& dir_entry : 
        std::filesystem::directory_iterator{folder}) 
  {
    if (! dir_entry.is_directory() ) continue;
    if (dir_entry.path().filename().string().find_first_not_of("0123456789") != std::string::npos) continue;
    cMovieOrTv *movieOrTv = getMovieOrTv(db, atoi(dir_entry.path().filename().string().c_str() ), type);
    movieOrTv->DeleteIfUnused();
    if(movieOrTv) delete(movieOrTv);
  }
}

void cMovieMoviedb::DeleteAllIfUnused(cTVScraperDB *db) {
// check for all files in folder. If a file has the pattern for movie backdrop or poster, check the movie with this number
for (const std::filesystem::directory_entry& dir_entry : 
        std::filesystem::directory_iterator{config.GetBaseDir() + "/movies"}) 
  {
    if (! dir_entry.is_regular_file() ) continue;
    size_t pos = dir_entry.path().filename().string().find_first_not_of("0123456789");
    if (dir_entry.path().filename().string()[pos] != '_') continue;
    cMovieMoviedb movieMoviedb(db, atoi(dir_entry.path().filename().string().c_str() ));
    movieMoviedb.DeleteIfUnused();
  }
}


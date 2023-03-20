#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <jansson.h>
#include "themoviedbscraper.h"

using namespace std;

cMovieDBScraper::cMovieDBScraper(string baseDir, cTVScraperDB *db, cOverRides *overrides) {
    apiKey = "abb01b5a277b9c2c60ec0302d83c5ee9";
    baseURL = "api.themoviedb.org/3";
    this->baseDir = baseDir;
    this->db = db;
    this->overrides = overrides;
    posterSize = "w780";
    stillSize = "w300";
    backdropSize = "w1280";
    actorthumbSize = "h632";
}

cMovieDBScraper::~cMovieDBScraper() {
}

int cMovieDBScraper::GetMovieRuntime(int movieID) {
// return 0 if no runtime is available in themovied
  int runtime = db->GetMovieRuntime(movieID);
  if (runtime  >  0) return runtime;
  if (runtime == -1) return 0; // no runtime available in themoviedb
// themoviedb never checked for runtime, check now
  cMovieDbMovie movie(db, this);
  movie.SetID(movieID);
  StoreMovie(movie, true);
  runtime = db->GetMovieRuntime(movieID);
  if (runtime  >  0) return runtime;
  if (runtime == -1) return 0; // no runtime available in themoviedb
  esyslog("tvscraper: ERROR, no runtime in cMovieDBScraper::GetMovieRuntime movieID = %d", movieID);
  return runtime;
}

void cMovieDBScraper::UpdateTvRuntimes(int tvID) {
  cMovieDbTv tv(db, this);
  tv.SetTvID(tvID);
  tv.UpdateDb(true);
}

void cMovieDBScraper::StoreMovie(cMovieDbMovie &movie, bool forceUpdate) {
// if movie does not exist in DB, download movie and store it in DB
      int movieID = movie.ID();
      if (!forceUpdate && db->MovieExists(movieID)) return;
      if (config.enableDebug && !forceUpdate) esyslog("tvscraper: movie \"%i\" does not yet exist in db", movieID);
      if(!movie.ReadMovie()) {
        esyslog("tvscraper: ERROR reading movie \"%i\" ", movieID);
        return;
      }
      movie.StoreDB();
      cMovieDbActors *actors = ReadActors(movieID);
      if (!actors) return;
      actors->StoreDB(db, movieID);
      StoreMedia(&movie, actors);
      delete actors;
}
bool cMovieDBScraper::Connect(void) {
    stringstream url;
    url << baseURL << "/configuration?api_key=" << apiKey;
    cLargeString configJSON("cMovieDBScraper::Connect", 1500);
    if (CurlGetUrl(url.str().c_str(), configJSON)) {
       json_t *root;
       root = json_loads(configJSON.c_str(), 0, NULL);
       if (!root) return false;
       bool ret = parseJSON(root);
       json_decref(root);
       return ret;
    }
    return false;
}

bool cMovieDBScraper::parseJSON(json_t *root) {
// parese result of https://api.themoviedb.org/3/configuration
    if(!json_is_object(root)) {
        return false;
    }
    json_t *images;
    images = json_object_get(root, "images");
    if(!json_is_object(images)) {
        return false;
    }
    
    json_t *imgUrl;
    imgUrl = json_object_get(images, "base_url");
    if(!json_is_string(imgUrl)) {
        return false;
    }
    imageUrl = json_string_value(imgUrl);
    return true;
}

cMovieDbActors *cMovieDBScraper::ReadActors(int movieID) {
    stringstream url;
    url << baseURL << "/movie/" << movieID << "/casts?api_key=" << apiKey;
    cLargeString actorsJSON("cMovieDBScraper::ReadActors", 10000);
    cMovieDbActors *actors = NULL;
    if (CurlGetUrl(url.str().c_str(), actorsJSON)) {
        actors = new cMovieDbActors(actorsJSON.c_str() );
        actors->ParseJSON();
    }
    return actors;
}

void cMovieDBScraper::StoreMedia(cMovieDbMovie *movie, cMovieDbActors *actors) {
    movie->StoreMedia();
    // actors->Store(actorsUrl.str(), actorsDestDir.str());
    actors->Store(db, movie->ID() );
}

void cMovieDBScraper::DownloadActors(int tvID, bool movie) {
  CONCATENATE(actorsUrl, imageUrl, actorthumbSize);
  CONCATENATE(actorsDestDir, baseDir, "/actors");
  CreateDirectory(actorsDestDir);

  const char *sql = "select actor_id, actor_path from actor_download where movie_id = ? and is_movie = ?";
  for (cSql &stmt: cSql(db, sql, tvID,  movie)) {
    const char *actor_path = stmt.getCharS(1);
    if (!actor_path || !*actor_path) continue;
    CONCATENATE(actorsFullUrl, actorsUrl, actor_path);
    CONCATENATE(downloadFullPath, actorsDestDir, "/actor_", stmt.getInt(0), ".jpg");
    Download(actorsFullUrl, downloadFullPath);
  }
  db->DeleteActorDownload (tvID, movie);
}

void cMovieDBScraper::DownloadMediaTv(int tvID) {
  const char *sql = "select media_path, media_number from tv_media where tv_id = ? and media_type = ? and media_number >= 0";
// if mediaType == season_poster, media_number is the season
  for (cSql &statement: cSql(db, sql, tvID, (int)mediaSeason)) {
    std::string baseDirDownload = concatenate(GetTvBaseDir(), tvID);
    CreateDirectory(baseDirDownload);
    baseDirDownload += "/";
    DownloadFile(GetPosterBaseUrl(), statement.getCharS(0), baseDirDownload, statement.getInt(1), "/poster.jpg", false);
    break;
  }
  const char *sql2 = "select media_path from tv_media where tv_id = ? and media_type = ? and media_number >= 0";
  for (int type = 1; type <= 2; type++) {
    for (cSql &statement: cSql(db, sql2, tvID, type)) {
      DownloadFile(GetPosterBaseUrl(), statement.getCharS(0), GetTvBaseDir(), tvID, type==1?"/poster.jpg":"/backdrop.jpg", false);
      break;
    }
  }
  db->deleteTvMedia (tvID, false, true);
}
void cMovieDBScraper::DownloadMedia(int movieID) {
  const char *sql = "select media_path, media_number from tv_media where tv_id = ? and media_type = ? and media_number < 0";
  std::string posterUrl = concatenate(imageUrl, posterSize);
  std::string backdropUrl = concatenate(imageUrl, backdropSize);
  std::string destDir = concatenate(baseDir, "/");
  std::string destDirCollections = concatenate(destDir, "collections/");
  CreateDirectory(destDirCollections);

  for (int type = -2; type <= 2; type ++) if (type !=0) {
    for (cSql &statement: cSql(db, sql, movieID, type)) {
      int media_number = -1 * statement.getInt(1);
      bool isPoster = abs(type)==1;
      DownloadFile(isPoster?posterUrl:backdropUrl, statement.getCharS(0), (type > 0)?destDir:destDirCollections, type > 0?movieID:media_number, isPoster?"_poster.jpg":"_backdrop.jpg", true);
      break;
    }
  }
  db->deleteTvMedia (movieID, true, true);
}
bool cMovieDBScraper::DownloadFile(const string &urlBase, const string &urlFileName, const string &destDir, int destID, const char * destFileName, bool movie) {
// download urlBase urlFileName to destDir destID destFileName
// for tv shows (movie == false), create the directory destDir destID
    if(urlFileName.empty() ) return false;
    stringstream destFullPath;
    destFullPath << destDir << destID;
    if (!movie) CreateDirectory(destFullPath.str() );
    destFullPath << destFileName;
    stringstream urlFull;
    urlFull << urlBase << urlFileName;
    return Download(urlFull.str(), destFullPath.str());
}

void cMovieDBScraper::StoreStill(int tvID, int seasonNumber, int episodeNumber, const string &stillPathTvEpisode) {
  if (stillPathTvEpisode.empty() ) return;
  string stillUrl = GetStillBaseUrl() + stillPathTvEpisode;
  stringstream pathStill;
  pathStill << GetTvBaseDir() << tvID;
  CreateDirectory(pathStill.str() );
  pathStill << "/" << seasonNumber;
  CreateDirectory(pathStill.str() );
  pathStill << "/still_" << episodeNumber;
  pathStill << ".jpg";
  Download(stillUrl, pathStill.str());
  return;
}


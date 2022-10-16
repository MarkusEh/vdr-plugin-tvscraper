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
  StoreMovie(movie);
  runtime = db->GetMovieRuntime(movieID);
  if (runtime == -1) return 0; // no runtime available in themoviedb
  return runtime;
}

vector<vector<string>> cMovieDBScraper::GetTvRuntimes(int tvID) {
  vector<vector<string>> runtimes = db->GetTvRuntimes(tvID);
  if (runtimes.size() > 0) return runtimes;
// themoviedb never checked for runtime, check now
  cMovieDbTv tv(db, this);
  tv.SetTvID(tvID);
  tv.UpdateDb(false);
  return db->GetTvRuntimes(tvID);
}

void cMovieDBScraper::StoreMovie(cMovieDbMovie &movie) {
// if movie does not exist in DB, download movie and store it in DB
      int movieID = movie.ID();
      if (db->MovieExists(movieID)) return;
      if (config.enableDebug) esyslog("tvscraper: movie \"%i\" does not yet exist in db", movieID);
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
  stringstream actorsUrl;
  actorsUrl << imageUrl << actorthumbSize;
  stringstream actorsDestDir;
  actorsDestDir << baseDir << "/actors";
  CreateDirectory(actorsDestDir.str());
  for (const vector<string> &actor: db->GetActorDownload(tvID, movie) )
    if (actor.size() == 2 && !actor[1].empty() )
      Download(actorsUrl.str() + actor[1], actorsDestDir.str() + "/actor_" + actor[0] + ".jpg");
  db->DeleteActorDownload (tvID, movie);
}

void cMovieDBScraper::DownloadMediaTv(int tvID) {
  int season;
  const char *path;
  for (sqlite3_stmt *statement = db->QueryPrepare("select media_path, media_number from tv_media where tv_id = ? and media_type = ? and media_number >= 0", "ii", tvID, (int)mediaSeason);
    db->QueryStep(statement, "si", &path, &season);) {
      stringstream pathPoster;
      pathPoster << GetTvBaseDir() << tvID;
      CreateDirectory(pathPoster.str() );
      pathPoster << "/";
      DownloadFile(GetPosterBaseUrl(), path, pathPoster.str(), season, "/poster.jpg", false);
      sqlite3_finalize(statement);
      break;
  }
  for (int type = 1; type <= 2; type ++) {
    for (vector<string> &media:db->GetTvMedia(tvID, (eMediaType) type, false) ) if (media.size() == 2 ) {
      int season = atoi(media[1].c_str() );
      if (season >= 0) {
        DownloadFile(GetPosterBaseUrl(), media[0], GetTvBaseDir(), tvID, type==1?"/poster.jpg":"/backdrop.jpg", false);
        break;
      }
    }
  }
  db->deleteTvMedia (tvID, false, true);
}
void cMovieDBScraper::DownloadMedia(int movieID) {
  stringstream posterUrl;
  posterUrl << imageUrl << posterSize;
  stringstream backdropUrl;
  backdropUrl << imageUrl << backdropSize;
  stringstream destDir;
  destDir << baseDir << "/";
  string destDirCollections = destDir.str() +  "collections/";
  CreateDirectory(destDirCollections);

  for (int type = -2; type <= 2; type ++) if (type !=0) {
    for (vector<string> &media:db->GetTvMedia(movieID, (eMediaType) type, true) ) if (media.size() == 2 ) {
      int season = atoi(media[1].c_str() );
      if (season < 0) {
        DownloadFile((abs(type)==1?posterUrl:backdropUrl).str(), media[0], (type > 0)?(destDir.str()):destDirCollections, type > 0?movieID:season * -1, abs(type)==1?"_poster.jpg":"_backdrop.jpg", true);
        break;
      }
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


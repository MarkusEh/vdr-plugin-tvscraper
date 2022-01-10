#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <jansson.h>
#include "themoviedbscraper.h"

using namespace std;

cMovieDBScraper::cMovieDBScraper(string baseDir, cTVScraperDB *db, string language, cOverRides *overrides) {
    apiKey = "abb01b5a277b9c2c60ec0302d83c5ee9";
    this->language = language;
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
  cMovieDbMovie movie(db, this, NULL, NULL);
  movie.SetID(movieID);
  StoreMovie(movie);
  runtime = db->GetMovieRuntime(movieID);
  if (runtime == -1) return 0; // no runtime available in themoviedb
  return runtime;
}

void cMovieDBScraper::StoreMovie(cMovieDbMovie &movie) {
// if movie does not exist in DB, download movie and store it in DB
      int movieID = movie.ID();
      if (db->MovieExists(movieID)) {
        stringstream destDir;
        destDir << baseDir << "/" << movieID << "_poster.jpg";
        if(FileExists(destDir.str())) return;
      }
      if (config.enableDebug) esyslog("tvscraper: movie \"%i\" does not jet exist in db", movieID);
      if(!movie.ReadMovie()) {
        if (config.enableDebug) esyslog("tvscraper: error reading movie \"%i\" ", movieID);
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
    string configJSON;
    if (CurlGetUrl(url.str().c_str(), &configJSON)) {
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
    string actorsJSON;
    cMovieDbActors *actors = NULL;
    if (CurlGetUrl(url.str().c_str(), &actorsJSON)) {
        actors = new cMovieDbActors(actorsJSON);
        actors->ParseJSON();
    }
    return actors;
}

void cMovieDBScraper::StoreMedia(cMovieDbMovie *movie, cMovieDbActors *actors) {
    stringstream posterUrl;
    posterUrl << imageUrl << posterSize;
    stringstream backdropUrl;
    backdropUrl << imageUrl << backdropSize;
    stringstream destDir;
    destDir << baseDir << "/";
    movie->StoreMedia(posterUrl.str(), backdropUrl.str(), destDir.str());
    stringstream actorsUrl;
    actorsUrl << imageUrl << actorthumbSize;
    stringstream actorsDestDir;
    actorsDestDir << baseDir << "/actors";
    CreateDirectory(actorsDestDir.str());
    actors->Store(actorsUrl.str(), actorsDestDir.str());
}


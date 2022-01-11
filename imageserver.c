#include "imageserver.h"

using namespace std;

cImageServer::cImageServer(cTVScraperDB *db) {
    this->db = db;
}

cImageServer::~cImageServer() {
}

scrapType cImageServer::GetIDs(const cEvent *event, const cRecording *recording, int &movie_tv_id, int &season_number, int &episode_number) {
    if(db->GetMovieTvID(event, recording, movie_tv_id, season_number, episode_number)) {
       if(season_number == -100) return scrapMovie;
       else return scrapSeries;
    }
    movie_tv_id = 0;
    return scrapNone;
}

cTvMedia cImageServer::GetPosterOrBanner(int id, int season_number, int episode_number, scrapType type) {
// for tv, use backdrop
    cTvMedia media;
    media.path = "";
    media.width = 0;
    media.height = 0;
    if (type == scrapSeries && id < 0) {
        stringstream path;
        path << config.GetBaseDir() << "/series/" << id *(-1)<< "/banner.jpg";
        media.path = path.str();
        if (!FileExists(media.path)) {
          media.path = "";
        } else {
          media.width = 758;
          media.height = 140;
        }
        return media;
    }
    if (type == scrapSeries) {
      if (GetTvPoster(media, id, season_number) ) return media;
      if (GetTvBackdrop(media, id, season_number) ) return media;
      if (GetTvPoster(media, id) ) return media;
      if (GetTvBackdrop(media, id) ) return media;
      if (GetTvStill(media, id, season_number, episode_number) ) return media;
      media.path = "";
      media.width = 0;
      media.height = 0;
      return media;
    } else if (type == scrapMovie) {
        stringstream path;
        path << config.GetBaseDir() << "/movies/" << id << "_poster.jpg";
        media.path = path.str();
        if (FileExists(media.path)) {
          media.width = 500;
          media.height = 750;
        } else media.path = "";
    }
    return media;
}

cTvMedia cImageServer::GetPoster(int id, int season_number, int episode_number) {
    cTvMedia media;
    media.path = "";
    media.width = 0;
    media.height = 0;
    if(id == 0) return media;
    if(id <  0) {
      stringstream path0;
      path0 << config.GetBaseDir() << "/series/" << id * (-1);
      if (season_number != 0 || episode_number != 0) {
// check: is there a season poster?
        stringstream pathS;
        pathS << path0.str() << "/season_poster_" << season_number << ".jpg";
        string seasonPoster = pathS.str();
//        if (config.enableDebug) esyslog("tvscraper: cImageServer::GetPoster season, path \"%s\" ", seasonPoster.c_str());
        if (FileExists(seasonPoster)) {
          media.path = seasonPoster;
          media.width = 680;
          media.height = 1000;
          return media;
        }
      }
// no season poster, or no season information
//    path  << config.GetBaseDir() << "/series/" << id * (-1) << "/poster_0.jpg";
      path0 << "/poster_0.jpg";
      string filePoster = path0.str();
//      if (config.enableDebug) esyslog("tvscraper: cImageServer::GetPoster, path \"%s\" ", filePoster.c_str());

      if (FileExists(filePoster)) {
        media.path = filePoster;
        media.width = 680;
        media.height = 1000;
      }
      return media;
    }

    if (season_number == -100) {
// movie
      stringstream path;
      path << config.GetBaseDir() << "/movies/" << id << "_poster.jpg";
      string filePoster = path.str();
      if (FileExists(filePoster)) {
          media.path = path.str();
          media.width = 500;
          media.height = 750;
      }
      return media;
    }
// tv
    if ((season_number != 0 || episode_number != 0) && GetTvPoster(media, id, season_number) ) return media;
    if (GetTvPoster(media, id) ) return media;
    media.path = "";
    media.width = 0;
    media.height = 0;
    return media;
}

bool cImageServer::GetBanner(cTvMedia &media, int id) {
// only for tv/series
    stringstream path;
    if(id < 0) {
      path << config.GetBaseDir() << "/series/" << id * (-1) << "/banner.jpg";
      string fileBanner = path.str();
      if (FileExists(fileBanner)) {
        media.path = fileBanner;
        media.width = 758;
        media.height = 140;
        return true;
      }
      stringstream path2;
      path2 << config.GetBaseDir() << "/series/" << id * (-1) << "/fanart_0.jpg";
      fileBanner = path2.str();
      if (FileExists(fileBanner)) {
        media.path = fileBanner;
        media.width = 758;
        media.height = 140;
        return true;
      }
      return false;
    }
// tv
    if (GetTvBackdrop(media, id) ) return true;
    return false;
}

vector<cTvMedia> cImageServer::GetPosters(int id, scrapType type) {
    vector<cTvMedia> posters;
    if(id < 0 && type == scrapSeries) {
        for (int i=0; i<3; i++) {
            stringstream path;
            path << config.GetBaseDir() << "/series/" << id * (-1) << "/poster_" << i << ".jpg";
            string filePoster = path.str();
            if (FileExists(filePoster)) {
                cTvMedia media;
                media.path = filePoster;
                media.width = 680;
                media.height = 1000;
                posters.push_back(media);
            } else
                break;
        }
        return posters;
    }
    if (type == scrapSeries) {
      cTvMedia media;
      if (GetTvPoster(media, id) ) posters.push_back(media);
    } else if (type == scrapMovie) {
        stringstream path;
        path << config.GetBaseDir() << "/movies/" << id << "_poster.jpg";
        string filePoster = path.str();
        if (FileExists(filePoster)) {
            cTvMedia media;
            media.path = path.str();
            media.width = 500;
            media.height = 750;
            posters.push_back(media);
        }
    }
    return posters;
}

vector<cTvMedia> cImageServer::GetSeriesFanarts(int id, int season_number, int episode_number) {
    vector<cTvMedia> fanart;
    if(id < 0) {
      for (int i=0; i<3; i++) {
        stringstream path;
        path << config.GetBaseDir() << "/series/" << id * (-1) << "/fanart_" << i << ".jpg";
        string fileFanart = path.str();
        if (FileExists(fileFanart)) {
            cTvMedia media;
            media.path = fileFanart;
            media.width = 1920;
            media.height = 1080;
            fanart.push_back(media);
        } else
            break;
      }
      return fanart;
    }
    cTvMedia media;
    if (GetTvBackdrop(media, id, season_number) ) fanart.push_back(media);
    if (GetTvBackdrop(media, id) ) fanart.push_back(media);
    return fanart;
}

cTvMedia cImageServer::GetMovieFanart(int id) {
    cTvMedia fanart;
    stringstream path;
    path << config.GetBaseDir() << "/movies/" << id << "_backdrop.jpg";
    string fileFanart = path.str();
    if (FileExists(fileFanart)) {
        fanart.path = fileFanart;
        fanart.width = 1280;
        fanart.height = 720;
    }
    return fanart;
}


cTvMedia cImageServer::GetCollectionPoster(int id) {
    cTvMedia poster;
    stringstream path;
    path << config.GetBaseDir() << "/movies/collections/" << id << "_poster.jpg";
    string filePoster = path.str();
    if (FileExists(filePoster)) {
        poster.path = filePoster;
        poster.width = 500;
        poster.height = 750;
    }
    return poster;
}

cTvMedia cImageServer::GetCollectionFanart(int id) {
    cTvMedia fanart;
    stringstream path;
    path << config.GetBaseDir() << "/movies/collections/" << id << "_backdrop.jpg";
    string fileFanart = path.str();
    if (FileExists(fileFanart)) {
        fanart.path = fileFanart;
        fanart.width = 1280;
        fanart.height = 720;
    }
    return fanart;
}


vector<cActor> cImageServer::GetActors(int id, int episodeID, scrapType type) {
    vector<cActor> actors;
    if (type == scrapNone) return actors;
    vector<vector<string> > actorsDB;
    if (type == scrapSeries && id < 0) {
        actorsDB = db->GetActorsSeries(id * (-1));
        int numActors = actorsDB.size();
        for (int i=0; i < numActors; i++) {
            vector<string> row = actorsDB[i];
            if (row.size() == 3) {
                cActor actor;
                actor.name = row[0];
                actor.role = row[1];
                cTvMedia thumb;
                stringstream thumbPath;
                thumbPath << config.GetBaseDir() << "/series/" << id * (-1) << "/" << row[2];
                thumb.path = thumbPath.str();
                thumb.width = 300;
                thumb.height = 450;
                actor.actorThumb = thumb;
                actors.push_back(actor);
            }
        }
      return actors;
    }
    if (type == scrapSeries) {
        actorsDB = db->GetActorsTv(id);
        if(episodeID) {
          vector<vector<string> > actorsDB_Guest = db->GetGuestActorsTv(episodeID);
          actorsDB.insert(actorsDB.end(), actorsDB_Guest.begin(), actorsDB_Guest.end());
        }

    } else if (type == scrapMovie) {
        actorsDB = db->GetActorsMovie(id);
    }
        int numActors = actorsDB.size();
        for (int i=0; i < numActors; i++) {
            vector<string> row = actorsDB[i];
            if (row.size() == 3) {
                cActor actor;
                actor.name = row[1];
                actor.role = row[2];
                stringstream thumbPath;
                thumbPath << config.GetBaseDir() << "/movies/actors/actor_" << row[0] << ".jpg";
                cTvMedia thumb;
                thumb.path = thumbPath.str();
                thumb.width = 421;
                thumb.height = 632;
                actor.actorThumb = thumb;
                actors.push_back(actor);
            }
       }
    return actors;
}

string cImageServer::GetDescription(int id, int season_number, int episode_number, scrapType type) {
    string description;
    if (type == scrapSeries) {
        description = db->GetDescriptionTv(id, season_number, episode_number);
        if(description.empty() ) description = db->GetDescriptionTv(id);
    } else if (type == scrapMovie) {
        description = db->GetDescriptionMovie(id);
    }
    return description;
}
cTvMedia cImageServer::GetStill(int id, int season_number, int episode_number){
  cTvMedia media;
  if (GetTvStill(media, id, season_number, episode_number) ) return media;
  cTvMedia mediaE;
  return mediaE;
}

bool cImageServer::GetTvStill (cTvMedia &media, int id, int season_number, int episode_number) {
      stringstream path;
      if (id > 0) path << config.GetBaseDir() << "/movies/tv/" << id << "/" << season_number << "/still_" << episode_number << ".jpg";
             else path << config.GetBaseDir() << "/series/" << id *(-1) << "/" << season_number << "/still_" << episode_number << ".jpg";
      media.path = path.str();
      if (FileExists(media.path)) {
          media.width = 300;
          media.height = 200; // actually, values are 165, 169, 225, and 229
      return true;
      }
      media.path = "";
      return false;
}

bool cImageServer::GetTvBackdrop(cTvMedia &media, int id, int season_number) {
  media.width = 1280;
  media.height = 720;
  return GetTvPosterOrBackdrop(media.path, id, season_number, "backdrop.jpg");
}

bool cImageServer::GetTvBackdrop(cTvMedia &media, int id) {
  media.width = 1280;
  media.height = 720;
  return GetTvPosterOrBackdrop(media.path, id, "backdrop.jpg");
}

bool cImageServer::GetTvPoster(cTvMedia &media, int id) {
  media.width = 780;
  media.height = 1108; // guess
  return GetTvPosterOrBackdrop(media.path, id, "poster.jpg");
}

bool cImageServer::GetTvPoster(cTvMedia &media, int id, int season_number) {
  media.width = 780;
  media.height = 1108; // guess
  return GetTvPosterOrBackdrop(media.path, id, season_number, "poster.jpg");
}

bool cImageServer::GetTvPosterOrBackdrop(string &filePath, int id, int season_number, const char *filename){
      stringstream path;
      path << config.GetBaseDir() << "/movies/tv/" << id << "/" << season_number << "/" << filename;
      filePath = path.str();
      return FileExists(filePath);
}
bool cImageServer::GetTvPosterOrBackdrop(string &filePath, int id, const char *filename){
      stringstream path;
      path << config.GetBaseDir() << "/movies/tv/" << id << "/" << filename;
      filePath = path.str();
      return FileExists(filePath);
}

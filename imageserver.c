#include "imageserver.h"

using namespace std;

cImageServer::cImageServer(cTVScraperDB *db) {
    this->db = db;
}

cImageServer::~cImageServer() {
}

scrapType cImageServer::GetIDs(csEventOrRecording *sEventOrRecording, int &movie_tv_id, int &season_number, int &episode_number) {
    if(db->GetMovieTvID(sEventOrRecording, movie_tv_id, season_number, episode_number)) {
       if(season_number == -100) return scrapMovie;
       else return scrapSeries;
    }
    movie_tv_id = 0;
    return scrapNone;
}

cTvMedia cImageServer::GetPosterOrBanner(int id, int season_number, int episode_number, scrapType type) {
// for tv, use backdrop
    cTvMedia media;
    if (type == scrapSeries && id < 0) {
        stringstream path;
        path << config.GetBaseDir() << "/series/" << id *(-1)<< "/banner.jpg";
        media.setIfExists(path.str(), 758, 140);
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
        if (media.setIfExists(path.str(), 500, 750) ) return media;
    }
    return media;
}

cTvMedia cImageServer::GetPoster(int id, int season_number, int episode_number) {
    cTvMedia media;
    if(id == 0) return media;
    if(id <  0) {
      media.width = 680;
      media.height = 1000;
      stringstream path0;
      path0 << config.GetBaseDir() << "/series/" << id * (-1);
      if (season_number != 0 || episode_number != 0) {
// check: is there a season poster?
        stringstream pathS;
        pathS << path0.str() << "/season_poster_" << season_number << ".jpg";
        if (media.setIfExists(pathS.str(), 680, 1000) ) return media;
      }
// no season poster, or no season information
//    path  << config.GetBaseDir() << "/series/" << id * (-1) << "/poster_0.jpg";

      if (media.setIfExists(path0.str() + "/poster_0.jpg", 680, 1000) ) return media;
      if (media.setIfExists(path0.str() + "/poster.jpg", 680, 1000) ) return media;
      const std::filesystem::path fs_path(path0.str() );
      if (std::filesystem::exists(fs_path) ) {
      for (auto const& dir_entry : std::filesystem::directory_iterator{fs_path}) {
        if (dir_entry.path().filename().string().find("season_poster_") != std::string::npos) {
          if (media.setIfExists(dir_entry.path(), 680, 1000) ) return media;
        }
      }
      } else esyslog("tvscraper:imageserver ERROR dir %s does not exist", path0.str().c_str() );
// no season poster (for this season), and no "normal" poster
      media.width = 0;
      media.height = 0;
      return media;
    }

    if (season_number == -100) {
// movie
      stringstream path;
      path << config.GetBaseDir() << "/movies/" << id << "_poster.jpg";
      if (media.setIfExists(path.str(), 500, 750) ) return media;
      int collection_id = db->GetMovieCollectionID(id);
      if (collection_id) media = GetCollectionPoster(collection_id);
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
      if (media.setIfExists(path.str(), 758, 140) ) return true;
      stringstream path2;
      path2 << config.GetBaseDir() << "/series/" << id * (-1) << "/fanart_0.jpg";
      if (media.setIfExists(path2.str(), 758, 140) ) return true;
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
            cTvMedia media;
            if (media.setIfExists(path.str(), 680, 1000) ) posters.push_back(media);
              else break;
        }
        return posters;
    }
    if (type == scrapSeries) {
      cTvMedia media;
      if (GetTvPoster(media, id) ) posters.push_back(media);
    } else if (type == scrapMovie) {
        stringstream path;
        path << config.GetBaseDir() << "/movies/" << id << "_poster.jpg";
        cTvMedia media;
        if (media.setIfExists(path.str(), 500, 750) ) posters.push_back(media);
    }
    return posters;
}

vector<cTvMedia> cImageServer::GetSeriesFanarts(int id, int season_number, int episode_number) {
    vector<cTvMedia> fanart;
    if(id < 0) {
      for (int i=0; i<3; i++) {
        stringstream path;
        path << config.GetBaseDir() << "/series/" << id * (-1) << "/fanart_" << i << ".jpg";
        cTvMedia media;
        if (media.setIfExists(path.str(), 1920, 1080) ) fanart.push_back(media);
          else break;
      }
      return fanart;
    }
    cTvMedia media;
    if (GetTvBackdrop(media, id, season_number) ) fanart.push_back(media);
    if (GetTvBackdrop(media, id) ) fanart.push_back(media);
    return fanart;
}

cTvMedia cImageServer::GetMovieFanart(int id) {
    cTvMedia media;
    stringstream path;
    path << config.GetBaseDir() << "/movies/" << id << "_backdrop.jpg";
    media.setIfExists(path.str(), 1280, 720);
    return media;
}


cTvMedia cImageServer::GetCollectionPoster(int id) {
    cTvMedia poster;
    stringstream path;
    path << config.GetBaseDir() << "/movies/collections/" << id << "_poster.jpg";
    poster.setIfExists(path.str(), 500, 750);
    return poster;
}

cTvMedia cImageServer::GetCollectionFanart(int id) {
    cTvMedia fanart;
    stringstream path;
    path << config.GetBaseDir() << "/movies/collections/" << id << "_backdrop.jpg";
    fanart.setIfExists(path.str(), 1280, 720);
    return fanart;
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
      return media.setIfExists(path.str(), 300, 200);
//          media.height = 200; // actually, values are 165, 169, 225, and 229
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

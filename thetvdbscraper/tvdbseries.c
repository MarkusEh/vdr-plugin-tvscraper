#include <string>
#include <sstream>
#include <vector>
#include "tvdbseries.h"
#include "thetvdbscraper.h"

using namespace std;

cTVDBSeries::cTVDBSeries(cTVScraperDB *db, cTVDBScraper *TVDBScraper, int seriesID):
  m_db(db),
  m_TVDBScraper(TVDBScraper),
  m_seriesID(seriesID)
{
// this IS a series, and a series has an ID
  if (seriesID == 0) esyslog("tvscraper: ERROR cTVDBSeries::cTVDBSeries, seriesID == 0");
}

cTVDBSeries::~cTVDBSeries() {
}

std::map<int,int> ParseJson_Seasons(json_t *jSeasons) {
  std::map<int,int> result;
  if (!json_is_array(jSeasons)) return result;
  size_t index;
  json_t *jSeason;
  json_array_foreach(jSeasons, index, jSeason) {
    result[json_integer_value_validated(jSeason, "id")] = json_integer_value_validated(jSeason, "number");
  }
  return result;
}

std::string GetLang3(json_t *jSeriesData, const std::string &lang) {
  if (!jSeriesData) return "eng";
  json_t *jNameTranslations = json_object_get(jSeriesData, "nameTranslations");
  if (!json_is_array(jNameTranslations)) return json_string_value_validated(jSeriesData, "originalLanguage");
  bool eng = false;
  size_t index;
  json_t *jLang3;
  json_array_foreach(jNameTranslations, index, jLang3) {
    const char *lang3 = json_string_value(jLang3);
    if (!lang3 || !*lang3) continue;
    if ( lang3[0] == lang[0] && lang3[1] == lang[1]) return lang3;
    if ( lang3[0] == 'e' && lang3[1] == 'n') eng = true;
  }
  if (eng) return "eng";  // backup for desired language lang
  return json_string_value_validated(jSeriesData, "originalLanguage");
}

std::string translation(json_t *jTranslations, const char *arrayAttributeName, const char *textAttributeName, const std::string &lang) {
  if (lang.length() < 2) return "";
//json_t *jTranslationsArray = json_array_validated(jTranslations, arrayAttributeName, "ParseJson_Series / tvdbseries.c");
  json_t *jTranslationsArray = json_array_validated(jTranslations, arrayAttributeName, ""); // no error here, can be empty
  if (!jTranslationsArray) return "";
  size_t index;
  json_t *jTranslation;
  json_array_foreach(jTranslationsArray, index, jTranslation) {
    const char *language = json_string_value_validated_c(jTranslation, "language");
    if (!language || !*language) continue;
    if (language[0] == lang[0] && language[1] == lang[1]) return json_string_value_validated(jTranslation, textAttributeName);
  }
  return "";
}

bool cTVDBSeries::ParseJson_all(json_t *jData) {
  if (jData == NULL) return false;
  if (!json_is_object(jData)) {
    esyslog("tvscraper: ERROR cTVDBSeries::ParseJson_all, jData is not an object");
    return false;
  }
  episodeRunTimes.clear();
  if (!ParseJson_Series(json_object_get(jData, "series"))) {
    esyslog("tvscraper: ERROR cTVDBSeries::ParseJson_all, !ParseJson_Series");
    return false;
  }
  return true;
}

bool cTVDBSeries::ParseJson_Series(json_t *jSeries) {
// curl -X 'GET' 'https://api4.thetvdb.com/v4/series/73893/extended?meta=translations&short=false' -H 'accept: application/json' -H 'Authorization: Bearer ' > 11_extended.json
// jSeries must be the "data" of the response

// no episodes (later, with language)
// includes translations -> nameTranslations and translations -> overviewTranslations
// includes characters: Actors, guest stars, director, writer, ... for all episodes ... 846 Characters ... (not here, first episodes)
// ignore Actors here. Actors (including images) are taken from the <Actors> section
// includes artworks, also for seasons (seasonId)

// we also have a poster only (ParseJson_Artwork will add more ...)
  if (!jSeries) return false;
  if (m_seriesID != json_integer_value_validated(jSeries, "id")) {
    esyslog("tvscraper: ERROR cTVDBSeries::ParseJson_Series, m_seriesID = %i != series id in json %i", m_seriesID, json_integer_value_validated(jSeries, "id") );
    return false;
  }
  originalName = json_string_value_validated(jSeries, "name");
  poster = json_string_value_validated(jSeries, "image"); // other images will be added in ParseJson_Artwork
  firstAired = json_string_value_validated(jSeries, "firstAired");
  popularity = json_integer_value_validated(jSeries, "score");
  if (popularity == 0) popularity = 1;  // 0 indicates that no score is available in internal db, and will result in access to external db. 
// Status
  status = "";
  json_t *jStatus = json_object_get(jSeries, "status");
  if (json_is_object(jStatus)) status = json_string_value_validated(jStatus, "name"); // e.g. Ended
// Networks
  networks = "";
  json_t *jOriginalNetwork = json_object_get(jSeries, "originalNetwork");
  if (json_is_object(jOriginalNetwork)) networks = json_string_value_validated(jOriginalNetwork, "name");
  genres = json_concatenate_array(jSeries, "genres", "name");
// IMDB_ID
  IMDB_ID = "";
  json_t *jRemoteIds = json_object_get(jSeries, "remoteIds");
  if (json_is_array(jRemoteIds)) {
    size_t index;
    json_t *jRemoteId;
    json_array_foreach(jRemoteIds, index, jRemoteId) {
      if (json_integer_value_validated(jRemoteId, "type") == 2) IMDB_ID = json_string_value_validated(jRemoteId, "id");
      if (!IMDB_ID.empty() ) break;
    }
  }
// translations (name, overview)
  json_t *jTranslations = json_object_validated(jSeries, "translations", "cTVDBSeries::ParseJson_Series");
  if (jTranslations) {
    name = translation(jTranslations, "nameTranslations", "name", m_TVDBScraper->language);
    overview = translation(jTranslations, "overviewTranslations", "overview", m_TVDBScraper->language);
  }
// defaultSeasonType
/*
  int defaultSeasonType = json_integer_value_validated(jSeries, "defaultSeasonType");
  string defaultSeasonTypeS = "";
  json_t *jSeasonTypes = json_object_get(jSeries, "seasonTypes");
  if (json_is_array(jSeasonTypes)) {
    size_t index;
    json_t *jSeasonType;
    json_array_foreach(jSeasonTypes, index, jSeasonType) {
      if (json_integer_value_validated(jSeasonType, "id") == defaultSeasonType) defaultSeasonTypeS = json_string_value_validated(jSeasonType, "type");
      if (!defaultSeasonTypeS.empty() ) break;
    }
  }
*/
  return true;
}

bool cTVDBSeries::ParseJson_Episode(json_t *jEpisode) {
// read data from json, for one episode, and write this data to db
  if (m_seriesID == 0) return false;  // seriesID must be set, before calling
// seasonType == 1 -> "Aired Order", "official"
// seasonType == 2 -> "DVD Order", "dvd"
// seasonType == 3 -> "Absolute Order", "absolute"
  if (!jEpisode) return false;
//read the episode
  int episodeID = json_integer_value_validated(jEpisode, "id");
  if (episodeID == 0) return false;
  string episodeName = json_string_value_validated(jEpisode, "name");
  if (episodeName.empty() ) return false;
  int seasonNumber = json_integer_value_validated(jEpisode, "seasonNumber");
  int episodeNumber = json_integer_value_validated(jEpisode, "number");
//episodeAbsoluteNumber = atoi((const char *)node_content);
  string episodeOverview = json_string_value_validated(jEpisode, "overview");
  string episodeFirstAired = json_string_value_validated(jEpisode, "aired");
  string episodeFilename = json_string_value_validated(jEpisode, "image");
  int episodeRunTime = json_integer_value_validated(jEpisode, "runtime");
  episodeRunTimes.insert(episodeRunTime);

  int episodeAbsoluteNumber = 0; // not available
  string episodeGuestStars("");  // part of series, add later
  string episodeDirector("");    // part of series, add later
  string episodeWriter("");      // part of series, add later
  string episodeIMDB_ID("");     // not available
  float episodeRating = 0.0;     // not available
  int episodeVoteCount = 0;      // not available
  m_db->InsertTv_s_e(m_seriesID * (-1), seasonNumber, episodeNumber, episodeAbsoluteNumber, episodeID, episodeName, episodeFirstAired, episodeRating, episodeVoteCount, episodeOverview, episodeGuestStars, episodeDirector, episodeWriter, episodeIMDB_ID, episodeFilename, episodeRunTime);
  return true;
}
void cTVDBSeries::StoreDB() {
  m_db->InsertTv(m_seriesID * (-1), name, originalName, overview, firstAired, networks, genres, popularity, rating, ratingCount, poster, fanart, IMDB_ID, status, episodeRunTimes, "");
}

struct sImageScore {
  sImageScore():
    score(-1),
    image("") {}
  sImageScore(int score_i, const std::string &image_i):
    score(score_i),
    image(image_i) {}
  int score = -1;
  std::string image;
};
bool operator< (const sImageScore &first, const sImageScore &second) {
  return first.score > second.score;
}

bool cTVDBSeries::ParseJson_Artwork(json_t *jSeries) {
// return true if db was updated
  if (!jSeries) return false;
  map<int,int> seasonIdNumber = ParseJson_Seasons(json_object_get(jSeries, "seasons") );
  json_t *jArtworks = json_object_get(jSeries, "artworks");
  if (!json_is_array(jArtworks)) esyslog("tvscraper: no artworks found for series %i", m_seriesID);
  sImageScore bestBanner;
  sImageScore bestPoster;
// first int in the map is the season number. Only one poster per season
  map<int,sImageScore> bestSeasonPoster;
// 3 best "Background" / "Fanart"
  multiset<sImageScore> bestBackgrounds;
  if (json_is_array(jArtworks)) {
    size_t index;
    json_t *jArtwork;
    json_array_foreach(jArtworks, index, jArtwork) {
      if (!jArtwork) continue;
      int type = json_integer_value_validated(jArtwork, "type");
      if (type == 0) continue;
      int score = json_integer_value_validated(jArtwork, "score");
      string image = json_string_value_validated(jArtwork, "image");
      if (image.empty() ) continue;

      if (type == 1) {
    // "Banner" / "series"
    //    "width": 758,
    //    "height": 140,
        if (score <= bestBanner.score) continue;
        bestBanner.score = score;
        bestBanner.image = image;
        continue;
      }
      if (type == 2) {
    // "Poster" / "series" (not season specific)
    //    "width": 680,
    //    "height": 1000,
        if (score <= bestPoster.score) continue;
        bestPoster.score = score;
        bestPoster.image = image;
        continue;
      }
      if (type == 3) {
    // "Background" / "series"  ("Fanart", not season specific)
    //    "width": 1920,
    //    "height": 1080,
        bestBackgrounds.insert(sImageScore(score, image));
        continue;
      }
      if (type == 5) {
    // "Icon" / "series" (not season specific), Name of series, some symbols, poster format, mostly text
    //    "width": 1024,
    //    "height": 1024,
        continue; // ignore
      }
      if (type == 6) {
    // "Banner" / "season"(season specific, Clearart)
    //    "width": 758,
    //    "height": 140,
    //    int seasonId = json_integer_value_validated(jArtwork, "seasonId");
        continue; // ignore
      }
      if (type == 7) {
    // "Poster" / "season"/ can be DVD Cover (season specific), name of series, name of season
    // might also be clearart
    // bei tatort: auch speziefisch für kommisare / länder
    //    "width": 680,
    //    "height": 1000,
        int seasonId = json_integer_value_validated(jArtwork, "seasonId");
        if (seasonId == 0) continue;
        auto found = seasonIdNumber.find(seasonId);
        if (found == seasonIdNumber.end() ) continue;
        auto f = bestSeasonPoster.find(found->second); // found->second is the season number
        if (f == bestSeasonPoster.end() ) bestSeasonPoster.insert({found->second, sImageScore(score, image)});
        else {
          if (score <= f->second.score) continue;
          f->second.score = score;
          f->second.image = image;
        }
        continue;
      }
    // "type": 8
    // "Background" / "season"
    //    "width": 1920,
    //    "height": 1080,
    // "type": 10
    // "Icon" / "season"
    //    "width": 1024,
    //    "height": 1024,
    // "type": 11
    // "16:9 Screencap" / "episode"
    //    "width": 640,
    //    "height": 360,
    // "type": 12
    // " 4:3 Screencap" / "episode"
    //    "width": 640,
    //    "height": 480,
    // "type": 13
    // "Photo" / "actor"
    //    "width": 300,
    //    "height": 450,
    // "type": 22, -> clear art  (landscape format, picture + some text), with language
    // "ClearArt" / "series"
    //    "width": 1000,
    //    "height": 562,
    // "type": 23, -> clear logo (landscape format, only text), with language ->
    // "ClearLogo" / "series"
    //    "width": 800,
    //    "height": 310,

    }
  }
  if (bestBanner.score >= 0) banner = bestBanner.image;
  if ( !banner.empty() ) m_db->insertTvMedia (m_seriesID *-1, banner, mediaBanner);
  if (bestPoster.score >= 0) poster = bestPoster.image;
  if ( !poster.empty() ) m_db->insertTvMedia (m_seriesID *-1, poster, mediaPoster);
// Backgrounds / Fanart
  if (!bestBackgrounds.empty() ) fanart = bestBackgrounds.begin()->image;
  else if (!fanart.empty() ) bestBackgrounds.insert(sImageScore(10, fanart));
  int num = 1;
  for (const sImageScore &imageScore: bestBackgrounds) {
    if (config.enableDebug) esyslog("tvscraper: fanart number %i score %i image %s", num, imageScore.score, imageScore.image.c_str());
    m_db->insertTvMedia (m_seriesID *-1, imageScore.image, mediaFanart);
    if (++num > 3) break; // download up to 3 backgrounds
  }
// season poster
  for (const auto &sPoster: bestSeasonPoster)
    m_db->insertTvMediaSeasonPoster (m_seriesID *-1, sPoster.second.image, mediaSeason, sPoster.first);
  return true;
}

bool cTVDBSeries::ParseJson_Character(json_t *jCharacter) {
// return true if db was updated
  if (!jCharacter) return false;
  int type = json_integer_value_validated(jCharacter, "type");
  if (type == 0) return false;
  string personName = json_string_value_validated(jCharacter, "personName");
  int episodeId = json_integer_value_validated(jCharacter, "episodeId");
  if (type == 1) {
// "Director"
    if (episodeId == 0) return false;
    m_db->execSql("UPDATE tv_s_e SET episode_director = ? WHERE episode_id = ?", "si", personName.c_str(), episodeId);
    return true;
  }
  if (type == 2) {
// "Writer"
    if (episodeId == 0) return false;
    m_db->execSql("UPDATE tv_s_e SET episode_writer = ? WHERE episode_id = ?", "si", personName.c_str(), episodeId);
    return true;
  }
  string name = json_string_value_validated(jCharacter, "name");
  if (type == 3) {
// "Actor"
    int series_id = json_integer_value_validated(jCharacter, "seriesId");
    string image = json_string_value_validated(jCharacter, "image");
    if (image.empty() ) image = json_string_value_validated(jCharacter, "personImgURL");
    m_db->InsertActor(series_id, personName, name, image);
    return true;
  }
  if (type == 4) {
// "Guest Star"    if (episodeId == 0) return false;
//    string image = json_string_value_validated(jCharacter, "personImgURL");
    string entry = personName;
    if (!name.empty() ) {
      entry.append(": ");
      entry.append(name);
    }
    string currentEntry = m_db->QueryString("select episode_guest_stars from tv_s_e where episode_id =?", "i", episodeId);
    if (currentEntry.find(entry) == string::npos) return false; // already in db
    if (currentEntry.empty() ) currentEntry = "|";
    currentEntry.append(entry);
    currentEntry.append("|");
    m_db->execSql("UPDATE tv_s_e SET episode_guest_stars = ? WHERE episode_id = ?", "si", currentEntry.c_str(), episodeId);
    return true;
  }
  return false;
}


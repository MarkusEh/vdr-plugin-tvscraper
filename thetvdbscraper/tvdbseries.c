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
  m_language = config.GetDefaultLanguage()->m_thetvdb; // note: m_language will be changed if the series is not translated to m_language
  if (m_language.length() != 3) 
    esyslog("tvscraper: ERROR cTVDBSeries::cTVDBSeries: strlen(m_language) != 3, m_language: %s", m_language.c_str() );
}

cTVDBSeries::~cTVDBSeries() {
}

std::map<int,int> ParseJson_Seasons(const rapidjson::Value &jSeries) {
  std::map<int,int> result;
  for (const rapidjson::Value &season: cJsonArrayIterator(jSeries, "seasons")) {
    result[getValueInt(season, "id")] = getValueInt(season, "number");
  }
  return result;
}

const char *cTVDBSeries::translation(const rapidjson::Value &jTranslations, const char *arrayAttributeName, const char *textAttributeName) {
  for (const rapidjson::Value &translation: cJsonArrayIterator(jTranslations, arrayAttributeName)) {
    const char *language = getValueCharS(translation, "language");
    if (language && strcmp(language, m_language.c_str() ) == 0) return getValueCharS(translation, textAttributeName);
  }
  return NULL;
}

bool cTVDBSeries::ParseJson_Series(const rapidjson::Value &jSeries, const cLanguage *displayLanguage) {
// curl -X 'GET' 'https://api4.thetvdb.com/v4/series/73893/extended?meta=translations&short=false' -H 'accept: application/json' -H 'Authorization: Bearer ' > 11_extended.json
// jSeries must be the "data" of the response

// no episodes (already done, with language)
// includes translations -> nameTranslations and translations -> overviewTranslations
// includes characters: Actors, guest stars, director, writer, ... for all episodes ... 846 Characters ... (not here, first episodes)
// ignore Actors here. Actors (including images) are taken from the <Actors> section
// includes artworks, also for seasons (seasonId)

// we also have a poster only (ParseJson_Artwork will add more ...)
  if (m_seriesID != getValueInt(jSeries, "id", 0, "cTVDBSeries::ParseJson_Series")) {
    esyslog("tvscraper: ERROR cTVDBSeries::ParseJson_Series, m_seriesID = %i != series id in json %i", m_seriesID, getValueInt(jSeries, "id") );
    return false;
  }
// any translations?
  translations = getArrayConcatenated(jSeries, "nameTranslations");
  popularity = getValueInt(jSeries, "score");
  if (popularity == 0) popularity = 1;  // 0 indicates that no score is available in internal db, and will result in access to external db. 
  m_db->exec("INSERT INTO tv_score (tv_id, tv_score, tv_languages, tv_languages_last_update) VALUES(?, ?, ?, ?) ON CONFLICT(tv_id) DO UPDATE SET tv_score = excluded.tv_score, tv_languages = excluded.tv_languages, tv_languages_last_update = excluded.tv_languages_last_update", -m_seriesID, popularity, translations, time(0) );
  m_language = displayLanguage->m_thetvdb;

  originalName = getValueCharS(jSeries, "name");
  poster = getValueCharS(jSeries, "image"); // other images will be added in ParseJson_Artwork
  firstAired = getValueCharS(jSeries, "firstAired");
// Status
  status = getValueCharS2(jSeries, "status", "name"); // e.g. Ended
// Networks
  networks = getValueCharS2(jSeries, "originalNetwork", "name"); // e.g. Ended
  genres = getValueArrayConcatenated(jSeries, "genres", "name");
// IMDB_ID
  IMDB_ID = NULL;
  int themoviedb_id = 0;
  for (const rapidjson::Value &remoteid: cJsonArrayIterator(jSeries, "remoteIds")) {
    int type = getValueInt(remoteid, "type");
    if (type == 2) IMDB_ID = getValueCharS(remoteid, "id");
    else if (type == 12 && !themoviedb_id) themoviedb_id = parse_int<int>(getValueCharS(remoteid, "id"));
    if (IMDB_ID && *IMDB_ID && themoviedb_id) break;
  }
  if (themoviedb_id) m_db->setEqual(themoviedb_id, -m_seriesID);
// translations (name, overview)
  rapidjson::Value::ConstMemberIterator translations_it = getTag(jSeries, "translations", "cTVDBSeries::ParseJson_Series", nullptr);
  if (translations_it != jSeries.MemberEnd() && translations_it->value.IsObject() ) {
    name = translation(translations_it->value, "nameTranslations", "name");
    overview = translation(translations_it->value, "overviewTranslations", "overview");
  }
  if (!name || !*name) {
    name = originalName; // translation in desired language is not available, use name in original language
// we already checked that the name translation is available, seems to be an error
    esyslog("tvscraper, ERROR cTVDBSeries::ParseJson_Series, m_language %s, name %s, seriesID %i", m_language.c_str(), name, m_seriesID);
  }
  return true;
}

bool cTVDBSeries::ParseJson_Episode(const rapidjson::Value &jEpisode, cSql &insertEpisode2, cSql &insertRuntime, const cLanguage *lang, cSql &insertEpisodeLang) {
// write both, tv_s_e & tv_lang
// read data (episode name) from json, for one episode, and write this data to db with additional languages
  if (m_seriesID == 0) return 0;  // seriesID must be set, before calling
  int episodeID = getValueInt(jEpisode, "id");
  if (episodeID == 0) return false;
// tv_s_e
  int episodeRunTime = getValueInt(jEpisode, "runtime");
  if (episodeRunTime != 0) insertRuntime.resetBindStep(-m_seriesID, episodeRunTime);
  else episodeRunTime = -1; // -1: no data available in external db; 0: data in external db not requested

// 2: remove name & overview: here in language lang, which might not be the correct language
  insertEpisode2.resetBindStep(
     -m_seriesID, getValueInt(jEpisode, "seasonNumber"), getValueInt(jEpisode, "number"), episodeID,
     getValueCharS(jEpisode, "aired"), getValueCharS(jEpisode, "image"), episodeRunTime);

// tv_name
  const char *episodeName = getValueCharS(jEpisode, "name");
  if (!episodeName || !*episodeName) return false;
//    don't save normed strings in database.
//      doesn't help for performance (norming one string only takes 5% of sentence_distance time
//      makes changes to the norm algorithm almost impossible
  insertEpisodeLang.resetBindStep(episodeID, lang->m_id, episodeName);
  return true;
}


int cTVDBSeries::ParseJson_Episode(const rapidjson::Value &jEpisode, cSql &insertEpisode) {
// return episode ID
// read data from json, for one episode, and write this data to db
  if (m_seriesID == 0) return 0;  // seriesID must be set, before calling
// seasonType == 1 -> "Aired Order", "official"
// seasonType == 2 -> "DVD Order", "dvd"
// seasonType == 3 -> "Absolute Order", "absolute"
//read the episode
  int episodeID = getValueInt(jEpisode, "id");
  if (episodeID == 0) return 0;
  int episodeRunTime = getValueInt(jEpisode, "runtime");
  if (episodeRunTime != 0) episodeRunTimes.insert(episodeRunTime);
  else episodeRunTime = -1; // -1: no data available in external db; 0: data in external db not requested

  insertEpisode.resetBindStep(
     -m_seriesID, getValueInt(jEpisode, "seasonNumber"), getValueInt(jEpisode, "number"), episodeID,
     getValueCharS(jEpisode, "name"), getValueCharS(jEpisode, "aired"),
     getValueCharS(jEpisode, "overview"), getValueCharS(jEpisode, "image"), episodeRunTime);
/*
  int episodeAbsoluteNumber = 0; // not available
  string episodeGuestStars("");  // part of series, add later
  string episodeDirector("");    // part of series, add later
  string episodeWriter("");      // part of series, add later
  string episodeIMDB_ID("");     // not available
  float episodeRating = 0.0;     // not available
  int episodeVoteCount = 0;      // not available
*/
  return episodeID;
}

bool cTVDBSeries::ParseJson_Episode(const rapidjson::Value &jEpisode, const cLanguage *lang, cSql &insertEpisodeLang) {
// read data (episode name) from json, for one episode, and write this data to db with additional languages
  int episodeID = getValueInt(jEpisode, "id");
  const char *episodeName = getValueCharS(jEpisode, "name");
  if (episodeID == 0 || !episodeName || !*episodeName) return false;
//    don't save normed strings in database.
//      doesn't help for performance (norming one string only takes 5% of sentence_distance time
//      makes changes to the norm algorithm almost impossible
  insertEpisodeLang.resetBindStep(episodeID, lang->m_id, episodeName);
  return true;
}

void cTVDBSeries::StoreDB() {
  if (episodeRunTimes.empty() ) episodeRunTimes.insert(-1); // empty episodeRunTimes results in re-reading it from external db. And there is no data on external db ...
  m_db->InsertTv(-m_seriesID, name, originalName, overview, firstAired, networks, genres, popularity, rating, ratingCount, cTVDBScraper::getDbUrl(poster), cTVDBScraper::getDbUrl(fanart), IMDB_ID, status, episodeRunTimes, nullptr, translations.c_str() );
}

class cImageScore {
  friend bool operator< (const cImageScore &first, const cImageScore &second);
  public:
  cImageScore():
    m_image(nullptr) {}
  cImageScore(const char *image, int score=0, int languageMatch=0, int width=0, int height=0):
    m_image(image),
    m_score(score),
    m_languageMatch(languageMatch),
    m_res((width/32) * (height/32)) {}
  const char *m_image;
  private:
  int m_score; // e.g. "score": 100007, and similar scores
  int m_languageMatch;
  int m_res;
};
bool operator< (const cImageScore &first, const cImageScore &second) {
// here, operator< means better
// so after standard sort, best is on top
  if (!first.m_image) return false;
  if (!second.m_image) return true;
  if (first.m_languageMatch != second.m_languageMatch) {
    if (first.m_languageMatch == 0) return false;  //avoid wrong language;
    if (second.m_languageMatch == 0) return true;  //avoid wrong language;
  }
  if (first.m_res != second.m_res) return first.m_res > second.m_res;
  if (first.m_score != second.m_score) return first.m_score > second.m_score;
  return first.m_languageMatch > second.m_languageMatch;
}

bool cTVDBSeries::ParseJson_Artwork(const rapidjson::Value &jSeries, const cLanguage *displayLanguage) {
// return true if db was updated
  map<int,int> seasonIdNumber = ParseJson_Seasons(jSeries);

//  vector<cImageScore> bestBanners;
  cImageScore bestBanner;
  cImageScore bestPoster;
// first int in the map is the season number. Only one poster per season
  map<int,cImageScore> bestSeasonPoster;
// 3 best "Background" / "Fanart"
  multiset<cImageScore> bestBackgrounds;
  size_t displayLanguageLength = (displayLanguage && displayLanguage->m_thetvdb)?strlen(displayLanguage->m_thetvdb):0;
  for (const rapidjson::Value &jArtwork: cJsonArrayIterator(jSeries, "artworks")) {
    int type = getValueInt(jArtwork, "type");
    if (type == 0) continue;
    const char *image = getValueCharS(jArtwork, "image");
    if (!image || !*image) continue;
    int languageMatch = 1; // 1: artwork has no lang. 0: no match. 2: match
    const char *language = getValueCharS(jArtwork, "language");
    if (language && *language) {
      if (displayLanguageLength && strncmp(language, displayLanguage->m_thetvdb, displayLanguageLength) == 0) languageMatch = 2;
      else languageMatch = 0;
    }
    cImageScore currentImage(image,
      getValueInt(jArtwork, "score"),
      languageMatch,
      getValueInt(jArtwork, "width"),
      getValueInt(jArtwork, "hight"));

    if (type == 1) {
  // "Banner" / "series"
  //    "width": 758,
  //    "height": 140,
      if (currentImage < bestBanner) bestBanner = currentImage;
//      bestBanners.insert(currentImage);
      continue;
    }
    if (type == 2) {
  // "Poster" / "series" (not season specific)
  //    "width": 680,
  //    "height": 1000,
      if (currentImage < bestPoster) bestPoster = currentImage;
      continue;
    }
    if (type == 3) {
  // "Background" / "series"  ("Fanart", not season specific)
  //    "width": 1920,
  //    "height": 1080,
      bestBackgrounds.insert(currentImage);
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
  //    int seasonId = getValueInt(jArtwork, "seasonId");
      continue; // ignore
    }
    if (type == 7) {
  // "Poster" / "season"/ can be DVD Cover (season specific), name of series, name of season
  // might also be clearart
  // bei tatort: auch speziefisch für kommisare / länder
  //    "width": 680,
  //    "height": 1000,
      int seasonId = getValueInt(jArtwork, "seasonId");
      if (seasonId == 0) continue;
      auto found = seasonIdNumber.find(seasonId);
      if (found == seasonIdNumber.end() ) continue;
      auto f = bestSeasonPoster.find(found->second); // found->second is the season number
      if (f == bestSeasonPoster.end() ) bestSeasonPoster.insert({found->second, currentImage});
      else {
        if (currentImage < f->second) f->second = currentImage;
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
  if (bestBanner.m_image && *bestBanner.m_image)
    m_db->insertTvMedia (-m_seriesID, cTVDBScraper::getDbUrl(bestBanner.m_image), mediaBanner);
  if (bestPoster.m_image) poster = bestPoster.m_image;
  if (poster && *poster) m_db->insertTvMedia (-m_seriesID, cTVDBScraper::getDbUrl(poster), mediaPoster);
// Backgrounds / Fanart
  if (!bestBackgrounds.empty() ) fanart = bestBackgrounds.begin()->m_image;
  else if (fanart && *fanart) bestBackgrounds.insert(cImageScore(fanart));
  int num = 1;
  for (const cImageScore &imageScore: bestBackgrounds) {
//  if (config.enableDebug) esyslog("tvscraper: fanart number %i score %i image %s", num, imageScore.score, imageScore.image.c_str());
    m_db->insertTvMedia (-m_seriesID, cTVDBScraper::getDbUrl(imageScore.m_image), mediaFanart);
    if (++num > 3) break; // download up to 3 backgrounds
  }
// season poster (max. 1 poster per season)
  cSql stmt(m_db, "INSERT OR REPLACE INTO tv_media (tv_id, media_path, media_type, media_number) VALUES (?, ?, ?, ?);");
  for (const auto &sPoster: bestSeasonPoster) {
    const char *url = cTVDBScraper::getDbUrl(sPoster.second.m_image);
    if (url && *url) stmt.resetBindStep(-m_seriesID, url, mediaSeason, sPoster.first);
  }
  return true;
}

bool cTVDBSeries::ParseJson_Character(const rapidjson::Value &jCharacter) {
// return true if db was updated
  int type = getValueInt(jCharacter, "type");
  if (type == 0) return false;
  const char *personName = getValueCharS(jCharacter, "personName");
  int episodeId = getValueInt(jCharacter, "episodeId");
  if (type == 1) {
// "Director"
    if (episodeId == 0) return false;
    m_db->exec("UPDATE tv_s_e SET episode_director = ? WHERE episode_id = ?", personName, episodeId);
    return true;
  }
  if (type == 2) {
// "Writer"
    if (episodeId == 0) return false;
    m_db->exec("UPDATE tv_s_e SET episode_writer = ? WHERE episode_id = ?", personName, episodeId);
    return true;
  }
  const char *name = getValueCharS(jCharacter, "name");
  if (type == 3 || type == 10) {
// "Actor" (3), Host (10)
    int series_id = getValueInt(jCharacter, "seriesId");
    const char *image = getValueCharS(jCharacter, "image");
    if (!image || !*image) image = getValueCharS(jCharacter, "personImgURL");
    m_db->InsertActor(series_id, personName, name, image);
    return true;
  }
  if (type == 4) {
// "Guest Star"
    if (episodeId == 0) return false;
//  const char *image = getValueCharS(jCharacter, "personImgURL");
    const char *sqlu = "UPDATE tv_s_e SET episode_guest_stars = ? WHERE episode_id = ?";
    cSql getGuestStars(m_db, "SELECT episode_guest_stars FROM tv_s_e WHERE episode_id =?", episodeId);
    const char *guestStars;
    if (getGuestStars.readRow(guestStars) && guestStars && *guestStars) {
      if (!name || !*name) {
        if (strstr(guestStars, personName) ) return false; // already in db
        m_db->exec(sqlu, concatenate(guestStars, personName, "|"), episodeId);
      } else {
        std::string entry = concatenate(personName, ": ", name);
        if (strstr(guestStars, entry.c_str() ) ) return false; // already in db
        m_db->exec(sqlu, concatenate(guestStars, entry, "|"), episodeId);
      }
    } else {
// currently, no entry in db
      if (!name || !*name) m_db->exec(sqlu, concatenate("|", personName, "|"), episodeId);
      else m_db->exec(sqlu, concatenate("|", personName, ": ", name, "|"), episodeId);
    }
    return true;
  }
  return false;
}


#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "themoviedbscraper.h"

cMovieDBScraper::cMovieDBScraper(cTVScraperDB *db, cOverRides *overrides) {
  this->db = db;
  this->overrides = overrides;
}

cMovieDBScraper::~cMovieDBScraper() {
}

void cMovieDBScraper::StoreMovie(int movieID, bool forceUpdate) {
// if movie does not exist in DB, download movie and store it in DB
  if (!forceUpdate && db->MovieExists(movieID)) return;
  if (config.enableDebug && !forceUpdate) esyslog("tvscraper: movie \"%i\" does not yet exist in db", movieID);
  cMovieDbMovie movie(db, this);
  movie.ReadAndStore(movieID);
}
bool cMovieDBScraper::Connect(void) {
  cJsonDocumentFromUrl document;
  document.set_enableDebug(config.enableDebug);
  if (!document.download_and_parse(cToSvConcat(baseURL, "/configuration?api_key=", apiKey).c_str())) return false;
  return parseJSON(document);
}

bool cMovieDBScraper::parseJSON(const rapidjson::Value &root) {
// parse result of https://api.themoviedb.org/3/configuration
  const char *imageUrl = getValueCharS2(root, "images", "base_url", "cMovieDBScraper::parseJSON, image URL");
  if (!imageUrl) return false;
  m_posterBaseUrl = concatenate(imageUrl, posterSize);
  m_backdropBaseUrl = concatenate(imageUrl, backdropSize);
  m_stillBaseUrl = concatenate(imageUrl, stillSize);
  m_actorsBaseUrl = concatenate(imageUrl, actorthumbSize);

  return true;
}

void cMovieDBScraper::DownloadActors(int tvID, bool movie) {
  cSql stmtDo(db, "SELECT actor_id, actor_path FROM actor_download WHERE movie_id = ? and is_movie = ?");
  for (cSql &stmt: stmtDo.resetBindStep(tvID,  movie)) {
    const char *actor_path = stmt.getCharS(1);
    if (!actor_path || !*actor_path) continue;
    CONCATENATE(actorsFullUrl, m_actorsBaseUrl, actor_path);
    CONCATENATE(downloadFullPath, config.GetBaseDirMovieActors(), "actor_", stmt.getInt(0), ".jpg");
    DownloadImg(actorsFullUrl, downloadFullPath);
  }
  db->DeleteActorDownload (tvID, movie);
}

void cMovieDBScraper::DownloadMediaTv(int tvID) {
  cSql sql(db, "select media_path, media_number from tv_media where tv_id = ? and media_type = ? and media_number >= 0");
// if mediaType == season_poster, media_number is the season
  for (cSql &statement: sql.resetBindStep(tvID, (int)mediaSeason)) {
    std::string baseDirDownload = concatenate(config.GetBaseDirMovieTv(), tvID, "/");
    CreateDirectory(baseDirDownload);
    DownloadFile(m_posterBaseUrl, statement.getCharS(0), baseDirDownload, statement.getInt(1), "/poster.jpg", false);
    break;
  }
  cSql sql2(db, "select media_path from tv_media where tv_id = ? and media_type = ? and media_number >= 0");
  for (int type = 1; type <= 2; type++) {
    for (cSql &statement: sql2.resetBindStep(tvID, type)) {
      DownloadFile(m_posterBaseUrl, statement.getCharS(0), config.GetBaseDirMovieTv(), tvID, type==1?"/poster.jpg":"/backdrop.jpg", false);
      break;
    }
  }
  db->deleteTvMedia (tvID, false, true);
}
void cMovieDBScraper::DownloadMedia(int movieID) {
  cSql stmt0(db, "SELECT media_path, media_number FROM tv_media WHERE tv_id = ? AND media_type = ? AND media_number < 0");
  for (int type = -2; type <= 2; type ++) if (type !=0) {
    for (cSql &statement: stmt0.resetBindStep(movieID, type)) {
      int media_number = -1 * statement.getInt(1);
      bool isPoster = abs(type)==1;
      DownloadFile(isPoster?m_posterBaseUrl:m_backdropBaseUrl, statement.getCharS(0), (type > 0)?config.GetBaseDirMovies():config.GetBaseDirMovieCollections(), type > 0?movieID:media_number, isPoster?"_poster.jpg":"_backdrop.jpg", true);
      break;
    }
  }
  db->deleteTvMedia (movieID, true, true);
}
bool cMovieDBScraper::DownloadFile(const string &urlBase, const string &urlFileName, const string &destDir, int destID, const char * destFileName, bool movie) {
// download urlBase urlFileName to destDir destID destFileName
// for tv shows (movie == false), create the directory destDir destID
  if(urlFileName.empty() ) return false;
  std::string destFullPath;
  destFullPath.reserve(200);
  stringAppend(destFullPath, destDir, destID);
  if (!movie) CreateDirectory(destFullPath);
  destFullPath.append(destFileName);
  return DownloadImg(concatenate(urlBase, urlFileName), destFullPath);
}

void cMovieDBScraper::StoreStill(int tvID, int seasonNumber, int episodeNumber, const char *stillPathTvEpisode) {
  if (!stillPathTvEpisode || !*stillPathTvEpisode) return;
  std::string pathStill;
  pathStill.reserve(200);
  stringAppend(pathStill, config.GetBaseDirMovieTv(), tvID);
  CreateDirectory(pathStill);
  stringAppend(pathStill, "/", seasonNumber);
  CreateDirectory(pathStill);
  stringAppend(pathStill, "/still_", episodeNumber, ".jpg");
  DownloadImg(concatenate(m_stillBaseUrl, stillPathTvEpisode), pathStill);
}

bool cMovieDBScraper::AddTvResults(vector<searchResultTvMovie> &resultSet, cSv tvSearchString, const cCompareStrings &compareStrings, const cLanguage *lang) {
// search for tv series, add search results to resultSet
  if (tvSearchString.empty() ) {
    esyslog("tvscraper: ERROR cMovieDbTv::AddTvResults, tvSearchString == empty");
    return false;
  }
// concatenate URL
  std::string url; url.reserve(300);
  stringAppend(url, baseURL, "/search/tv?api_key=", apiKey);
  if (lang) stringAppend(url, "&language=", lang->m_themoviedb);
  stringAppend(url, "&query=");
  stringAppendCurlEscape(url, tvSearchString);

  cJsonDocumentFromUrl root;
// call api, get json
  apiCalls.start();
  root.set_enableDebug(config.enableDebug);
  bool success = root.download_and_parse(url.c_str());
  apiCalls.stop();
  if (!success) return false;
  if (getValueInt(root, "status_code", -1) == 50) {
    const char *status_message = getValueCharS(root, "status_message");
    if (status_message) {
      esyslog("tvscraper: cMovieDBScraper::AddTvResults, status_message = %s", status_message);
      return false;
    }
  }
  for (const rapidjson::Value &result: cJsonArrayIterator(root, "results")) {
    int id = getValueInt(result, "id");
    if (id == 0) continue;
    cNormedString normedOriginalName(removeLastPartWithP(getValueCharS(result, "original_name")));
    cNormedString normedName(removeLastPartWithP(getValueCharS(result, "name")));
    for (char delim: compareStrings) {
// distance == deviation from search text
//      sRes.normedName.reset(removeLastPartWithP(getValueCharS(result, "original_name")));
      int dist_a = compareStrings.minDistance(delim, normedOriginalName, 1000);
      if (lang) {
// search string is not in original language, reduce match to original_name somewhat
        dist_a = std::min(dist_a + 50, 1000);
        dist_a = compareStrings.minDistance(delim, normedName, dist_a);
      }
      dist_a = std::min(dist_a + 10, 1000); // avoid this, prefer TVDB

      auto sResIt = find_if(resultSet.begin(), resultSet.end(), [id,delim](const searchResultTvMovie& x) { return x.id() == id && x.delim() == delim;});
      if (sResIt != resultSet.end() ) {
        sResIt->setMatchTextMin(dist_a, lang?normedName:normedOriginalName);
      } else {
// create new result object sRes
        searchResultTvMovie sRes(id, false, getValueCharS(result, "first_air_date") );
        sRes.setPositionInExternalResult(resultSet.size() );
        sRes.setDelim(delim);
        sRes.m_normedName = lang?normedName:normedOriginalName;

        sRes.setMatchText(dist_a);
        sRes.setPopularity(getValueDouble(result, "popularity"), getValueDouble(result, "vote_average"), getValueInt(result, "vote_count") );
        resultSet.push_back(std::move(sRes));
      }
    }
  }
  return true;
}

void cMovieDBScraper::AddMovieResults(vector<searchResultTvMovie> &resultSet, cSv SearchString, const cCompareStrings &compareStrings, const char *description, bool setMinTextMatch, const cYears &years, const cLanguage *lang) {
  if (SearchString.empty() ) {
    esyslog("tvscraper: ERROR cMovieDbMovie::AddMovieResults, SearchString == empty");
    return;
  }
  std::string url = concatenate(baseURL, "/search/movie?api_key=", GetApiKey());
  if (lang) stringAppend(url, "&language=", lang->m_themoviedb);

  stringAppend(url, config.GetThemoviedbSearchOption(), "&query=");
  stringAppendCurlEscape(url, SearchString);
  size_t lenUrl0 = url.length();

  int num_pages = AddMovieResultsForUrl(url.c_str(), resultSet, compareStrings, description, setMinTextMatch, lang);
  bool found = false;
  if (num_pages > 3 && years.size() + 1 < num_pages) {
// several pages, restrict with years
    for (int year: years) {
      url.erase(lenUrl0);
      stringAppend(url, "&year=", (unsigned int)year);
      if (AddMovieResultsForUrl(url.c_str(), resultSet, compareStrings, description, setMinTextMatch, lang) > 0) found = true;
    }
  }
  if (!found) {
// no years avilable, or not found with the available year. Check all results in all pages
    if (num_pages > 10) num_pages = 10;
    for (int page = 2; page <= num_pages; page ++) {
      url.erase(lenUrl0);
      stringAppend(url, "&page=", (unsigned int)page);
      AddMovieResultsForUrl(url.c_str(), resultSet, compareStrings, description, setMinTextMatch, lang);
    }
  }
}

int cMovieDBScraper::AddMovieResultsForUrl(const char *url, vector<searchResultTvMovie> &resultSet, const cCompareStrings &compareStrings, const char *description, bool setMinTextMatch, const cLanguage *lang) {
// return 0 if no results where found (calling the URL shows no results). Otherwise number of pages
// add search results from URL to resultSet
  cJsonDocumentFromUrl root;
  root.set_enableDebug(config.enableDebug);
  apiCalls.start();
  bool success = root.download_and_parse(url);
  apiCalls.stop();
  if (!success) return 0;
  if (getValueInt(root, "status_code", -1) == 50) {
    const char *status_message = getValueCharS(root, "status_message");
    if (status_message) {
      esyslog("tvscraper: cMovieDBScraper::AddMovieResultsForUrl, status_message = %s", status_message);
      return 0;
    }
  }
  if (getValueInt(root, "total_results", 0, "cMovieDbMovie::AddMovieResultsForUrl", &root) == 0) return 0;
  AddMovieResults(root, resultSet, compareStrings, description, setMinTextMatch, lang);
  return getValueInt(root, "total_pages", 0, "cMovieDbMovie::AddMovieResultsForUrl", &root);
}
void cMovieDBScraper::AddMovieResults(const rapidjson::Document &root, vector<searchResultTvMovie> &resultSet, const cCompareStrings &compareStrings, const char *description, bool setMinTextMatch, const cLanguage *lang) {
  int res = -1;
  for (const rapidjson::Value &result: cJsonArrayIterator(root, "results")) {
    if (!result.IsObject() ) continue;
    ++res;
// id of result
    int id = getValueInt(result, "id", 0, "cMovieDbMovie::AddMovieResults, id");
    if (!id) continue;
// figure out "dist": text match of strings
// Title & OriginalTitle
    int distOrigTitle = compareStrings.minDistance(0, removeLastPartWithP(getValueCharS(result, "original_title")), 1000);
    if (lang) distOrigTitle = std::min(1000, distOrigTitle + 50);  // increase distance for original title, because it's a less likely match
    int dist = compareStrings.minDistance(0, removeLastPartWithP(getValueCharS(result, "title")), distOrigTitle);
    if (dist > 300 && description && strlen(description) > 25) {
      const char *overview = getValueCharS(result, "overview");
      if (overview && *overview) dist = cNormedString(description).sentence_distance(overview, dist);
    }
    if (setMinTextMatch && res == 0) dist = std::min(dist, 500); // api.themoviedb.org has some alias names, which are used in the search but not displayed. Set the text match to a "minimum" of 500 -> 0.5 , because it was found by api.themoviedb.org
    auto sResIt = find_if(resultSet.begin(), resultSet.end(), [id](const searchResultTvMovie& x) { return x.id() == id;});
    if (sResIt != resultSet.end() ) {
      sResIt->setMatchTextMin(dist);
    } else {

// a result was found and will be added to the list
      searchResultTvMovie sRes(id, true, getValueCharS(result, "release_date") );
      sRes.setPositionInExternalResult(resultSet.size() );
      sRes.setMatchText(dist);
      sRes.setPopularity(getValueDouble(result, "popularity"), getValueDouble(result, "vote_average"), getValueInt(result, "vote_count") );
      resultSet.push_back(std::move(sRes));
    }
  }
}


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

int cMovieDBScraper::GetMovieRuntime(int movieID) {
// return 0 if no runtime is available in themovied
  int runtime = db->GetMovieRuntime(movieID);
  if (runtime  >  0) return runtime;
  if (runtime == -1) return 0; // no runtime available in themoviedb
// themoviedb never checked for runtime, check now
  StoreMovie(movieID, true);
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

void cMovieDBScraper::StoreMovie(int movieID, bool forceUpdate) {
// if movie does not exist in DB, download movie and store it in DB
  if (!forceUpdate && db->MovieExists(movieID)) return;
  if (config.enableDebug && !forceUpdate) esyslog("tvscraper: movie \"%i\" does not yet exist in db", movieID);
  cMovieDbMovie movie(db, this);
  movie.ReadAndStore(movieID);
}
bool cMovieDBScraper::Connect(void) {
  rapidjson::Document document;
  cLargeString configJSON("cMovieDBScraper::Connect", 1500);
  if (!jsonCallRest(document, configJSON, concatenate(baseURL, "/configuration?api_key=", apiKey).c_str(), config.enableDebug)) return false;
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
    Download(actorsFullUrl, downloadFullPath);
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
  return Download(concatenate(urlBase, urlFileName), destFullPath);
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
  Download(concatenate(m_stillBaseUrl, stillPathTvEpisode), pathStill);
}
bool cMovieDBScraper::AddTvResults(vector<searchResultTvMovie> &resultSet, string_view tvSearchString, const cLanguage *lang) {
// search for tv series, add search results to resultSet
// return true if a result matching searchString was found, and no splitting of searchString was required
// otherwise, return false. Note: also in this case some results might have been added

  string_view searchString1, searchString2, searchString3, searchString4;
  std::vector<std::optional<cNormedString>> normedStrings = getNormedStrings(tvSearchString, searchString1, searchString2, searchString3, searchString4);
  cLargeString buffer("cMovieDbTv::AddTvResults", 10000);
  size_t size0 = resultSet.size();
  AddTvResults(buffer, resultSet, tvSearchString, normedStrings, lang);
  if (resultSet.size() > size0) {
    for (size_t i = size0; i < resultSet.size(); i++)
      if (normedStrings[0].value().sentence_distance(resultSet[i].normedName) < 600) return true;
    return false;
  }
  if (!searchString1.empty() ) AddTvResults(buffer, resultSet, searchString1, normedStrings, lang);
  if (resultSet.size() > size0) return false;
  if (!searchString2.empty() ) AddTvResults(buffer, resultSet, searchString2, normedStrings, lang);
  if (resultSet.size() > size0) return false;
  if (searchString4.empty() ) return false;
// searchString3 is the string before ' - '. We will search for this later, as part of the <title> - <episode> pattern
  AddTvResults(buffer, resultSet, searchString4, normedStrings, lang);
  return false;
}
bool cMovieDBScraper::AddTvResults(cLargeString &buffer, vector<searchResultTvMovie> &resultSet, string_view tvSearchString, const std::vector<std::optional<cNormedString>> &normedStrings, const cLanguage *lang) {
// search for tv series, add search results to resultSet
// concatenate URL
  CONVERT(SearchString_rom, tvSearchString, removeRomanNumC);
  if (*SearchString_rom == 0) {
    esyslog("tvscraper: ERROR cMovieDbTv::AddTvResults, SearchString_rom == empty");
    return false;
  }
  std::string url; url.reserve(300);
  stringAppend(url, baseURL, "/search/tv?api_key=", apiKey);
  if (lang) stringAppend(url, "&language=", lang->m_themoviedb);
  stringAppend(url, "&query=");
  stringAppendCurlEscape(url, SearchString_rom);

  rapidjson::Document root;
// call api, get json
  if (!jsonCallRest(root, buffer, url.c_str(), config.enableDebug)) return false;
  for (const rapidjson::Value &result: cJsonArrayIterator(root, "results")) {
    int id = getValueInt(result, "id");
    if (id == 0) continue;
    bool alreadyInList = false;
    for (const searchResultTvMovie &sRes: resultSet) if (sRes.id() == id) { alreadyInList = true; break; }
    if (alreadyInList) continue;

// create new result object sRes
    searchResultTvMovie sRes(id, false, getValueCharS(result, "first_air_date") );
    sRes.setPositionInExternalResult(resultSet.size() );

// distance == deviation from search text
    int dist_a = 1000;
    sRes.normedName.reset(removeLastPartWithP(getValueCharS(result, "original_name")));
    dist_a = sRes.normedName.minDistanceNormedStrings(normedStrings, dist_a);
    if (lang) {
// search string is not in original language, reduce match to original_name somewhat
      dist_a = std::min(dist_a + 50, 1000);
      sRes.normedName.reset(removeLastPartWithP(getValueCharS(result, "name")));
      dist_a = sRes.normedName.minDistanceNormedStrings(normedStrings, dist_a);
    }
    dist_a = std::min(dist_a + 10, 1000); // avoid this, prefer TVDB
    sRes.setMatchText(dist_a);
    sRes.setPopularity(getValueDouble(result, "popularity"), getValueDouble(result, "vote_average"), getValueInt(result, "vote_count") );
    resultSet.push_back(std::move(sRes));
  }
  return true;
}

void cMovieDBScraper::AddMovieResults(vector<searchResultTvMovie> &resultSet, std::string_view SearchString, const char *description, const cYears &years, const cLanguage *lang) {
  string_view searchString1, searchString2, searchString3, searchString4;
  std::vector<std::optional<cNormedString>> normedStrings = getNormedStrings(SearchString, searchString1, searchString2, searchString3, searchString4);

  cLargeString buffer("cMovieDbMovie::AddMovieResults", 10000);
  size_t size0 = resultSet.size();
  AddMovieResults(buffer, resultSet, SearchString, normedStrings, description, true, years, lang);
  if (searchString1.length() > 6  || (!searchString1.empty() && resultSet.size() == size0) )
    AddMovieResults(buffer, resultSet, searchString1, normedStrings, description, false, years, lang);
  if (searchString2.length() > 6  || (!searchString2.empty() && resultSet.size() == size0) )
    AddMovieResults(buffer, resultSet, searchString2, normedStrings, description, false, years, lang);
  if (searchString3.length() > 6  || (!searchString3.empty() && resultSet.size() == size0) )
    AddMovieResults(buffer, resultSet, searchString3, normedStrings, description, false, years, lang);
  if (searchString4.length() > 6  || (!searchString4.empty() && resultSet.size() == size0) )
    AddMovieResults(buffer, resultSet, searchString4, normedStrings, description, false, years, lang);
}

void cMovieDBScraper::AddMovieResults(cLargeString &buffer, vector<searchResultTvMovie> &resultSet, std::string_view SearchString, const std::vector<std::optional<cNormedString>> &normedStrings, const char *description, bool setMinTextMatch, const cYears &years, const cLanguage *lang) {
  if (SearchString.empty() ) {
    esyslog("tvscraper: ERROR cMovieDbMovie::AddMovieResults, SearchString == empty");
    return;
  }
  const char *t = config.GetThemoviedbSearchOption().c_str();
  CONVERT(SearchString_ext_rom, SearchString, removeRomanNumC);
  if (*SearchString_ext_rom == 0) {
    esyslog("tvscraper: ERROR cMovieDbMovie::AddMovieResults, SearchString_ext_rom == empty");
    return;
  }
  CURLESCAPE(query, SearchString_ext_rom);
  int numLa1 = lang?concat::numChars("&language="):0;
  int numLa2 = lang?concat::numChars(lang->m_themoviedb):0;
  char lang_u[numLa1 + numLa2 + 1];
  if (lang) {
    concat::addChars(lang_u, numLa1, "&language=");
    concat::addChars(lang_u + numLa1, numLa2, lang->m_themoviedb);
  }
  lang_u[numLa1 + numLa2] = 0;

  CONCATENATE(url, baseURL, "/search/movie?api_key=", GetApiKey(), lang_u, t, "&query=", query);
  int num_pages = AddMovieResultsForUrl(buffer, url, resultSet, normedStrings, description, setMinTextMatch);
  bool found = false;
  if (num_pages > 3 && years.size() + 1 < num_pages) {
// several pages, restrict with years
    for (int year: years) {
      CONCATENATE(url, baseURL, "/search/movie?api_key=", GetApiKey(), lang_u, t, "&year=", (unsigned int)year, "&query=", query);
      if (AddMovieResultsForUrl(buffer, url, resultSet, normedStrings, description, setMinTextMatch) > 0) found = true;
    }
  }
  if (!found) {
// no years avilable, or not found with the available year. Check all results in all pages
    if (num_pages > 10) num_pages = 10;
    for (int page = 2; page <= num_pages; page ++) {
      CONCATENATE(url, baseURL, "/search/movie?api_key=", GetApiKey(), lang_u, t, "&page=", (unsigned int)page, "&query=", query);
      AddMovieResultsForUrl(buffer, url, resultSet, normedStrings, description, setMinTextMatch);
    }
  }
}

int cMovieDBScraper::AddMovieResultsForUrl(cLargeString &buffer, const char *url, vector<searchResultTvMovie> &resultSet, const std::vector<std::optional<cNormedString>> &normedStrings, const char *description, bool setMinTextMatch) {
// return 0 if no results where found (calling the URL shows no results). Otherwise number of pages
// add search results from URL to resultSet
  rapidjson::Document root;
  if (!jsonCallRest(root, buffer, url, config.enableDebug)) return 0;
  if (getValueInt(root, "total_results", 0, "cMovieDbMovie::AddMovieResultsForUrl") == 0) return 0;
  AddMovieResults(root, resultSet, normedStrings, description, setMinTextMatch);
  return getValueInt(root, "total_pages", 0, "cMovieDbMovie::AddMovieResultsForUrl");
}
void cMovieDBScraper::AddMovieResults(const rapidjson::Document &root, vector<searchResultTvMovie> &resultSet, const std::vector<std::optional<cNormedString>> &normedStrings, const char *description, bool setMinTextMatch) {
  int res = -1;
  for (const rapidjson::Value &result: cJsonArrayIterator(root, "results")) {
    if (!result.IsObject() ) continue;
    ++res;
// id of result
    int id = getValueInt(result, "id", 0, "cMovieDbMovie::AddMovieResults, id");
    if (!id) continue;
    bool alreadyInList = false;
    for (const searchResultTvMovie &sRes: resultSet) if (sRes.id() == id) { alreadyInList = true; break; }
    if (alreadyInList) continue;

// a result was found and will be added to the list
    searchResultTvMovie sRes(id, true, getValueCharS(result, "release_date") );
    sRes.setPositionInExternalResult(resultSet.size() );
// Title & OriginalTitle
    int distOrigTitle = cNormedString(removeLastPartWithP(getValueCharS(result, "original_title"))).minDistanceNormedStrings(normedStrings, 1000);
    distOrigTitle = std::min(1000, distOrigTitle + 50);  // increase distance for original title, because it's a less likely match
    int dist = cNormedString(removeLastPartWithP(getValueCharS(result, "title"))).minDistanceNormedStrings(normedStrings, distOrigTitle);
    if (dist > 300 && description && strlen(description) > 25) {
      const char *overview = getValueCharS(result, "overview");
      if (overview && *overview) dist = cNormedString(description).sentence_distance(overview, dist);
    }
    if (setMinTextMatch && res == 0) dist = std::min(dist, 500); // api.themoviedb.org has some alias names, which are used in the search but not displayed. Set the text match to a "minimum" of 500 -> 0.5 , because it was found by api.themoviedb.org
    sRes.setMatchText(dist);
    sRes.setPopularity(getValueDouble(result, "popularity"), getValueDouble(result, "vote_average"), getValueInt(result, "vote_count") );
    resultSet.push_back(std::move(sRes));
  }
}


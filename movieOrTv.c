#include "movieOrTv.h"
#include <fstream>
#include <iostream>
#include <filesystem>

// implemntation of cMovieMoviedb
void cMovieMoviedb::DeleteMediaAndDb(cTVScraperDB *db) {
  stringstream base;
  base << config.GetBaseDir() << "/movies/" << m_id;
  DeleteFile(base.str() + "_backdrop.jpg");
  DeleteFile(base.str() + "_poster.jpg");
  db->DeleteMovie(m_id);
} 
bool cMovieMoviedb::IsUsed(cTVScraperDB *db) {
  return db->CheckMovieOutdatedEvents(m_id, -100, 0) || db->CheckMovieOutdatedRecordings(m_id, -100, 0);
}

// implemntation of cTvMoviedb
void cTvMoviedb:: DeleteMediaAndDb(cTVScraperDB *db) {
  stringstream folder;
  folder << config.GetBaseDir() << "/movies/tv/" << m_id;
  DeleteAll(folder.str() );
  db->DeleteSeries(m_id);
}

bool cTvMoviedb::IsUsed(cTVScraperDB *db) {
  return db->CheckMovieOutdatedEvents(m_id, 0, 0) || db->CheckMovieOutdatedRecordings(m_id, 0, 0);
}


// implemntation of cTvTvdb
void cTvTvdb:: DeleteMediaAndDb(cTVScraperDB *db) {
  stringstream folder;
  folder << config.GetBaseDir() << "/series/" << m_id;
  DeleteAll(folder.str() );
  db->DeleteSeries(m_id * -1);
}

bool cTvTvdb::IsUsed(cTVScraperDB *db) {
  return db->CheckMovieOutdatedEvents(m_id * -1, 0, 0) || db->CheckMovieOutdatedRecordings(m_id * -1, 0, 0);
}

// create object
cMovieOrTv *cMovieOrTv::getMovieOrTv(int id, ecMovieOrTvType type) {
  switch (type) {
    case theMoviedbMovie:  return new cMovieMoviedb(id);
    case theMoviedbSeries: return new cTvMoviedb(id);
    case theTvdbSeries:    return new cTvTvdb(id);
  }
  return NULL;
}

void cMovieOrTv::DeleteAllIfUnused(cTVScraperDB *db) {
// check all movies in db
  for(const vector<string> &movie: db->GetAllMovies() ) if (movie.size() == 1) {
    cMovieMoviedb movieMoviedb(atoi(movie[0].c_str() ));
    movieMoviedb.DeleteIfUnused(db);
  }

// check all tv shows in db
  for(const vector<string> &tv: db->GetAllTv() ) if (tv.size() == 1) {
      int tvID = atoi(tv[0].c_str() );
      if (tvID > 0) { cTvMoviedb tvMoviedb(tvID); tvMoviedb.DeleteIfUnused(db); }
         else        { cTvTvdb tvTvdb(tvID * -1); tvTvdb.DeleteIfUnused(db); }
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
    cMovieOrTv *movieOrTv = getMovieOrTv(atoi(dir_entry.path().filename().string().c_str() ), type);
    movieOrTv->DeleteIfUnused(db);
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
    cMovieMoviedb movieMoviedb(atoi(dir_entry.path().filename().string().c_str() ));
    movieMoviedb.DeleteIfUnused(db);
  }
}


#include <string>
#include "moviedbactors.h"

void readAndStoreMovieDbActors(cTVScraperDB *db, const rapidjson::Value &jActors, int movieTvID, bool movie, bool episode) {
// for movies: movie = true, otherwise: movie = false
// for tv: if episode == true -> "guest_stars"
// jActors must have the node "cast" (except for episode == true -> "guest_stars")
  cSql stmtAddActorDownload = db->addActorDownload(); // "INSERT OR REPLACE INTO actor_download (movie_id, is_movie, actor_id, actor_path) VALUES (?, ?, ?, ?);"
  const char *insMovie ="INSERT OR REPLACE INTO actor_movie      (actor_id, movie_id,  actor_role) VALUES (?, ?, ?);";
  const char *insTv =   "INSERT OR REPLACE INTO actor_tv         (actor_id, tv_id,     actor_role) VALUES (?, ?, ?);";
  const char *insEpis = "INSERT OR REPLACE INTO actor_tv_episode (actor_id, episode_id,actor_role) VALUES (?, ?, ?);";
  const char *ins = movie?insMovie:episode?insEpis:insTv;
  cSql stmtInsertActorMovie(db, cStringRef(ins));
  cSql stmtCheckActors(db, "SELECT 1 FROM actors WHERE actor_id = ? LIMIT 1");
  cSql stmtInsertActors(db, "INSERT OR REPLACE INTO actors (actor_id, actor_name, actor_has_image) VALUES (?, ?, ?);");
  for (const rapidjson::Value &jActor: cJsonArrayIterator(jActors, episode?"guest_stars":"cast")) {
    int id = getValueInt(jActor, "id");
    const char *name = getValueCharS(jActor, "name");
    if (id == 0 || !name) continue;
    const char *role = getValueCharS(jActor, "character");
    const char *path = getValueCharS(jActor, "profile_path");
    stmtInsertActorMovie.resetBindStep(id, movieTvID, cStringRef(role));
    if (path && *path) {
      stmtInsertActors.resetBindStep(id, cStringRef(name), true);
      stmtAddActorDownload.resetBindStep(movieTvID, movie, id, cStringRef(path));
    } else {
      if (!stmtCheckActors.resetBindStep(id).readRow() )
        stmtInsertActors.resetBindStep(id, cStringRef(name), false);
    }
  }
}

void getDirectorWriter(std::string &director, std::string &writer, const rapidjson::Value &document) {
  cContainer director_c;
  cContainer writer_c;
  for (const rapidjson::Value &jCrewMember: cJsonArrayIterator(document, "crew")) {
    const char *fieldValue = getValueCharS(jCrewMember, "job");
    if (fieldValue && strcmp(fieldValue, "Director") == 0) director_c.insert(getValueCharS(jCrewMember, "name"));
    fieldValue = getValueCharS(jCrewMember, "department");
    if (fieldValue && strcmp(fieldValue, "Writing") == 0) writer_c.insert(getValueCharS(jCrewMember, "name"));
  }
  director = director_c.moveBuffer();
  writer   = writer_c.moveBuffer();
}

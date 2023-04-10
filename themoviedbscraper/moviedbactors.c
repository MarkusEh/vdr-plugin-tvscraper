#include <string>
#include "moviedbactors.h"

void readAndStoreMovieActors(cTVScraperDB *db, const rapidjson::Value &jActors, int movieID) {
  rapidjson::Value::ConstMemberIterator jCrew_it = jActors.FindMember("crew");
  if (jCrew_it != jActors.MemberEnd() && jCrew_it->value.IsArray() ) {
    std::string director = cMovieDbTv::GetCrewMember(jCrew_it->value, "job", "Director");
    std::string writer   = cMovieDbTv::GetCrewMember(jCrew_it->value, "department", "Writing");
    db->exec("UPDATE movie_runtime2 SET movie_director = ?, movie_writer = ? WHERE movie_id = ?", director, writer, movieID);
  }
  cSql stmtAddActorDownload(db);
  cSql stmtInsertActorMovie(db);
  cSql stmtCheckActors(db);
  cSql stmtInsertActors(db);
  const char *insertActors = "INSERT OR REPLACE INTO actors (actor_id, actor_name, actor_has_image) VALUES (?, ?, ?);";
  for (const rapidjson::Value &jActor: cJsonArrayIterator(jActors, "cast")) {
    int id = getValueInt(jActor, "id");
    const char *name = getValueCharS(jActor, "name");
    if (id == 0 || !name) continue;
    const char *role = getValueCharS(jActor, "character");
    const char *path = getValueCharS(jActor, "profile_path");
    stmtInsertActorMovie.prepareBindStep("INSERT OR REPLACE INTO actor_movie (actor_id, movie_id, actor_role) VALUES (?, ?, ?);",
      id, movieID, role);
    if (path && *path) {
      stmtInsertActors.prepareBindStep(insertActors, id, name, true);
      db->AddActorDownload(stmtAddActorDownload, movieID, true, id, path);
    } else {
      stmtCheckActors.prepareBindStep("SELECT 1 FROM actors WHERE actor_id = ? LIMIT 1", id);
      if (!stmtCheckActors.readRow() )
        stmtInsertActors.prepareBindStep(insertActors, id, name, false);
    }
  }
}

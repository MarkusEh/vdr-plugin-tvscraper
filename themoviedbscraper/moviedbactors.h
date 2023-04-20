#ifndef __TVSCRAPER_MOVIEDBACTORS_H
#define __TVSCRAPER_MOVIEDBACTORS_H


void readAndStoreMovieDbActors(cTVScraperDB *db, const rapidjson::Value &jActors, int movieTvEpisodeID, bool movie, bool episode = false);
void getDirectorWriter(std::string &director, std::string &writer, const rapidjson::Value &document);


#endif //__TVSCRAPER_MOVIEDBACTORS_H

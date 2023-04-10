#ifndef __TVSCRAPER_MOVIEDBACTORS_H
#define __TVSCRAPER_MOVIEDBACTORS_H


void readAndStoreMovieActors(cTVScraperDB *db, const rapidjson::Value &jActors, int movieID);

#endif //__TVSCRAPER_MOVIEDBACTORS_H

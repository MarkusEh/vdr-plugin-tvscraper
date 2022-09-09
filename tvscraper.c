/*
 * tvscraper.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */
enum eMediaType {
    mediaFanartCollection = -2,
    mediaPosterCollection = -1,
    mediaUnknown = 0,
    mediaPoster = 1,
    mediaFanart = 2,
    mediaSeason = 3,
    mediaBanner = 4,
};

#include <getopt.h>
#include <vdr/plugin.h>
#include "searchResultTvMovie.h"
#include "searchResultTvMovie.c"
#include "tools/splitstring.c"
#include "tools/stringhelpers.c"
#include "config.h"
#include "eventOrRec.h"
#include "tvscraperdb.h"
#include "autoTimers.h"
#include "config.c"
cTVScraperConfig config;
#include "eventOrRec.c"
#include "tools/jsonHelpers.c"
#include "tools/curlfuncs.cpp"
#include "tools/filesystem.c"
#include "tools/fuzzy.c"
#include "overrides.c"
#include "tvscraperdb.c"
#include "thetvdbscraper/tvdbseries.c"
#include "thetvdbscraper/thetvdbscraper.c"
#include "themoviedbscraper/moviedbtv.c"
#include "themoviedbscraper/moviedbmovie.c"
#include "themoviedbscraper/moviedbactors.c"
#include "themoviedbscraper/themoviedbscraper.c"
#include "movieOrTv.c"
#include "searchEventOrRec.c"
#include "worker.c"
#include "services.h"
#include "setup.c"
#include "images.c"
#include "autoTimers.c"

static const char *VERSION        = "1.1.5";
static const char *DESCRIPTION    = "Scraping movie and series info";
// static const char *MAINMENUENTRY  = "TV Scraper";


class cPluginTvscraper : public cPlugin {
private:
    bool cacheDirSet;
    cTVScraperDB *db;
    cTVScraperWorker *workerThread;
    cOverRides *overrides;
    cMovieOrTv *lastMovieOrTv = NULL;
public:
    cPluginTvscraper(void);
    virtual ~cPluginTvscraper();
    virtual const char *Version(void) { return VERSION; }
    virtual const char *Description(void) { return DESCRIPTION; }
    virtual const char *CommandLineHelp(void);
    virtual bool ProcessArgs(int argc, char *argv[]);
    virtual bool Initialize(void);
    virtual bool Start(void);
    virtual void Stop(void);
    virtual void Housekeeping(void);
    virtual void MainThreadHook(void);
    virtual cString Active(void);
    virtual time_t WakeupTime(void);
    virtual const char *MainMenuEntry(void) { return NULL; }
    virtual cOsdObject *MainMenuAction(void);
    virtual cMenuSetupPage *SetupMenu(void);
    virtual bool SetupParse(const char *Name, const char *Value);
    cMovieOrTv *GetMovieOrTv(const cEvent *event, const cRecording *recording, int *runtime=NULL);
    virtual bool Service(const char *Id, void *Data = NULL);
    virtual const char **SVDRPHelpPages(void);
    virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
};

cPluginTvscraper::cPluginTvscraper(void) {
}

cPluginTvscraper::~cPluginTvscraper() {
}

const char *cPluginTvscraper::CommandLineHelp(void) {
    return "  -d <CACHEDIR>, --dir=<CACHEDIR> Set directory where database and images are stored\n" \
           "  -c, --readOnlyClient Don't update any data, just read the data\n";
}

bool cPluginTvscraper::ProcessArgs(int argc, char *argv[]) {
    static const struct option long_options[] = {
        { "dir", required_argument, NULL, 'd' },
        { "readOnlyClient", no_argument, NULL, 'c' },
        { "themoviedbSearchOption", required_argument, NULL, 's' },
        { "autoTimersPath", required_argument, NULL, 'p' },
        { 0, 0, 0, 0 }
    };

    int c;
    cacheDirSet = false;
    while ((c = getopt_long(argc, argv, "d:s:c:p", long_options, NULL)) != -1) {
        switch (c) {
            case 'd':
                cacheDirSet = true;
                config.SetBaseDir(optarg);
                break;
            case 's':
                config.SetThemoviedbSearchOption(optarg);
                break;
            case 'p':
                config.SetAutoTimersPath(optarg);
                break;
            case 'c':
                config.SetReadOnlyClient();
                break;
            default:
                return false;
        }
    }
    return true;
}

bool cPluginTvscraper::Initialize(void) {
    if (!cacheDirSet) {
        config.SetBaseDir(cPlugin::CacheDirectory(PLUGIN_NAME_I18N));
        cacheDirSet = true;
        config.Initialize();
    }
    return true;
}

bool cPluginTvscraper::Start(void) {
    db = new cTVScraperDB();
    if (!db->Connect()) {
        esyslog("tvscraper: could not connect to Database. Aborting!");
        return false;
    };
    overrides = new cOverRides();
    overrides->ReadConfig(cPlugin::ConfigDirectory(PLUGIN_NAME_I18N));
    workerThread = new cTVScraperWorker(db, overrides);
    workerThread->SetDirectories();
    workerThread->SetLanguage();
    workerThread->Start();
    return true;
}

void cPluginTvscraper::Stop(void) {
    while (workerThread->Active()) {
        workerThread->Stop();
    }
    delete workerThread;
    delete db;
    delete overrides;
}

void cPluginTvscraper::Housekeeping(void) {
}

void cPluginTvscraper::MainThreadHook(void) {
}

cString cPluginTvscraper::Active(void) {
    return NULL;
}

time_t cPluginTvscraper::WakeupTime(void) {
    return 0;
}

cOsdObject *cPluginTvscraper::MainMenuAction(void) {
    return NULL;
}

cMenuSetupPage *cPluginTvscraper::SetupMenu(void) {
    return new cTVScraperSetup(workerThread, *db);
}

bool cPluginTvscraper::SetupParse(const char *Name, const char *Value) {
    return config.SetupParse(Name, Value);
}

cMovieOrTv *cPluginTvscraper::GetMovieOrTv(const cEvent *event, const cRecording *recording, int *runtime) {
// NULL will be returned in case of errors or if nothing was found
// otherwiese, a new cMovieOrTv will be created. Don't forget to delete the cMovieOrTv!!!
  if (event && recording) {
    esyslog("tvscraper: ERROR calling vdr service interface, call->event && call->recording are provided. Please set one of these parameters to NULL");
    return NULL;
  }
  csEventOrRecording *sEventOrRecording = GetsEventOrRecording(event, recording);
  if (!sEventOrRecording) return NULL;

  cMovieOrTv *movieOrTv = cMovieOrTv::getMovieOrTv(db, sEventOrRecording, runtime);
  delete sEventOrRecording;
  return movieOrTv;
}

bool cPluginTvscraper::Service(const char *Id, void *Data) {
    if (strcmp(Id, "GetScraperOverview") == 0) {
        if (Data == NULL) return true;
        cGetScraperOverview* call = (cGetScraperOverview*) Data;
        cMovieOrTv *movieOrTv = GetMovieOrTv(call->m_event, call->m_recording, &call->m_runtime);
        if (!movieOrTv) { call->m_videoType = eVideoType::none; return true;}
        if (call->m_runtime < 0) call->m_runtime = 0;
        movieOrTv->getScraperOverview(call);
        delete movieOrTv;
        return true;
    }
    if (strcmp(Id, "GetScraperImageDir") == 0) {
        if (Data == NULL) return true;
        cGetScraperImageDir* call = (cGetScraperImageDir*) Data;
        call->scraperImageDir = config.GetBaseDir();
        return true;
    }
    if (strcmp(Id, "GetScraperMovieOrTv") == 0) {
        if (Data == NULL) return true;
    
        cScraperMovieOrTv* call = (cScraperMovieOrTv*) Data;
        call->found = false;
        cMovieOrTv *movieOrTv = GetMovieOrTv(call->event, call->recording);
        if (!movieOrTv) return true;

        movieOrTv->clearScraperMovieOrTv(call);
        call->found = true;
        movieOrTv->getScraperMovieOrTv(call);
        delete movieOrTv;
	
        return true;
    }

    if (strcmp(Id, "GetEventType") == 0) {
// Commit 096894d4 in https://gitlab.com/kamel5/SkinNopacity fixes a bug in SkinNopacity. Part of SkinNopacity 1.1.12
// TVGuide: Version 1.3.6+ is required
        if (Data == NULL) return true;
        ScraperGetEventType* call = (ScraperGetEventType*) Data;
        if (lastMovieOrTv) delete lastMovieOrTv;
        lastMovieOrTv = GetMovieOrTv(call->event, call->recording);
        call->type = tNone;
        call->movieId = 0;
        call->seriesId = 0;
        call->episodeId = 0;
        if (!lastMovieOrTv) return true;
        call->type = lastMovieOrTv->getType();
        if (call->type == tSeries) {
            call->seriesId = 1234;
        } else {
            call->movieId = 1234;
        }
        return true;
    }

    if (strcmp(Id, "GetSeries") == 0) {
        if (Data == NULL) return true;
        cSeries* call = (cSeries*) Data;
        if( call->seriesId == 0 || lastMovieOrTv == NULL) {
            esyslog("tvscraper: ERROR calling vdr service interface \"GetSeries\", call->seriesId == 0 || lastMovieOrTv == 0");
            call->name = "Error calling VDR service interface GetSeries";
            return true;
        }

        float popularity, vote_average;
        if (!db->GetTv(lastMovieOrTv->dbID(), call->name, call->overview, call->firstAired, call->network, call->genre, popularity, vote_average, call->status)) {
            esyslog("tvscraper: ERROR calling vdr service interface \"GetSeries\", Id = %i not found", lastMovieOrTv->dbID());
            call->name = "Error calling VDR service interface GetSeries";
            return true;
        }
        call->rating = vote_average;

// data for cEpisode episode;
        call->episode.season = lastMovieOrTv->getSeason();
        call->episode.number = lastMovieOrTv->getEpisode();
        int episodeID;
        db->GetTvEpisode(lastMovieOrTv->dbID(), lastMovieOrTv->getSeason(), lastMovieOrTv->getEpisode(), episodeID, call->episode.name, call->episode.firstAired, vote_average, call->episode.overview, call->episode.guestStars);
        call->episode.rating = vote_average;
// guestStars
        if(call->episode.guestStars.empty() ) {
          vector<vector<string> > GuestStars = db->GetGuestActorsTv(episodeID);
          int numActors = GuestStars.size();
          for (int i=0; i < numActors; i++) {
              vector<string> row = GuestStars[i];
              if (row.size() == 3) {
                  if(i != 0) call->episode.guestStars.append("; ");
                  call->episode.guestStars.append(row[1]); // name
                  call->episode.guestStars.append(": ");
                  call->episode.guestStars.append(row[2]); // role
              }
          }
        }
        lastMovieOrTv->getSingleImage(eImageLevel::episodeMovie, eOrientation::landscape, NULL, &call->episode.episodeImage.path, &call->episode.episodeImage.width, &call->episode.episodeImage.height);

// more not episode related data
        call->actors = lastMovieOrTv->GetActors();
        lastMovieOrTv->getSingleImage(eImageLevel::seasonMovie, eOrientation::portrait, NULL, &call->seasonPoster.path, &call->seasonPoster.width, &call->seasonPoster.height); 
        call->posters = lastMovieOrTv->getImages(eOrientation::portrait);
        call->banners = lastMovieOrTv->getBanners();
        call->fanarts = lastMovieOrTv->getImages(eOrientation::landscape);

        return true;
    }

    if (strcmp(Id, "GetMovie") == 0) {
        if (Data == NULL) return true;
        cMovie* call = (cMovie*) Data;
        if (call->movieId == 0 || lastMovieOrTv == NULL) {
            esyslog("tvscraper: ERROR calling vdr service interface \"GetMovie\", call->movieId == 0 || lastMovieOrTv == 0");
            call->title = "Error calling VDR service interface GetMovie";
            return true;
        }

        int collection_id;
        if (!db->GetMovie(lastMovieOrTv->dbID(), call->title, call->originalTitle, call->tagline, call->overview, call->adult, collection_id, call->collectionName, call->budget, call->revenue, call->genres, call->homepage, call->releaseDate, call->runtime, call->popularity, call->voteAverage)) {
            esyslog("tvscraper: ERROR calling vdr service interface \"GetMovie\", Id = %i not found", lastMovieOrTv->dbID());
            call->title = "Error calling VDR service interface GetMovie";
            return true;
        }

        cImageLevelsInt level(eImageLevel::seasonMovie, eImageLevel::tvShowCollection);
        lastMovieOrTv->getSingleImageBestL(level, eOrientation::portrait, NULL, &call->poster.path, &call->poster.width, &call->poster.height); 
        lastMovieOrTv->getSingleImageBestL(level, eOrientation::landscape, NULL, &call->fanart.path, &call->fanart.width, &call->fanart.height);
        lastMovieOrTv->getSingleImage(eImageLevel::tvShowCollection, eOrientation::portrait,  NULL, &call->collectionPoster.path, &call->collectionPoster.width, &call->collectionPoster.height); 
        lastMovieOrTv->getSingleImage(eImageLevel::tvShowCollection, eOrientation::landscape, NULL, &call->collectionFanart.path, &call->collectionFanart.width, &call->collectionFanart.height); 
        call->actors = lastMovieOrTv->GetActors();

        return true;
    }
    
    if (strcmp(Id, "GetPosterBanner") == 0) {
// if banner available: for TV shows: return banner, and report as TV show (tSeries)
// otherwise: return posterand report as movie (tMovie)
        if (Data == NULL) return true;
        ScraperGetPosterBanner* call = (ScraperGetPosterBanner*) Data;
        cMovieOrTv *movieOrTv = GetMovieOrTv(call->event, NULL);
        if (!movieOrTv) { call->type = tNone; return true;}
        if (movieOrTv->getSingleImageBestL(
            cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection),
            eOrientation::banner,
            NULL, &call->banner.path, &call->banner.width, &call->banner.height) != eImageLevel::none) {
// Banner available -> return banner
          call->type = tSeries;
        } else {
// No banner available -> return poster
          call->type = tMovie;
          movieOrTv->getSingleImageBestLO(
            cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection),
            cOrientationsInt(eOrientation::portrait, eOrientation::landscape),
            NULL, &call->poster.path, &call->poster.width, &call->poster.height);
        }
        delete movieOrTv;
        return true;
    }

    if (strcmp(Id, "GetPosterBannerV2") == 0) {
// for TV shows: return banner
// for movies: get poster
        if (Data == NULL) return true;
        ScraperGetPosterBannerV2* call = (ScraperGetPosterBannerV2*) Data;
        cMovieOrTv *movieOrTv = GetMovieOrTv(call->event, call->recording);
        if (!movieOrTv) { call->type = tNone; return true;}
        if (movieOrTv->getSingleImageBestL(
            cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection),
            eOrientation::banner,
            NULL, &call->banner.path, &call->banner.width, &call->banner.height) != eImageLevel::none) {
// Banner available -> return banner
          call->type = tSeries;
        } else {
// No banner available -> return poster
          call->type = tMovie;
          movieOrTv->getSingleImageBestLO(
            cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection),
            cOrientationsInt(eOrientation::portrait, eOrientation::landscape),
            NULL, &call->poster.path, &call->poster.width, &call->poster.height);
        }
        delete movieOrTv;
        return true;
    }

    if (strcmp(Id, "GetPoster") == 0) {
        if (Data == NULL) return true;
        ScraperGetPoster* call = (ScraperGetPoster*) Data;
        cMovieOrTv *movieOrTv = GetMovieOrTv(call->event, call->recording);
        if (!movieOrTv) {
          call->poster.path = "";
          call->poster.width = 0;
          call->poster.height = 0;
        } else {
          movieOrTv->getSingleImageBestLO(
            cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection),
            cOrientationsInt(eOrientation::portrait, eOrientation::landscape, eOrientation::banner),
            NULL, &call->poster.path, &call->poster.width, &call->poster.height);
          delete movieOrTv;
        }
        return true;
    }

    if (strcmp(Id, "GetPosterThumb") == 0) {
        if (Data == NULL) return true;
        ScraperGetPosterThumb* call = (ScraperGetPosterThumb*) Data;
        cMovieOrTv *movieOrTv = GetMovieOrTv(call->event, call->recording);
        if (!movieOrTv) {
          call->poster.path = "";
          call->poster.width = 0;
          call->poster.height = 0;
        } else {
          movieOrTv->getSingleImageBestLO(
            cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection),
            cOrientationsInt(eOrientation::portrait, eOrientation::landscape, eOrientation::banner),
            NULL, &call->poster.path, &call->poster.width, &call->poster.height);
          delete movieOrTv;
        }
        return true;
    }

    if (strcmp(Id, "GetScraperUpdateTimes") == 0) {
      if (Data == NULL) return true;
      cGetScraperUpdateTimes* call = (cGetScraperUpdateTimes*) Data;
      call->m_EPG_UpdateTime = LastModifiedTime(config.GetEPG_UpdateFileName().c_str() );
      call->m_recordingsUpdateTime = LastModifiedTime(config.GetRecordingsUpdateFileName().c_str() );
      return true;
    }
  return false;
}

const char **cPluginTvscraper::SVDRPHelpPages(void) {
 static const char *HelpPages[] = {
    "SCRE\n"
    "    Scrap recordings.\n"
    "    Before that, delete all existing scrap results for recordings.\n"
    "    Note: Cache is not deleted, and will be used.\n"
    "    Alternate command: ScrapRecordings",
    "SCEP\n"
    "    Scrap EPG.\n"
    "    Before that, delete all existing scrap results for EPG.\n"
    "    Note: Cache is not deleted, and will be used.\n"
    "    Alternate command: ScrapEPG",
    NULL
    };
  return HelpPages;
}

cString cPluginTvscraper::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode) {
  if ((strcasecmp(Command, "SCRE") == 0) || (strcasecmp(Command, "ScrapRecordings") == 0)) {
    workerThread->InitVideoDirScan();
    return cString("Scraping Video Directory started");
  }
  if ((strcasecmp(Command, "SCEP") == 0) || (strcasecmp(Command, "ScrapEPG") == 0)) {
    workerThread->InitManualScan();
    return cString("EPG Scraping started");
  }
  if ((strcasecmp(Command, "DELC") == 0) || (strcasecmp(Command, "DeleteCache") == 0)) {
    if ( Option && *Option ) {
      int del_entries = db->DeleteFromCache(Option);
      return cString::sprintf("%i cache entry/entries deleted", del_entries);
    }
    ReplyCode = 550;
    return cString("Option missing");
  }
  if ((strcasecmp(Command, "DELS") == 0) || (strcasecmp(Command, "DeleteSeries") == 0)) {
    if ( Option && *Option ) {
      int del_entries = db->DeleteSeries(atoi(Option) );
      return cString::sprintf("%i series entry/entries deleted", del_entries);
    }
    ReplyCode = 550;
    return cString("Option missing");
  }
  if ((strcasecmp(Command, "DELM") == 0) || (strcasecmp(Command, "DeleteMovie") == 0)) {
    if ( Option && *Option ) {
      int del_entries = db->DeleteMovie(atoi(Option) );
      return cString::sprintf("%i movie entry/entries deleted", del_entries);
    }
    ReplyCode = 550;
    return cString("Option missing");
  }
  return NULL;
}

VDRPLUGINCREATOR(cPluginTvscraper);

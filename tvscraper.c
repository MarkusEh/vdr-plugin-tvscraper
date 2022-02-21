/*
 * tvscraper.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */
#include <getopt.h>
#include <vdr/plugin.h>
#include "searchResultTvMovie.h"
#include "searchResultTvMovie.c"
#include "tools/splitstring.c"
#include "tools/stringhelpers.c"
#include "config.c"
cTVScraperConfig config;
#include "eventOrRec.c"
#include "tools/jsonHelpers.c"
#include "tools/curlfuncs.cpp"
#include "tools/filesystem.c"
#include "tools/fuzzy.c"
#include "overrides.c"
#include "tvscraperdb.c"
#include "thetvdbscraper/tvdbmirrors.c"
#include "thetvdbscraper/tvdbseries.c"
#include "thetvdbscraper/tvdbmedia.c"
#include "thetvdbscraper/tvdbactors.c"
#include "thetvdbscraper/thetvdbscraper.c"
#include "themoviedbscraper/moviedbtv.c"
#include "themoviedbscraper/moviedbmovie.c"
#include "themoviedbscraper/moviedbactors.c"
#include "themoviedbscraper/themoviedbscraper.c"
#include "movieOrTv.c"
#include "searchEventOrRec.c"
#include "worker.c"
#include "services.h"
#include "imageserver.c"
#include "setup.c"

static const char *VERSION        = "0.9.6";
static const char *DESCRIPTION    = "Scraping movie and series info";
// static const char *MAINMENUENTRY  = "TV Scraper";


class cPluginTvscraper : public cPlugin {
private:
    bool cacheDirSet;
    cTVScraperDB *db;
    cTVScraperWorker *workerThread;
    cImageServer *imageServer;
    cOverRides *overrides;
    int lastEventId;
    int lastSeasonNumber;
    int lastEpisodeNumber;
    bool GetPosterOrOtherPicture(cTvMedia &media, const cEvent *event, const cRecording *recording);
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
    virtual bool Service(const char *Id, void *Data = NULL);
    virtual const char **SVDRPHelpPages(void);
    virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
};

cPluginTvscraper::cPluginTvscraper(void) {
}

cPluginTvscraper::~cPluginTvscraper() {
}

const char *cPluginTvscraper::CommandLineHelp(void) {
    return "  -d <CACHEDIR>, --dir=<CACHEDIR> Set directory where database and images are stored\n";
}

bool cPluginTvscraper::ProcessArgs(int argc, char *argv[]) {
    static const struct option long_options[] = {
        { "dir", required_argument, NULL, 'd' },
        { 0, 0, 0, 0 }
    };

    int c;
    cacheDirSet = false;
    while ((c = getopt_long(argc, argv, "d:", long_options, NULL)) != -1) {
        switch (c) {
            case 'd':
                cacheDirSet = true;
                config.SetBaseDir(optarg);
                break;
            default:
                return false;
        }
    }
    return true;
}

bool cPluginTvscraper::Initialize(void) {
    return true;
}

bool cPluginTvscraper::Start(void) {
    if (!cacheDirSet) {
        config.SetBaseDir(cPlugin::CacheDirectory(PLUGIN_NAME_I18N));
    }
    db = new cTVScraperDB();
    if (!db->Connect()) {
        esyslog("tvscraper: could not connect to Database. Aborting!");
        return false;
    };
    overrides = new cOverRides();
    overrides->ReadConfig(cPlugin::ConfigDirectory(PLUGIN_NAME_I18N));
    imageServer = new cImageServer(db);
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
    delete imageServer;
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
    return new cTVScraperSetup(workerThread);
}

bool cPluginTvscraper::SetupParse(const char *Name, const char *Value) {
    return config.SetupParse(Name, Value);
}

bool cPluginTvscraper::GetPosterOrOtherPicture(cTvMedia &media, const cEvent *event, const cRecording *recording) {
  csEventOrRecording *sEventOrRecording = GetsEventOrRecording(event, recording);
  if (!sEventOrRecording) return false;

  int id, sn, en;
  scrapType type = imageServer->GetIDs(sEventOrRecording, id, sn, en);
//      if (config.enableDebug) esyslog("tvscraper: GetPoster, id %i type %i", id, type);
  delete sEventOrRecording;
  if (type == scrapNone) return false;
// we have an event or rec., which is assigned to a movie or tv show :)
  media = imageServer->GetPoster(id, sn, en);
  if (media.width != 0) return true;
// try to add a picture as poster. As posters are used in lists, ...
  vector<cTvMedia> fanarts = imageServer->GetSeriesFanarts(id, sn, en); // for movies, this will return the backdrop
  if (fanarts.size() > 0) { media = fanarts[0]; return true; }
// try banner, as last resort ...
  if (id < 0 && imageServer->GetBanner(media, id) ) return true;
  return false;
}


bool cPluginTvscraper::Service(const char *Id, void *Data) {
    if (Data == NULL)
        return false;
    
    if (strcmp(Id, "GetEventType") == 0) {
        ScraperGetEventType* call = (ScraperGetEventType*) Data;
        csEventOrRecording *sEventOrRecording = GetsEventOrRecording(call->event, call->recording);
        if (!sEventOrRecording) {
            call->type = tNone;
            lastEventId = 0;
            return false;
	}

        scrapType type = imageServer->GetIDs(sEventOrRecording, lastEventId, lastSeasonNumber, lastEpisodeNumber);
        delete sEventOrRecording;

        if( lastEventId == 0 ) {
            call->type = tNone;
            return false;
        }

        if (type == scrapSeries) {
            call->type = tSeries;
            call->seriesId = 1234;
        } else if (type == scrapMovie) {
            call->type = tMovie;
            call->movieId = 1234;
        } else {
            call->type = tNone;
        }
	
        return true;
    }

    if (strcmp(Id, "GetSeries") == 0) {
        cSeries* call = (cSeries*) Data;
        if( call->seriesId == 0 || lastEventId == 0 )
            return false;

        float popularity, vote_average;
        db->GetTv(lastEventId, call->name, call->overview, call->firstAired, call->network, call->genre, popularity, vote_average, call->status);
        call->rating = vote_average;

// data for cEpisode episode;
        call->episode.season = lastSeasonNumber;
        call->episode.number = lastEpisodeNumber;
        int episodeID;
        db->GetTvEpisode(lastEventId, lastSeasonNumber, lastEpisodeNumber, episodeID, call->episode.name, call->episode.firstAired, vote_average, call->episode.overview, call->episode.guestStars);
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
        call->episode.episodeImage = imageServer->GetStill(lastEventId, lastSeasonNumber, lastEpisodeNumber);

// more not episode related data
        call->actors = imageServer->GetActors(lastEventId, episodeID, scrapSeries);
        call->posters = imageServer->GetPosters(lastEventId, scrapSeries);
        cTvMedia media;
        call->seasonPoster = media;  // default: empty
        call->banners.clear();
        if (imageServer->GetBanner(media, lastEventId) ) call->banners.push_back(media);
        call->fanarts = imageServer->GetSeriesFanarts(lastEventId, lastSeasonNumber, lastEpisodeNumber);

//        if (imageServer->GetTvPoster(media, lastEventId, lastSeasonNumber) ) call->seasonPoster = media;
        call->seasonPoster = imageServer->GetPoster(lastEventId, lastSeasonNumber, lastEpisodeNumber);
/*
        bool debug = lastEventId == -252244;
        if (debug) esyslog("tvscraper: getSeries, call->posters.size() = %lu, call->seasonPoster.width = %i, call->fanarts.size() = %lu", call->posters.size(), call->seasonPoster.width, call->fanarts.size() );
        if (call->seasonPoster.width == 0) {
// try to add a picture as poster. As posters are used in lists, ...
          if (call->fanarts.size() > 0) {
            cTvMedia media = imageServer->GetPoster(lastEventId, 1996, lastEpisodeNumber);
            if (media.width) {
              call->seasonPoster = media;
              call->posters.push_back(media);
              if (debug) esyslog("tvscraper: getSeries (2), call->posters.size() = %lu, call->fanarts.size() = %lu", call->posters.size(), call->fanarts.size() );
              if (debug && call->posters.size() > 0) esyslog("tvscraper: getSeries (3), call->posters[0].width %i, call->posters[0].path %s", call->posters[0].width, call->posters[0].path.c_str() );
            
              return true;
            }
            call->seasonPoster = call->fanarts[0];
            call->posters.push_back(call->seasonPoster);
            return true;
          }
          if (call->episode.episodeImage.width) { call->posters.push_back(call->episode.episodeImage); return true; }
// avoid banners, if possible. They have the wrong format ...
          if (call->banners.size() > 0) { call->posters.push_back(call->banners[0]); return true; }
        }
*/
        return true;
    }

    if (strcmp(Id, "GetMovie") == 0) {
        cMovie* call = (cMovie*) Data;
        if (call->movieId == 0 || lastEventId == 0)
            return false;

        int collection_id;
        db->GetMovie(lastEventId, call->title, call->originalTitle, call->tagline, call->overview, call->adult, collection_id, call->collectionName, call->budget, call->revenue, call->genres, call->homepage, call->releaseDate, call->runtime, call->popularity, call->voteAverage);


        call->poster = imageServer->GetPoster(lastEventId, lastSeasonNumber, lastEpisodeNumber);
        call->fanart = imageServer->GetMovieFanart(lastEventId);
        call->collectionPoster = imageServer->GetCollectionPoster(collection_id);
        call->collectionFanart = imageServer->GetCollectionFanart(collection_id);
        int episodeID = 0;
        call->actors = imageServer->GetActors(lastEventId, episodeID, scrapMovie);

        return true;
    }
    
    if (strcmp(Id, "GetPosterBanner") == 0) {
        ScraperGetPosterBanner* call = (ScraperGetPosterBanner*) Data;
        csEventOrRecording *sEventOrRecording = GetsEventOrRecording(call->event, NULL);
        if (!sEventOrRecording) return false;
        int id, sn, en;
        scrapType type = imageServer->GetIDs(sEventOrRecording, id, sn, en);
//        if (config.enableDebug) esyslog("tvscraper: GetPosterBanner, id %i type %i", id, type);
        delete sEventOrRecording;
        if (type == scrapSeries)
            call->type = tSeries;
        else if (type == scrapMovie)
            call->type = tMovie;
        else
            call->type = tNone;
        if (type != scrapNone) {
            cTvMedia media = imageServer->GetPosterOrBanner(id, sn, en, type);
            if( type == scrapMovie ) {
                call->poster = media;
            } else if( type == scrapSeries ) {
                call->banner = media;
            }
            return true;
        }
        return false;
    }

    if (strcmp(Id, "GetPoster") == 0) {
        ScraperGetPoster* call = (ScraperGetPoster*) Data;
        return GetPosterOrOtherPicture(call->poster, call->event, call->recording);
    }

    if (strcmp(Id, "GetPosterThumb") == 0) {
        ScraperGetPosterThumb* call = (ScraperGetPosterThumb*) Data;
        return GetPosterOrOtherPicture(call->poster, call->event, call->recording);
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

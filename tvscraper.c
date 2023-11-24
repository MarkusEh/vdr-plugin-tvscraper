/*
 * tvscraper.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */
#include "services.h"
#include <map>
// Some documantation about KODI TV interface, and supported data: ======================================
// see https://alwinesch.github.io/group__cpp__kodi__addon__pvr___defs__epg___p_v_r_e_p_g_tag.html
// see https://alwinesch.github.io/group__cpp__kodi__addon__pvr___defs___recording___p_v_r_recording.html
// Series link: If set for an epg-based timer rule, matching events will be found by checking strSeriesLink instead of strTitle (and bFullTextEpgSearch)  see https://github.com/xbmc/xbmc/pull/12609/files
// tag.SetFlags(PVR_RECORDING_FLAG_IS_SERIES);
// SetSeriesNumber (this is the season number)

// BEGIN OF deprecated !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// removed from services.h, please don't use
// still here, so old plugins (with the old interface) continue to work for some time ...

// Data structure for service "GetScraperImageDir"
// deprecated, please use "GetEnvironment". Note: GetEnvironment is also available in scraper2vdr
class cGetScraperImageDir {
public:
//in: nothing, no input required
//out
  std::string scraperImageDir;   // this was given to the plugin with --dir, or is the default cache directory for the plugin. It will always end with a '/'
  cPlugin *call(cPlugin *pScraper = NULL) {
    if (!pScraper) return cPluginManager::CallFirstService("GetScraperImageDir", this);
    else return pScraper->Service("GetScraperImageDir", this)?pScraper:NULL;
  }
};

// Data structure for service "GetPosterBanner"
// deprecated, please use "GetPosterBannerV2"
class ScraperGetPosterBanner {
public:
	ScraperGetPosterBanner(void) {
		type = tNone;
	};
// in
    const cEvent *event;             // check type for this event
//out
    tvType type;                	 //typeSeries or typeMovie
    cTvMedia poster;
    cTvMedia banner;
};
// END OF deprecated !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

enum eMediaType: int {
    mediaFanartCollection = -2,
    mediaPosterCollection = -1,
    mediaUnknown = 0,
    mediaPoster = 1,
    mediaFanart = 2,
    mediaSeason = 3,
    mediaBanner = 4,
};

#include <getopt.h>
#include "tools/stringhelpers.h"
#include "tools/tvscraperhelpers.h"
#include "tools/largeString.h"
#include "tools/channelhelpers.h"
#include "extEpgPlugin.h"
#include "config.h"
cTVScraperConfig config;
#include "tools/fuzzy.c"
#include "searchResultTvMovie.h"
#include "searchResultTvMovie.c"
#include "channelmap.c"
#include "eventOrRec.h"
#include "tvscraperdb.h"
#include "autoTimers.h"
#include "config.c"
#include "tools/jsonHelpers.c"
#include "tools/curlfuncs.cpp"
#include "tools/filesystem.c"
#include "tools/largeString.cpp"
#include "eventOrRec.c"
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
// #include "PLUGINS/example/extEpg.c"
#include "services.c"

static const char *VERSION        = "1.2.4";
static const char *DESCRIPTION    = "Scraping movie and series info";

//***************************************************************************
// cExtEpgHandler
//***************************************************************************

class cExtEpgHandler : public cEpgHandler
{
  public:
    cExtEpgHandler() : cEpgHandler() { }
    ~cExtEpgHandler() { }

    virtual bool SetDescription(cEvent *Event, const char *Description) {
// return true if we want to prevent a change of Event->Description()
      if (!Event->Description() ) return false;
      return config.myDescription(Event->Description() );
    }
    virtual bool SetShortText(cEvent *Event, const char *ShortText) {
// return true if we want to prevent a change of Event->ShortText()
      if (!ShortText || !*ShortText) return true;  // prevent deletion of an existing short text.
      return false;
    }
};

class cTVScraperLastMovieLock {
  private:
    cStateKey m_stateKey;
  public:
    cTVScraperLastMovieLock(bool Write = false) {
      config.stateLastMovieLock.Lock(m_stateKey, Write);
    }
    ~cTVScraperLastMovieLock() { m_stateKey.Remove(); }
};

//***************************************************************************
// cPluginTvscraper
//***************************************************************************

class cPluginTvscraper : public cPlugin {
private:
    bool cacheDirSet;
    cTVScraperDB *db;
    cTVScraperWorker *workerThread;
    cOverRides *overrides;
    int m_movie_tv_id = 0;
    int m_season_number = 0;
    int m_episode_number = 0;
    cExtEpgHandler *extEpgHandler = NULL;
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
// create, but never delete (because VDR deletes this during shutdown)
  extEpgHandler = new cExtEpgHandler();
}

cPluginTvscraper::~cPluginTvscraper() {
}

const char *cPluginTvscraper::CommandLineHelp(void) {
    return "  -d <CACHEDIR>, --dir=<CACHEDIR> Set directory where database and images are stored\n" \
           "  -c, --readOnlyClient Don't update any data, just read the data\n"  \
           "  -p, --autoTimersPath Overide the default path for auto timer recordings\n" \
           "  -t, --timersOnNumberOfTsFiles Create auto timers if number of ts files != 1\n";
}

bool cPluginTvscraper::ProcessArgs(int argc, char *argv[]) {
    static const struct option long_options[] = {
        { "dir", required_argument, NULL, 'd' },
        { "readOnlyClient", no_argument, NULL, 'c' },
        { "themoviedbSearchOption", required_argument, NULL, 's' },
        { "autoTimersPath", required_argument, NULL, 'p' },
        { "timersOnNumberOfTsFiles", no_argument, NULL, 't' },
        { 0, 0, 0, 0 }
    };

    int c;
    cacheDirSet = false;
    while ((c = getopt_long(argc, argv, "d:s:c:p:t", long_options, NULL)) != -1) {
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
            case 't':
                config.SetTimersOnNumberOfTsFiles();
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
    }
    return true;
}

bool cPluginTvscraper::Start(void) {
    config.Initialize();
    db = new cTVScraperDB();
    if (!db->Connect()) {
        esyslog("tvscraper: could not connect to Database. Aborting!");
        return false;
    };
    overrides = new cOverRides();
    overrides->ReadConfig(cPlugin::ConfigDirectory(PLUGIN_NAME_I18N));
    workerThread = new cTVScraperWorker(db, overrides);
    workerThread->SetDirectories();
    workerThread->Start();
    return true;
}

void cPluginTvscraper::Stop(void) {
    while (workerThread->Active()) {
        workerThread->Stop();
        sleep(1);
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
  return cMovieOrTv::getMovieOrTv(db, event, recording, runtime);
}

bool cPluginTvscraper::Service(const char *Id, void *Data) {
    if (strcmp(Id, "GetScraperVideo") == 0) {
        if (Data == NULL) return true;
        cGetScraperVideo* call = (cGetScraperVideo*) Data;
        call->m_scraperVideo = std::make_unique<cScraperVideoImp>(call->m_event, call->m_recording, db);
        return true;
    }
    if (strcmp(Id, "GetScraperImageDir") == 0) {
        if (Data == NULL) return true;
        cGetScraperImageDir* call = (cGetScraperImageDir*) Data;
        call->scraperImageDir = config.GetBaseDir();
        return true;
    }

    if (strcmp(Id, "GetEventType") == 0) {
// Commit 096894d4 in https://gitlab.com/kamel5/SkinNopacity fixes a bug in SkinNopacity. Part of SkinNopacity 1.1.12
// TVGuide: Version 1.3.6+ is required
        if (Data == NULL) return true;
        ScraperGetEventType* call = (ScraperGetEventType*) Data;
// set default return values
        call->movieId = 0;
        call->seriesId = 0;
        call->episodeId = 0;
        call->type = tNone;
// input validation
        if (call->event && call->recording) {
          esyslog("tvscraper: ERROR GetEventType, event && recording are provided. Please set one of these parameters to NULL");
          return true;
        }
// get m_movie_tv_id, m_season_number, m_episode_number;
        bool found = false;
        int movie_tv_id, season_number, episode_number;
        if (call->event    ) found = db->GetMovieTvID(call->event,     movie_tv_id, season_number, episode_number);
        if (call->recording) found = db->GetMovieTvID(call->recording, movie_tv_id, season_number, episode_number);
        if (!found || movie_tv_id == 0) {
          cTVScraperLastMovieLock l(true);
          m_movie_tv_id = 0;
          m_season_number = 0;
          m_episode_number = 0;
          return true;
        }
        if (season_number == -100) {
          call->movieId = movie_tv_id;
          call->type = tMovie;
        } else {
          call->seriesId = movie_tv_id;
          call->type = tSeries;
        }
        cTVScraperLastMovieLock l(true);
        m_movie_tv_id = movie_tv_id;
        m_season_number = season_number;
        m_episode_number = episode_number;
        return true;
    }

    if (strcmp(Id, "GetSeries") == 0) {
        if (Data == NULL) return true;
        cSeries* call = (cSeries*) Data;
        cMovieOrTv *movieOrTv = NULL;
        {
          cTVScraperLastMovieLock l;
          if (call->seriesId != m_movie_tv_id) return true; // we assume that the data request is outdated
          if (call->seriesId == 0 || m_movie_tv_id == 0) {
            esyslog("tvscraper: ERROR calling vdr service interface \"GetSeries\", call->seriesId == 0 || m_movie_tv_id == 0");
            call->name = "Error calling VDR service interface GetSeries";
            return true;
          }
          if (m_season_number == -100) {
            esyslog("tvscraper: ERROR calling vdr service interface \"GetSeries\", m_season_number == -100");
            call->name = "Error calling VDR service interface GetSeries";
            return true;
          }
          if (m_movie_tv_id > 0) movieOrTv = new cTvMoviedb(db, m_movie_tv_id, m_season_number, m_episode_number);
                else             movieOrTv = new cTvTvdb(db, -1*m_movie_tv_id, m_season_number, m_episode_number);
        }

        float popularity, vote_average;
        if (!db->GetTv(movieOrTv->dbID(), call->name, call->overview, call->firstAired, call->network, call->genre, popularity, vote_average, call->status)) {
          esyslog("tvscraper: ERROR calling vdr service interface \"GetSeries\", Id = %i not found", movieOrTv->dbID());
          call->name = "Error calling VDR service interface GetSeries";
          return true;
        }
        call->rating = vote_average;

// data for cEpisode episode;
        call->episode.season = movieOrTv->getSeason();
        call->episode.number = movieOrTv->getEpisode();
        int episodeID;
        db->GetTvEpisode(movieOrTv->dbID(), movieOrTv->getSeason(), movieOrTv->getEpisode(), episodeID, call->episode.name, call->episode.firstAired, vote_average, call->episode.overview, call->episode.guestStars);
        call->episode.rating = vote_average;
// guestStars
        if(movieOrTv->dbID() > 0) {
          call->episode.guestStars = "";
          for (cSql &stmt: cSql(db, "select actor_name, actor_role from actors, actor_tv_episode where actor_tv_episode.actor_id = actors.actor_id and actor_tv_episode.episode_id = ?", episodeID)) {
            if(!call->episode.guestStars.empty() ) call->episode.guestStars.append("; ");
            call->episode.guestStars.append(stmt.getCharS(0)); // name
            call->episode.guestStars.append(": ");
            call->episode.guestStars.append(stmt.getCharS(1)); // role
          }
        }
        movieOrTv->getSingleImage(eImageLevel::episodeMovie, eOrientation::landscape, NULL, &call->episode.episodeImage.path, &call->episode.episodeImage.width, &call->episode.episodeImage.height);

// more not episode related data
        call->actors = movieOrTv->GetActors();
        movieOrTv->getSingleImage(eImageLevel::seasonMovie, eOrientation::portrait, NULL, &call->seasonPoster.path, &call->seasonPoster.width, &call->seasonPoster.height);
        call->posters = movieOrTv->getImages(eOrientation::portrait);
        call->banners = movieOrTv->getBanners();
        call->fanarts = movieOrTv->getImages(eOrientation::landscape);

        return true;
    }

    if (strcmp(Id, "GetMovie") == 0) {
        if (Data == NULL) return true;
        cMovie* call = (cMovie*) Data;
        cMovieOrTv *movieOrTv = NULL;
        {
          cTVScraperLastMovieLock l;
          if (call->movieId != m_movie_tv_id) return true; // we assume that the data request is outdated
          if (call->movieId == 0 || m_movie_tv_id == 0) {
            esyslog("tvscraper: ERROR calling vdr service interface \"GetMovie\", call->movieId == 0 || movieOrTv == 0");
            call->title = "Error calling VDR service interface GetMovie";
            return true;
          }
          if (m_season_number != -100) {
            esyslog("tvscraper: ERROR calling vdr service interface \"GetMovie\", m_season_number != -100");
            call->title = "Error calling VDR service interface GetMovie";
            return true;
          }
          movieOrTv = new cMovieMoviedb(db, m_movie_tv_id);
        }

        int collection_id;
        if (!db->GetMovie(movieOrTv->dbID(), call->title, call->originalTitle, call->tagline, call->overview, call->adult, collection_id, call->collectionName, call->budget, call->revenue, call->genres, call->homepage, call->releaseDate, call->runtime, call->popularity, call->voteAverage)) {
            esyslog("tvscraper: ERROR calling vdr service interface \"GetMovie\", Id = %i not found", movieOrTv->dbID());
            call->title = "Error calling VDR service interface GetMovie";
            return true;
        }

        cImageLevelsInt level(eImageLevel::seasonMovie, eImageLevel::tvShowCollection);
        movieOrTv->getSingleImageBestL(level, eOrientation::portrait, NULL, &call->poster.path, &call->poster.width, &call->poster.height);
        movieOrTv->getSingleImageBestL(level, eOrientation::landscape, NULL, &call->fanart.path, &call->fanart.width, &call->fanart.height);
        movieOrTv->getSingleImage(eImageLevel::tvShowCollection, eOrientation::portrait,  NULL, &call->collectionPoster.path, &call->collectionPoster.width, &call->collectionPoster.height);
        movieOrTv->getSingleImage(eImageLevel::tvShowCollection, eOrientation::landscape, NULL, &call->collectionFanart.path, &call->collectionFanart.width, &call->collectionFanart.height);
        call->actors = movieOrTv->GetActors();

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
// always get poster
// for TV shows: return also banner, if available
        if (Data == NULL) return true;
        ScraperGetPosterBannerV2* call = (ScraperGetPosterBannerV2*) Data;
        cMovieOrTv *movieOrTv = GetMovieOrTv(call->event, call->recording);
        if (!movieOrTv) { call->type = tNone; return true;}
        call->type = movieOrTv->getType();
// get poster. No fallback. If there is no poster, leave GetPosterBannerV2->banner empty
        movieOrTv->getSingleImageBestLO(
          cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection),
          cOrientationsInt(eOrientation::portrait),
          NULL, &call->poster.path, &call->poster.width, &call->poster.height);
// get banner. If no image in banner format available, return landscape image
        movieOrTv->getSingleImageBestLO(
          cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection),
          cOrientationsInt(eOrientation::banner, eOrientation::landscape),
          NULL, &call->banner.path, &call->banner.width, &call->banner.height);
        delete movieOrTv;
        return true;
    }

    if (strcmp(Id, "GetPoster") == 0) {
        if (Data == NULL) return true;
        ScraperGetPoster* call = (ScraperGetPoster*) Data;
        cMovieOrTv *movieOrTv = GetMovieOrTv(call->event, call->recording);
        if (!movieOrTv) {
          call->poster = getEpgImage(call->event, call->recording, true);
        } else {
          movieOrTv->getSingleImageBestLO(
            cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection),
            cOrientationsInt(eOrientation::portrait, eOrientation::landscape, eOrientation::banner),
            NULL, &call->poster.path, &call->poster.width, &call->poster.height);
          delete movieOrTv;
          if (call->poster.path.empty() ) call->poster = getEpgImage(call->event, call->recording, true);
        }
        return true;
    }

    if (strcmp(Id, "GetPosterThumb") == 0) {
        if (Data == NULL) return true;
        ScraperGetPosterThumb* call = (ScraperGetPosterThumb*) Data;
        cMovieOrTv *movieOrTv = GetMovieOrTv(call->event, call->recording);
        if (!movieOrTv) {
          call->poster = getEpgImage(call->event, call->recording, true);
        } else {
          movieOrTv->getSingleImageBestLO(
            cImageLevelsInt(eImageLevel::seasonMovie, eImageLevel::tvShowCollection, eImageLevel::anySeasonCollection),
            cOrientationsInt(eOrientation::portrait, eOrientation::landscape, eOrientation::banner),
            NULL, &call->poster.path, &call->poster.width, &call->poster.height);
          delete movieOrTv;
          if (call->poster.path.empty() ) call->poster = getEpgImage(call->event, call->recording, true);
        }
        return true;
    }

    if (strcmp(Id, "GetAutoTimerReason") == 0) {
      if (Data == NULL) return true;
      cGetAutoTimerReason* call = (cGetAutoTimerReason*) Data;
      call->createdByTvscraper = false;
// check: aux available, xml tag "tvscraper" in aux?
      const char *aux = nullptr;
      if (call->timer) aux = call->timer->Aux();
      else if (call->recording_in && call->recording_in->Info() ) aux = call->recording_in->Info()->Aux();
      if (!aux) return true;
      cSv xml_tvscraper = partInXmlTag(aux, "tvscraper");
      if (xml_tvscraper.empty() ) return true;
// our timer
      call->createdByTvscraper = true;
// call->recordingName
      cSv xml_causedBy = partInXmlTag(xml_tvscraper, "causedBy");
      if (xml_causedBy.empty() ) xml_causedBy = tr("(name not available)");
      call->recordingName = xml_causedBy;
// call->reason
      call->reason = "";
      cSv xml_reason = partInXmlTag(xml_tvscraper, "reason");
      if (xml_reason == "collection") {
        std::string      collectionNameStr;
        cSv collectionName = partInXmlTag(xml_tvscraper, "collectionName");
        if (collectionName.empty() && call->recording_in) {
// try to get the collection name from the recording
          cMovieOrTv *movieOrTv = cMovieOrTv::getMovieOrTv(db, nullptr, call->recording_in);
          if (movieOrTv) {
            collectionNameStr = movieOrTv->getCollectionName();  // to avoid deletion of the string before we us it, the string is assigned to collectionNameStr (which is a string)
            collectionName = collectionNameStr;
            delete movieOrTv;
          }
        }
        if (collectionName.empty() ) collectionName = tr("(name not available)");
        stringAppendFormated(call->reason, tr("Complement collection %.*s, caused by recording"), (int)collectionName.length(), collectionName.data() );
      } else if (xml_reason == "TV show, missing episode") {
        cSv seriesName = partInXmlTag(xml_tvscraper, "seriesName");
        cSql stmt(db, "select tv_name from tv2 where tv_id = ?"); // define this here so returned cSv is still valid when we append to call->reason
        if (seriesName.empty() && call->recording_in) {
// try to get the series  name from the recording
          cMovieOrTv *movieOrTv = cMovieOrTv::getMovieOrTv(db, nullptr, call->recording_in);
          if (movieOrTv) {
            stmt.resetBindStep(movieOrTv->dbID() );
            if (stmt.readRow() ) seriesName = stmt.getStringView(0);
            delete movieOrTv;
          }
        }
        if (seriesName.empty() ) seriesName = tr("(name not available)");
        stringAppendFormated(call->reason, tr("Episode of series %.*s, caused by recording"), (int)seriesName.length(), seriesName.data() );
      } else {
        call->reason.append(tr("Improve recording"));
      }
      if (call->requestRecording) call->recording = recordingFromAux(aux);
      else {
        call->recording = nullptr;
        call->reason.append(" ");
        call->reason.append(xml_causedBy);
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

    if (strcmp(Id, "GetEnvironment") == 0) {
        if (Data == NULL) return true;
        cEnvironment* call = (cEnvironment*) Data;
        call->basePath = config.GetBaseDir();
        call->seriesPath = config.GetBaseDirSeries();
        call->moviesPath = config.GetBaseDirMovies();
        return true;
    }

  return false;
}

const char **cPluginTvscraper::SVDRPHelpPages(void) {
 static const char *HelpPages[] = {
    "SCRE <recording>\n"
    "    Scrap one recording.\n"
    "    Use full path name to the recording directory, including the video directory and the actual '*.rec'.\n"
    "SCRE\n"
    "    Scrap all recordings.\n"
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
    if (Option && *Option) {
      LOCK_RECORDINGS_READ;
      const cRecording *rec = Recordings->GetByName(Option);
      if (!rec) return cString::sprintf("Recording %s not found, use full path name to the recording directory, including the video directory and the actual '*.rec'", Option);
    }

    workerThread->InitVideoDirScan(Option);
    if (Option && *Option) return cString::sprintf("Scraping %s started", Option);
    else return cString("Scraping Video Directory started");
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

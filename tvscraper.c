/*
 * tvscraper.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */
#include "services.h"
#include <map>
// BEGIN OF deprecated !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// removed from services.h, please dont use
// still here, so old plugins (with the old interface) continue to work for some time ...
// everything after this line is deprecated !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// use cGetScraperVideo / cScraperVideo instead
enum class eVideoType {
  none = 0,
  movie = 1,
  tvShow = 2,
  musicVideo = 3,
};

class cEpisode2 {
public:
    int number;
    int season;
    int absoluteNumber;
    std::string name;
    std::string firstAired;
    std::vector<cActor> guestStars;
    std::string overview;
    float vote_average;
    int vote_count;
    cTvMedia episodeImage;
    std::string episodeImageUrl;
    std::vector<std::string> director;
    std::vector<std::string> writer;
    std::string IMDB_ID;
};

// Data structure for Kodi
// see https://alwinesch.github.io/group__cpp__kodi__addon__pvr___defs__epg___p_v_r_e_p_g_tag.html
// see https://alwinesch.github.io/group__cpp__kodi__addon__pvr___defs___recording___p_v_r_recording.html
// Series link: If set for an epg-based timer rule, matching events will be found by checking strSeriesLink instead of strTitle (and bFullTextEpgSearch)  see https://github.com/xbmc/xbmc/pull/12609/files
// tag.SetFlags(PVR_RECORDING_FLAG_IS_SERIES);
// SetSeriesNumber (this is the season number)

//Data structure for full information
class cScraperMovieOrTv {
public:
//IN
    const cEvent *event;             // must be NULL for recordings ; provide data for this event
    const cRecording *recording;     // must be NULL for events     ; or for this recording
    bool httpImagePaths;             // if true, provide http paths to images
    bool media;                      // if true, provide local filenames for media
//OUT
// Note: tvscraper will clear all output parameters, so you don't have to do this before calling tvscraper
    bool found;
    bool movie;
    std::string title;
    std::string originalTitle;
    std::string tagline;
    std::string overview;
    std::vector<std::string> genres;
    std::string homepage;
    std::string releaseDate;  // for TV shows: firstAired
    bool adult;
    std::vector<int> runtimes;
    float popularity;
    float voteAverage;
    int voteCount;
    std::vector<std::string> productionCountries;
    std::vector<cActor> actors;
    std::string IMDB_ID;
    std::string posterUrl;   // only if httpImagePaths == true
    std::string fanartUrl;   // only if httpImagePaths == true
    std::vector<cTvMedia> posters;
    std::vector<cTvMedia> banners;
    std::vector<cTvMedia> fanarts;
// only for movies
    int budget;
    int revenue;
    int collectionId;
    std::string collectionName;
    cTvMedia collectionPoster;
    cTvMedia collectionFanart;
    std::vector<std::string> director;
    std::vector<std::string> writer;
// only for TV Shows
    std::string status;
    std::vector<std::string> networks;
    std::vector<std::string> createdBy;
// episode related
    bool episodeFound;
    cTvMedia seasonPoster;
    cEpisode2 episode;
};

//Data structure for live, overview information for each recording / event
// to uniquely identify a recording/event:
// movie + dbid + seasonNumber + episodeNumber (for movies, only dbid is required)
// note: if nothing was found, m_videoType = videoType::none will be returned
class cGetScraperOverview {
public:
  cGetScraperOverview (const cEvent *event = NULL, const cRecording *recording = NULL, std::string *title = NULL, std::string *episodeName = NULL, std::string *IMDB_ID = NULL, cTvMedia *image = NULL, cImageLevels imageLevels = cImageLevels(), cOrientations imageOrientations = cOrientations(), std::string *releaseDate = NULL, std::string *collectionName = NULL):
  m_event(event),
  m_recording(recording),
  m_title(title),
  m_episodeName(episodeName),
  m_IMDB_ID(IMDB_ID),
  m_image(image),
  m_imageLevels(imageLevels),
  m_imageOrientations(imageOrientations),
  m_releaseDate(releaseDate),
  m_collectionName(collectionName)
  {
  }

  bool call(cPlugin *pScraper) {
    m_videoType = eVideoType::none;
    if (!pScraper) return false;
    else return pScraper->Service("GetScraperOverview", this);
  }
//IN: Use constructor, setRequestedImageFormat and setRequestedImageLevel to set these values
  const cEvent *m_event;             // must be NULL for recordings ; provide data for this event
  const cRecording *m_recording;     // must be NULL for events     ; or for this recording
  std::string *m_title;
  std::string *m_episodeName;
  std::string *m_IMDB_ID;
  cTvMedia *m_image;
  cImageLevels m_imageLevels;
  cOrientations m_imageOrientations;
  std::string *m_releaseDate;
  std::string *m_collectionName;
//OUT
// Note: tvscraper will clear all output parameters, so you don't have to do this before calling tvscraper
  eVideoType m_videoType;
  int m_dbid;
  int m_runtime;
// only for movies
  int m_collectionId;
// only for TV shows
  int m_episodeNumber;
  int m_seasonNumber;
};

inline bool operator< (const tChannelID &c1, const tChannelID &c2) {
  if (c1.Source() != c2.Source() ) return c1.Source() < c2.Source();
  if (c1.Nid() != c2.Nid() ) return c1.Nid() < c2.Nid();
  if (c1.Tid() != c2.Tid() ) return c1.Tid() < c2.Tid();
  if (c1.Sid() != c2.Sid() ) return c1.Sid() < c2.Sid();
  return c1.Rid() < c2.Rid();
}
class cGetChannelLanguages {
public:
  cGetChannelLanguages() {}
  std::map<tChannelID, int> m_channelLanguages; // if a channel is not in this map, it has the default language
  int m_defaultLanguage;
  std::map<int, std::string> m_channelNames;
  bool call(cPlugin *pScraper) {
    if (!pScraper) return false;
    else return pScraper->Service("GetChannelLanguages", this);
  }
};

class cGetChannelHD {
public:
  cGetChannelHD() {}
  std::map<tChannelID, int> m_channelHD; // if a channel is not in this map, it is SD
// currently, only 0 (SD) and 1 (HD) are supported. More might be added
// note: if this map is empty, the SD/HD information was not maitained
  bool call(cPlugin *pScraper) {
    if (!pScraper) return false;
    else return pScraper->Service("GetChannelHD", this);
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
#include "tools/largeString.h"
#include "searchResultTvMovie.h"
#include "searchResultTvMovie.c"
#include "config.h"
cTVScraperConfig config;
#include "tools/stringhelpers.c"
#include "channelmap.c"
#include "eventOrRec.h"
#include "tvscraperdb.h"
#include "autoTimers.h"
#include "extEpg.h"
#include "config.c"
#include "tools/jsonHelpers.c"
#include "tools/curlfuncs.cpp"
#include "tools/filesystem.c"
#include "tools/fuzzy.c"
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
#include "extEpg.c"
#include "services.c"

static const char *VERSION        = "1.1.10";
static const char *DESCRIPTION    = "Scraping movie and series info";

class cPluginTvscraper : public cPlugin {
private:
    bool cacheDirSet;
    cTVScraperDB *db;
    cTVScraperWorker *workerThread;
    cOverRides *overrides;
    cMovieOrTv *lastMovieOrTv = NULL;
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
    if (strcmp(Id, "GetScraperVideo") == 0) {
        if (Data == NULL) return true;
        cGetScraperVideo* call = (cGetScraperVideo*) Data;
        call->m_scraperVideo = std::make_unique<cScraperVideoImp>(call->m_event, call->m_recording, db);
        return true;
    }
    if (strcmp(Id, "GetScraperOverview") == 0) {
        if (Data == NULL) return true;
        cGetScraperOverview* call = (cGetScraperOverview*) Data;
        cMovieOrTv *movieOrTv = GetMovieOrTv(call->m_event, call->m_recording, &call->m_runtime);
        if (!movieOrTv) {
          call->m_videoType = eVideoType::none;
          if (call->m_image) *(call->m_image) = getEpgImage(call->m_event, call->m_recording);
          return true;
        }
        if (call->m_runtime < 0) call->m_runtime = 0;
        movieOrTv->getScraperOverview(call);
        delete movieOrTv;
        if (call->m_image && call->m_image->path.empty() ) *(call->m_image) = getEpgImage(call->m_event, call->m_recording);
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

    if (strcmp(Id, "GetScraperUpdateTimes") == 0) {
      if (Data == NULL) return true;
      cGetScraperUpdateTimes* call = (cGetScraperUpdateTimes*) Data;
      call->m_EPG_UpdateTime = LastModifiedTime(config.GetEPG_UpdateFileName().c_str() );
      call->m_recordingsUpdateTime = LastModifiedTime(config.GetRecordingsUpdateFileName().c_str() );
      return true;
    }
    if (strcmp(Id, "GetChannelLanguages") == 0) {
      if (Data == NULL) return true;
      cGetChannelLanguages* call = (cGetChannelLanguages*) Data;
      call->m_channelLanguages = config.GetChannelLanguages();
      call->m_defaultLanguage = config.GetDefaultLanguage()->m_id;
      call->m_channelNames.clear();
      for (const auto &lang: config.m_languages) call->m_channelNames.insert({lang.m_id, string(lang.m_name)});
      return true;
    }
    if (strcmp(Id, "GetChannelHD") == 0) {
      if (Data == NULL) return true;
      cGetChannelHD* call = (cGetChannelHD*) Data;
      call->m_channelHD = config.GetChannelHD();
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

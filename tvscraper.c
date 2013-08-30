/*
 * tvscraper.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */
#include <getopt.h>
#include <vdr/plugin.h>
#include "tools/curlfuncs.cpp"
#include "tools/filesystem.c"
#include "tools/fuzzy.c"
#include "tools/splitstring.c"
#include "tools/stringhelpers.c"
#include "config.c"
cTVScraperConfig config;
#include "tvscraperdb.c"
#include "thetvdbscraper/tvdbmirrors.c"
#include "thetvdbscraper/tvdbseries.c"
#include "thetvdbscraper/tvdbmedia.c"
#include "thetvdbscraper/tvdbactors.c"
#include "thetvdbscraper/thetvdbscraper.c"
#include "themoviedbscraper/moviedbmovie.c"
#include "themoviedbscraper/moviedbactors.c"
#include "themoviedbscraper/themoviedbscraper.c"
#include "worker.c"
#include "services.h"
#include "imageserver.c"
#include "setup.c"

static const char *VERSION        = "0.0.4";
static const char *DESCRIPTION    = "Scraping movie and series info";
static const char *MAINMENUENTRY  = "TV Scraper";

class cPluginTvscraper : public cPlugin {
private:
    bool cacheDirSet;
    cTVScraperDB *db;
    cTVScraperWorker *workerThread;
    cImageServer *imageServer;
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
    imageServer = new cImageServer(db);
    workerThread = new cTVScraperWorker(db);
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

bool cPluginTvscraper::Service(const char *Id, void *Data) {
    if (strcmp(Id, "TVScraperGetPosterOrBanner") == 0) {
        if (Data == NULL)
            return false;
        TVScraperGetPosterOrBanner* call = (TVScraperGetPosterOrBanner*) Data;
        if (!call->event)
            return false;
        scrapType type = imageServer->GetScrapType(call->event);
        if (type == scrapSeries)
            call->type = typeSeries;
        else if (type == scrapMovie)
            call->type = typeMovie;
        else
            call->type = typeNone;
        int id = imageServer->GetID(call->event->EventID(), type, false);
        if (id > 0) {
            call->media = imageServer->GetPosterOrBanner(id, type);
            return true;
        }
        return false;
    }
    if (strcmp(Id, "TVScraperGetPoster") == 0) {
        if (Data == NULL)
            return false;
        TVScraperGetPoster* call = (TVScraperGetPoster*) Data;
        if (!call->event)
            return false;
        scrapType type = imageServer->GetScrapType(call->event);
        int id = imageServer->GetID(call->event->EventID(), type, call->isRecording);
        if (id > 0) {
            call->media = imageServer->GetPoster(id, type);
            return true;
        }
        return false;
    }
    if (strcmp(Id, "TVScraperGetFullInformation") == 0) {
        if (Data == NULL)
            return false;
        TVScraperGetFullInformation* call = (TVScraperGetFullInformation*) Data;
        if (!call->event)
            return false;
        
        scrapType type = imageServer->GetScrapType(call->event);
        int id = imageServer->GetID(call->event->EventID(), type, call->isRecording);

        if (id == 0)
            return false;
        
        if (type == scrapSeries) {
            call->type = typeSeries;
            call->banner = imageServer->GetBanner(id);
        } else if (type == scrapMovie) {
            call->type = typeMovie;
        } else {
            call->type = typeNone;
        }
        call->posters = imageServer->GetPosters(id, type);
        call->fanart = imageServer->GetFanart(id, type);
        call->actors = imageServer->GetActors(id, type);
        call->description = imageServer->GetDescription(id, type);
        return true;
    }
    return false;
}

const char **cPluginTvscraper::SVDRPHelpPages(void) {
    return NULL;
}

cString cPluginTvscraper::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode) {
    return NULL;
}

VDRPLUGINCREATOR(cPluginTvscraper);

# vdr-plugin-tvscraper

This is a "plugin" for the Video Disk Recorder (VDR).

- Written by:                   Stefan Braun <louis.braun@gmx.de>
- Re-written and maintained by: Markus Ehrnsperger (MarkusE @ vdr-portal.de)
- Latest version available at:  https://github.com/MarkusEh/vdr-plugin-tvscraper

Movie information provided by [TMDB](https://www.themoviedb.org/).
Series / TV show information provided by TheTVDB.com. [Please consider supporting them](https://u10505776.ct.sendgrid.net/ls/click?upn=xMYYCP13hVd-2BZPpbVcMPHeLXfv-2BPRdIsKm2qSeirIHi7kH9am8IxD-2BavFbeqGqIXIcjH_INgc0CXRkIvGU-2BJ1W6HLAynNbR0UBoMb2tkpDdezO3QRj-2FQPQAMrtJKVcB0N7eSvbYpjQTDNOQBf2pb94uDgGr0-2BXyXFk7Oyfva30BASYCRLvtRQyi5eOCAbH8fon7UETlPydobeLA3Recu9OsXol8c7Ng4pDAsH6KsFF8CH7HQoVxivnpKrQW2v4ek7U-2BYlWmH31o9Koke3vq-2FDsQ0P-2BiKLECU3LgttsntUcN8fnUs-3D).

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
See the file COPYING for more information.

Description
-----------

TVScraper runs in the background and collects metadata (posters,
banners, fanart, actor thumbs and roles, descriptions) for all
available EPG events on selectable channels and for recordings.
Additionally the plugin provides the collected metadata via the VDR
service interface to other plugins which deal with EPG information.

TVScraper uses the thetvdb.com API for collecting series metadata and
themoviedb.org API for movies. Check the websites of both services for
the terms of use.

Important: To avoid unnecessary traffic, only activate these channels
to be scraped which are reasonable. After plugin installation all
channels are deactivated by default, so please consider this point when
you activate the channels you are interested in ;)

Additionally you are invited to contribute to the used web services with
providing missing data for your favorite movies and series.

Additional documentation is available in the
[wiki](https://github.com/MarkusEh/vdr-plugin-tvscraper/wiki)
and it's sub-pages.

Requirements
------------

- vdr 2.4.0 or later. Recommended: vdr 2.6.0 or later
- libsqlite3
- libcurl
- libjansson
- gcc v8 or later. gcc must support -std=c++17.
Note: if your version of GCC does not support -std=c++17, you can use -std=c++1z instead of -std=c++17. But:
  - If you use -std=c++1z and see errors indicating a missing include (filesystem), you can change #include \<filesystem\> to #include \<experimental/filesystem\>
  - There might be still compilation errors, because features in \<filesystem\> might be missing in \<experimental/filesystem\> . In this case, you have to upgrade your gcc compiler to GCC v8 or later
  - See also:
    - https://stackoverflow.com/questions/60336940/g-error-unrecognized-std-c17-what-is-g-version-and-how-to-install#60340063
    - https://www.vdr-portal.de/forum/index.php?thread/135111-announce-vdr-plugin-tvscraper-1-0-0/&postID=1351517#post1351517
    - https://www.vdr-portal.de/forum/index.php?thread/135453-kann-tvscraper-auf-opensuse-leap-15-4-nicht-%C3%BCbersetzen/&postID=1355981#post1355981


- If you use TVGuide: Version 1.3.6+ is required
- If you use SkinNopacity: Version 1.1.12+ is required

Installation and configuration
------------------------------

Just install the plugin depending on your used distribution. During VDR
startup the plugin base directory can be set with the following option:

-d \<CACHEDIR\>, --dir=\<CACHEDIR\> Set directory where database and images
                                are stored

If no directory is provided, the plugin uses VDRCACHEDIR as default.
Please care about that the user who runs VDR has full read/write access
to this directory, otherwise the plugin will not start.

If a client can access this directory (because it is mounted), and the
parameter

--readOnlyClient

is provided, the client needs only read access, and will not scrape.
This feature is experimental, and might not work in all situations,
e.g. for very old recordings with missing data in the info/info.vdr file.
Make sure that the same version of tvscraper is running on client and server.
In some situations,
- sudo systemctl enable rpc-statd and
- sudo systemctl start rpc-statd
on the server might help

As already mentioned, after first installations no channels are activated
to be scraped. Please configure these channels in the plugin setup menu.
(not required in --readOnlyClient mode).
Additionally you can trigger that your already existing recordings are
scraped, so that also for this recordings metadata is available.

With a "make install" the file "override.conf" which provides the
possibility to define scraping behaviour manually (see description
below) is created in \<PLGCFGDIR\> (for debian based distributions,
this should be /var/lib/vdr/plugins/tvscraper).
An existing override.conf will not be overwritten.

The plugins uses a sqlite3 database to store the necessary information.
If /dev/shm/ is available, the database is kept in memory during runtime
which improves performance. In the configured plugin basedir only a
persistant backup of the database is stored then. If /dev/shm/ is not
available, only the database file in the plugin base directory is used.


External EPG
------------
There are solutions to import external EPG to vdr, mixing the
EIT EPG information (comming from the station) with the information
of the external EPG provider, and using own IDs for the EPG events.
See, for example, [epgd](https://github.com/horchi/vdr-epg-daemon).
While this works good over all, it breaks VDR functions relying
on data from EIT, like VPS.

To ensure the complete VDR functionality relying on EIT (including VPS),
another approach can be used: Leave VDR as is in almost all aspects,
and only replace the description of the EPG events with infomation
from an external EPG provider.
This is implemented as proof of concept in extEpg.c

To try it out,
- Note: This is only a proof of concept and not intended for productive use.
For this feature, only information which is publicly available
on the internet is used.
Make sure to use this feature only if this is allowed by the laws and
regulations applicable to you.
- Disable all other plugins providing external epg, like vdr-plugin-epg2vdr.
- Copy the channelmap.conf file in the format
defined by [epgd](https://github.com/horchi/vdr-epg-daemon) to \<PLGCFGDIR\>
- Note: even if the format of channelmap.conf is identical, this does not
mean that all [epgd](https://github.com/horchi/vdr-epg-daemon) features
are available. For example, this proof of concept is only available
for tvsp. All lines in channelmap.conf not starting with "tvsp:" are ignored.
- Restart vdr. That should by all.
- To disable this feature, remove channelmap.conf from \<PLGCFGDIR\>
directoy and restart vdr


Usage
-----

After the initial configuration the plugin runs completely independent in
the background, you don't have to care about anything. The plugins checks
at least every 24 hours for new EPG events and collects the metadata for
these events automatically.

After each run the plugin performs a cleanup, all images for movies which
are not available in the current EPG are deleted. Actors thumbs
are kept to avoid unnecessary traffic for the web services, because the
propability that this data is needed in the future again is rather high.

If a running recording is detected, the plugin marks the corresponding movie
meta data so that the information for this movie will be kept permanentely.

Usage of override.conf: even if tvscraper tries to do everything correct on
it's own, in some cases scraping delivers wrong results. Some EPG Events are
not reasonable to scrape, because they reoccur constantly but deliver wrong
results everytime, or tvscraper searchs for a movie instead of a series
(for instance german "Heute"). In such cases it is possible to use
\<PLGCFGDIR\>/override.conf to adjust the scraping behaviour. Each line in
this file has to start either with "ignore", "settype", "substitute" or
"ignorePath":

- Ignore specific EPG Events or recordings: just create a line in the format
```
  ignore;string
  to ignore "string".
```
- Set scrape type for specific EPG Event or recording:
```
  settype;string;type
  "string" defines the name of the event or recording to set the type manually,
  "type" can be either "series" or "movie"
```
- Substitute Search String:
```
  substitute;string;substitution
  "string" is replaced by "substitution" in every search.
```
- Ignore all recordings in a deditcatd directory:
```
  ignorePath;string
  "string" can be any substring of a recording path, e.g. "music/"
```

Service Interface
-----------------

Other Plugins can request information about meta data from tvscraper via
a call to the provided service interface.

In general each call expects a pointer to a cEvent or cRecording
object as input variable inside the struct passed to the call.

As output variables tvscraper provides media info via the "cTvMedia" struct:

```
class cTvMedia {
public:
    std::string path;
    int width;
    int height;
};
```

and actors information via the "cActor" struct:

```
class cActor {
    std::string name;
    std::string role;
    cTvMedia actorThumb;
};
```

Some calls offered by the service interface (see services.h for complete list):


- GetPoster

  Retreive poster for specified event or recording.

```
// Data structure for service "GetPoster"
class ScraperGetPoster {
public:
// in
    const cEvent *event;             // check type for this event
    const cRecording *recording;     // or for this recording
//out
    cTvMedia poster;
};
```

    Example:

```
    static cPlugin *pTVScraper = cPluginManager::GetPlugin("tvscraper");
    if (pTVScraper) {
      ScraperGetPoster call;
      call.event = Event;                 //provide Event here
      call.recording = Recording;         //if recording, otherwise NULL
      if (pTVScraper->Service("GetPoster", &call)) {
          ... further processing of call.media
      }
    }
```

- call GetEventType, GetSeries & GetMovie

```
// Data structure for service "GetEventType"
class ScraperGetEventType {
public:
// in
    const cEvent *event;             // check type for this event
    const cRecording *recording;     // or for this recording
//out
    tvType type;                         //typeSeries or typeMovie
    int movieId;   // note: Please ignore, nothing usefull
    int seriesId;  // note: Please ignore, nothing usefull
    int episodeId; // note: Please ignore, nothing usefull
};
```

Example: (see services.h for data structures of cSeries & cMovie, and for enum tvType with tSeries & tMovie)

```
  static cPlugin *pTVScraper = cPluginManager::GetPlugin("tvscraper");
  if (pTVScraper) {
    ScraperGetEventType call;
    call.event = Event;                 //provide Event here
    call.recording = Recording;         //if recording, otherwise NULL
    if (pTVScraper->Service("GetEventType", &call)) {
      if (call.type == tSeries) {
        cSeries call_series;
        call_series.seriesId = call.seriesId; // this will be ignored, must not be 0. Data from last call to GetEventType will be used
        if (pTVScraper->Service("GetSeries", &call_series)) {
        ... further processing ...
      } else if (call.type == tMovie) {
        cMovie call_movie;
        call_movie.movieId = call.movieId; // this will be ignored, must not be 0. Data from last call to GetEventType will be used
        if (pTVScraper->Service("GetMovie", &call_movie)) {
        ... further processing ...
      }
    }
```

See services.h for more available methods

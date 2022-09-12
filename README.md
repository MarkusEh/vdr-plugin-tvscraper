# vdr-plugin-tvscraper

This is a "plugin" for the Video Disk Recorder (VDR).

Written by:                   Stefan Braun <louis.braun@gmx.de>
Re-written and maintained by: Markus Ehrnsperger (MarkusE @ vdr-portal.de)

Latest version available at:
https://github.com/MarkusEh/vdr-plugin-tvscraper

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
to be scrapped which are reasonable. After plugin installation all
channels are deactivated by default, so please consider this point when
you activate the channels you are interested in ;)

Additionally you are invited to contribute to the used web services with
providing missing data for your favorite movies and series.

See also https://github.com/MarkusEh/vdr-plugin-tvscraper/wiki/Auto-Timers-to-Improve-and-Enhance-Existing-Recordings
and https://github.com/MarkusEh/vdr-plugin-tvscraper/wiki/How-it-works

Requirements
------------

To run the plugin the following libaries have to be installed:
- libsqlite3
- libcurl
- libjansson
- gcc must support -std=c++17  (for GCC v5, v6, v7: -std=c++1z instead of -std=c++17 might help. Also, a patch mmight help, see https://www.vdr-portal.de/forum/index.php?thread/135111-announce-vdr-plugin-tvscraper-1-0-0/&postID=1351517#post1351517)
- vdr 2.4.0 or later

If you use TVGuide: Version 1.3.6+ is required
If you use SkinNopacity: Version 1.1.12+ is required

Installation and configuration
------------------------------

Just install the plugin depending on your used distribution. During VDR
startup the plugin base directory can be set with the following option:

-d <CACHEDIR>, --dir=<CACHEDIR> Set directory where database and images
                                are stored

If no directory is provided, the plugin uses VDRCACHEDIR as default.
Please care about that the user who runs VDR has full read/write access
to this directory, otherwise the plugin will not start.

If a client can access this directory (because it is mounted), and the
parameter

--readOnlyClient

is provided, the client needs only read access, and will not scrap.
This feature is experimental, and might not work in all situations,
e.g. for very old recordings with missing data in the info/info.vdr file.
Make sure that the same version of tvscraper is running on client and server.

As already mentioned, after first installations no channels are activated
to be scrapped. Please configure these channels in the plugin setup menu.
(not required in --readOnlyClient mode).
Additionally you can trigger that your already existing recordings are
scrapped, so that also for this recordings metadata is available.

With a "make install" the file "override.conf" which provides the
possibility to define scraping behaviour manually (see description
below) is created in <PLGCFGDIR>. An existing override.conf will
not be overwritten.

The plugins uses a sqlite3 database to store the necessary information.
If /dev/shm/ is available, the database is kept in memory during runtime
which improves performance. In the configured plugin basedir only a
persistant backup of the database is stored then. If /dev/shm/ is not
available, only the database file in the plugin base directory is used.

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
not reasonable to scrap, because they reoccur constantly but deliver wrong
results everytime, or tvscraper searchs for a movie instead of a series
(for instance german "Heute"). In such cases it is possible to use
<PLGCFGDIR>/override.conf to adjust the scraping behaviour. Each line in
this file has to start either with "ignore", "settype", "substitute" or
"ignorePath":

- Ignore specific EPG Events or recordings: just create a line in the format
  ignore;string
  to ignore "string".
- Set scrap type for specific EPG Event or recording:
  settype;string;type
  "string" defines the name of the event or recording to set the type manually,
  "type" can be either "series" or "movie"
- Substitute Search String:
  substitute;string;substitution
  "string" is replaced by "substitution" in every search.
- Ignore all recordings in a deditcatd directory:
  ignorePath;string
  "string" can be any substring of a recording path, e.g. "music/"

Service Interface
-----------------

Other Plugins can request information about meta data from tvscraper via
a call to the provided service interface.

In general each call expects a pointer to a cEvent or cRecording
object as input variable inside the struct passed to the call.

As output variables tvscraper provides media info via the "cTvMedia" struct:

class cTvMedia {
public:
    std::string path;
    int width;
    int height;
};


and actors information via the "cActor" struct:

class cActor {
    std::string name;
    std::string role;
    cTvMedia actorThumb;
};

The service interface offers the following calls:

- GetPosterBanner

  With this call, a poster for a movie or a banner for a series which belongs
  to a specific event can be retreived.

// Data structure for service "GetPosterBanner"
class ScraperGetPosterBanner {
public:
// in
    const cEvent *event;             // check type for this event
//out
    tvType type;                         //typeSeries or typeMovie
    cTvMedia poster;
    cTvMedia banner;
};


  Example:

  static cPlugin *pTVScraper = cPluginManager::GetPlugin("tvscraper");
  if (pTVScraper) {
    ScraperGetPosterBanner call;
    call.event = Event;			//provide Event here
    if (pTVScraper->Service("GetPosterBanner", &call)) {
    	... further processing of call.media and call.type
    }
  }

- GetPoster

  Retreive poster for specified event or recording.

  // Data structure for service "GetPoster"
class ScraperGetPoster {
public:
// in
    const cEvent *event;             // check type for this event
    const cRecording *recording;     // or for this recording
//out
    cTvMedia poster;
};

    Example:

    static cPlugin *pTVScraper = cPluginManager::GetPlugin("tvscraper");
    if (pTVScraper) {
      ScraperGetPoster call;
      call.event = Event;                 //provide Event here
      call.recording = Recording;         //if recording, otherwise NULL
      if (pTVScraper->Service("GetPoster", &call)) {
          ... further processing of call.media
      }
    }

- call GetEventType, GetSeries & GetMovie

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

Example: (see services.h for data structures of cSeries & cMovie, and for enum tvType with tSeries & tMovie)

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

see services.h for more available methods

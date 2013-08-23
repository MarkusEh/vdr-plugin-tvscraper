enum tvMediaType {
    typeSeries,
    typeMovie,
    typeNone,
};

struct tvMedia {
    std::string path;
    int width;
    int height;
};

struct tvActor {
    std::string name;
    std::string role;
    tvMedia thumb;
};

// Data structure for service "TVScraper-GetPosterOrBanner"
struct TVScraperGetPosterOrBanner
{
// in
    const cEvent *event;             // search image for this event 
//out
    tvMediaType type;                //typeSeries or typeMovie
    tvMedia media;                   //banner or poster
};

// Data structure for service "TVScraper-GetPoster"
struct TVScraperGetPoster
{
// in
    const cEvent *event;             // search image for this event 
    bool isRecording;                // search in current EPG or recordings
//out
    tvMedia media;                   //poster
};


/* Data structure for service "TVScraper-GetFullEPGInformation"
if type == typeMovie a poster and a fanart image is delivered
if type == typeSeries a banner and up to three posters and fanarts are delivered
*/
struct TVScraperGetFullInformation
{
// in
    const cEvent *event;             // search all media for this event
    bool isRecording;                // search in current EPG or recordings
//out
    tvMediaType type;
    tvMedia banner;
    std::vector<tvMedia> posters;
    std::vector<tvMedia> fanart;
    std::vector<tvActor> actors;
    std::string description;
};
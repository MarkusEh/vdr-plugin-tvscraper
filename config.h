#ifndef __TVSCRAPER_CONFIG_H
#define __TVSCRAPER_CONFIG_H
#include <string>
#include <vector>

using namespace std;

enum scrapType {
    scrapSeries,
    scrapMovie,
    scrapNone
};
struct sMovieOrTv {
    scrapType type;
    int id;
    int season;
    int episode;
    int year;
    int episodeSearchWithShorttext;
};

class cTVScraperConfig {
    private:
        string baseDir;
        string themoviedbSearchOption = "";
        bool readOnlyClient = false;
        vector<string> channels; 
    public:
        cTVScraperConfig();
        ~cTVScraperConfig();
        int enableDebug;
        void SetBaseDir(string dir) { baseDir = dir; };
        void SetThemoviedbSearchOption(const string &option) { themoviedbSearchOption = option; };
        void SetReadOnlyClient() { readOnlyClient = true; };
        string GetBaseDir(void) { return baseDir; };
        string GetThemoviedbSearchOption(void) { return themoviedbSearchOption; };
        bool GetReadOnlyClient() { return readOnlyClient; }
        void ClearChannels(void);
        void AddChannel(string channelID);
        bool ChannelActive(int channelNum);
        bool SetupParse(const char *Name, const char *Value);
        vector<string> GetChannels(void) { return channels; };
        void PrintChannels(void);
};

#endif //__TVSCRAPER_CONFIG_H

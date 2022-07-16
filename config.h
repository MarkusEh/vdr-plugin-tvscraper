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
        string themoviedbSearchOption = "";
        bool readOnlyClient = false;
        vector<string> channels; 
        string baseDir;
        int baseDirLen;
        string baseDirSeries = "";
        string baseDirMovies = "";
        string baseDirMovieActors = "";
        string baseDirMovieCollections = "";
        string baseDirMovieTv = "";
    public:
        cTVScraperConfig();
        ~cTVScraperConfig();
        int enableDebug;
        void SetThemoviedbSearchOption(const string &option) { themoviedbSearchOption = option; };
        void SetReadOnlyClient() { readOnlyClient = true; };
        void SetBaseDir(const string &dir);
        const string &GetBaseDir(void) const { return baseDir; };
        int GetBaseDirLen(void) const { return baseDirLen; };
        const string &GetBaseDirSeries(void) const { return baseDirSeries; };
        const string &GetBaseDirMovies(void) const { return baseDirMovies; };
        const string &GetBaseDirMovieActors(void) const { return baseDirMovieActors; };
        const string &GetBaseDirMovieCollections(void) const { return baseDirMovieCollections; };
        const string &GetBaseDirMovieTv(void) const { return baseDirMovieTv; };
        const string &GetThemoviedbSearchOption(void) const { return themoviedbSearchOption; };
        bool GetReadOnlyClient() const { return readOnlyClient; }
        void ClearChannels(void);
        void AddChannel(string channelID);
        bool ChannelActive(int channelNum);
        bool SetupParse(const char *Name, const char *Value);
        vector<string> GetChannels(void) { return channels; };
        void PrintChannels(void);
};

#endif //__TVSCRAPER_CONFIG_H

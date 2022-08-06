#ifndef __TVSCRAPER_CONFIG_H
#define __TVSCRAPER_CONFIG_H
#include <string>
#include <vector>
#include <set>

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
        set<string> channels; 
        set<string> hd_channels; 
        set<string> m_excludedRecordingFolders;
        set<int> m_TV_Shows;
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
        int enableAutoTimers;
        void Initialize();
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
        void ClearChannels(bool hd);
        void ClearExcludedRecordingFolders() { m_excludedRecordingFolders.clear(); }
        void ClearTV_Shows() { m_TV_Shows.clear(); }
        void AddChannel(const string &channelID, bool hd);
        void AddExcludedRecordingFolder(const string &recordingFolder) { m_excludedRecordingFolders.insert(recordingFolder); }
        void AddTV_Show(int TV_Show) { m_TV_Shows.insert(TV_Show); }
        bool ChannelActive(const cChannel *channel) const { return channels.find(*(channel->GetChannelID().ToString())) != channels.end(); }
        bool ChannelHD(const cChannel *channel) const { return hd_channels.find(*(channel->GetChannelID().ToString())) != hd_channels.end(); }
        bool recordingFolderSelected(const std::string &recordingFolder) const { return m_excludedRecordingFolders.find(recordingFolder) == m_excludedRecordingFolders.end(); }
        bool TV_ShowSelected(int TV_Show) const { return m_TV_Shows.find(TV_Show) != m_TV_Shows.end(); }
        bool SetupParse(const char *Name, const char *Value);
        const set<string> &GetChannels(void) const { return channels; };
        const set<string> &GetHD_Channels(void) const { return hd_channels; };
        const set<string> &GetExcludedRecordingFolders(void) const { return m_excludedRecordingFolders; };
        const set<int> &GetTV_Shows(void) const { return m_TV_Shows; };
        void PrintChannels(void);
};

#endif //__TVSCRAPER_CONFIG_H

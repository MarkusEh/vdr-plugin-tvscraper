#ifndef __TVSCRAPER_AUTOTIMERS_H
#define __TVSCRAPER_AUTOTIMERS_H
#include<string>
#include<set>
#include<vector>

class cMovieOrTv;
class cMovieOrTvAT {
  public:
    cMovieOrTvAT(int v) {}
    cMovieOrTvAT(const tChannelID &channel_id, int movie_tv_id, int season_number, int episode_number)
      { m_movie_tv_id = movie_tv_id; m_season_number = season_number; m_episode_number = episode_number; m_hd = config.ChannelHD(channel_id); m_language = config.GetLanguage_n(channel_id); }
    cMovieOrTvAT(const tChannelID &channel_id, const cMovieOrTv *movieOrTv);
    cMovieOrTvAT(const cMovieOrTvAT *movieOrTvAT)
     { m_movie_tv_id = movieOrTvAT->m_movie_tv_id; m_season_number = movieOrTvAT->m_season_number; m_episode_number = movieOrTvAT->m_episode_number; m_language = movieOrTvAT->m_language; m_hd = movieOrTvAT->m_hd; }
    int m_movie_tv_id; // movie if season_number == -100. Otherwisse, tv
    int m_season_number;
    int m_episode_number;
    int m_language;
    int m_hd;
};

bool compareMovieOrTvAT (const cMovieOrTvAT *first, const cMovieOrTvAT *second) {
  if (first->m_movie_tv_id != second->m_movie_tv_id) return first->m_movie_tv_id < second->m_movie_tv_id;
  if (first->m_season_number != second->m_season_number) return first->m_season_number < second->m_season_number;
  if (first->m_episode_number != second->m_episode_number) return first->m_episode_number < second->m_episode_number;
  return first->m_language < second->m_language;
}
bool equalWoLanguageMovieOrTvAT (const cMovieOrTvAT *first, const cMovieOrTvAT *second) {
  if (first->m_movie_tv_id != second->m_movie_tv_id) return false;
  if (first->m_season_number != second->m_season_number) return false;
  if (first->m_episode_number != second->m_episode_number) return false;
  return true;
}
bool operator< (const cMovieOrTvAT &first, const cMovieOrTvAT &second) {
  return compareMovieOrTvAT(&first, &second);
}

class cTimerMovieOrTv: public cMovieOrTvAT {
  public:
    cTimerMovieOrTv(const cTimer *ti, const cMovieOrTv *movieOrTv): cMovieOrTvAT(ti->Channel()->GetChannelID(), movieOrTv)
      { m_timerId = ti->Id(); m_tstart = ti->StartTime(); }
    int m_timerId;
    time_t m_tstart;
    mutable bool m_needed = false;
    bool isBetter(const cTimerMovieOrTv &sec) const { return m_hd != sec.m_hd?(m_hd > sec.m_hd): (m_tstart < sec.m_tstart); }
};

class cEventMovieOrTv: public cMovieOrTvAT {
  public:
    cEventMovieOrTv(int v): cMovieOrTvAT(v) {}
    bool isBetter(const cEventMovieOrTv &sec) const { return m_hd != sec.m_hd?(m_hd > sec.m_hd): (m_event_start_time < sec.m_event_start_time); }
    tChannelID m_channelid;
    tEventID m_event_id;
    time_t m_event_start_time;
};

class cScraperRec: public cMovieOrTvAT {
  public:
    cScraperRec(tEventID event_id, time_t event_start_time, const tChannelID &channel_id, const std::string &name, int movie_tv_id, int season_number, int episode_number, int numberOfErrors): cMovieOrTvAT(channel_id, movie_tv_id, season_number, episode_number)
    { m_event_id = event_id; m_event_start_time = event_start_time; m_channelid = channel_id; m_name = name; m_numberOfErrors = numberOfErrors; }
    bool isBetter(const cScraperRec &sec) const { return m_hd != sec.m_hd?m_hd > sec.m_hd: m_numberOfErrors < sec.m_numberOfErrors; }
    int seasonNumber() const { return m_season_number;}
    int episodeNumber() const { return m_episode_number;}
    int movieTvId() const { return m_movie_tv_id;}
    bool improvemntPossible() const { return m_hd < 1 || m_numberOfErrors != 0; }
    int numberOfErrors() const { return m_numberOfErrors; }
    int hd() const { return m_hd; }
    const std::string &name() const { return m_name;}
  private:
    tChannelID m_channelid;
    tEventID m_event_id;
    time_t m_event_start_time;
    std::string m_name;
    int m_numberOfErrors;

    friend bool operator< (const cScraperRec &rec1, const cScraperRec &rec2);
    friend bool operator< (const cScraperRec &first, const cEventMovieOrTv &sec);
    friend bool operator< (const cEventMovieOrTv &first, const cScraperRec &sec);
    friend bool operator< (const cScraperRec &first, int sec);
    friend bool operator< (int first, const cScraperRec &sec);
};

bool timersForEvents(const cTVScraperDB &db);
cEvent* getEvent(tEventID eventid, const tChannelID &channelid);
#endif // __TVSCRAPER_AUTOTIMERS_H

#ifndef __TVSCRAPER_AUTOTIMERS_H
#define __TVSCRAPER_AUTOTIMERS_H
#include<string>
#include<set>
#include<vector>

class cMovieOrTvAT {
  public:
    int m_hd;
    int m_movie_tv_id; // movie if season_number == -100. Otherwisse, tv
    int m_season_number;
    int m_episode_number;
};

class cTimerMovieOrTv: public cMovieOrTvAT {
  public:
    int m_timerId;
    time_t m_tstart;
    mutable bool m_needed = false;
    bool isBetter(const cTimerMovieOrTv &sec) const { return m_hd != sec.m_hd?(m_hd > sec.m_hd): (m_tstart < sec.m_tstart); }
};

class cEventMovieOrTv {
  public:
    bool isBetter(const cEventMovieOrTv &sec) const { return m_hd != sec.m_hd?(m_hd > sec.m_hd): (m_event->StartTime() < sec.m_event->StartTime()); }
    const cEvent *m_event;
    int m_hd;
    int m_movie_tv_id; // movie if season_number == -100. Otherwisse, tv
    int m_season_number;
    int m_episode_number;
};

class cScraperRec {
  public:
    cScraperRec(int event_id, int event_start_time, const std::string &channel_id, const std::string &name, int movie_tv_id, int season_number, int episode_number, int hd, int numberOfErrors)
    { m_event_id = event_id; m_event_start_time = event_start_time; m_channel_id = channel_id; m_name = name; m_movie_tv_id = movie_tv_id; m_season_number = season_number; m_episode_number = episode_number; m_hd = hd; m_numberOfErrors = numberOfErrors; }
    bool isBetter(const cScraperRec &sec) const { return m_hd != sec.m_hd?m_hd > sec.m_hd: m_numberOfErrors < sec.m_numberOfErrors; }
    int seasonNumber() const { return m_season_number;}
    int episodeNumber() const { return m_episode_number;}
    int movieTvId() const { return m_movie_tv_id;}
    bool improvemntPossible() const { return m_hd < 1 || m_numberOfErrors != 0; }
    int numberOfErrors() const { return m_numberOfErrors; }
    int hd() const { return m_hd; }
    const std::string &name() const { return m_name;}
  private:
    int m_event_id;
    int m_event_start_time;
    std::string m_channel_id;
    std::string m_name;
    int m_movie_tv_id; // movie if season_number == -100. Otherwisse, tv
    int m_season_number;
    int m_episode_number;
    int m_hd; // 0: sd, 1:hd, 2:uhd
    int m_numberOfErrors;

    friend bool operator< (const cScraperRec &rec1, const cScraperRec &rec2);
    friend bool operator< (const cScraperRec &first, const cEventMovieOrTv &sec);
    friend bool operator< (const cEventMovieOrTv &first, const cScraperRec &sec);
    friend bool operator< (const cScraperRec &first, int sec);
    friend bool operator< (int first, const cScraperRec &sec);
};

bool timersForEvents(const cTVScraperDB &db);
#endif // __TVSCRAPER_AUTOTIMERS_H

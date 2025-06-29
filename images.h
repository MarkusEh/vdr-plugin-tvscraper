#ifndef __TVSCRAPER_IMAGES_H
#define __TVSCRAPER_IMAGES_H
#include "services.h"

class cImageLevelsInt:cImageLevels {
  public:
    cImageLevelsInt();
    cImageLevelsInt(cImageLevels parent);
    cImageLevelsInt(eImageLevel first, eImageLevel second = eImageLevel::none, eImageLevel third = eImageLevel::none, eImageLevel forth = eImageLevel::none);
    void push(eImageLevel lv);
    eImageLevel pop();
    eImageLevel popFirst();
    bool inList(eImageLevel lv, bool (*equal)(eImageLevel lv1, eImageLevel lv2));
    cImageLevelsInt removeDuplicates(bool (*equal)(eImageLevel lv1, eImageLevel lv2));
    static const int s_max_levels = 4;
    static bool equalTvShows(eImageLevel lv1, eImageLevel lv2);
    static bool equalMovies(eImageLevel lv1, eImageLevel lv2);
  private:
    void findTopElement();
    int m_pop;
    int m_push;
};

class cOrientationsInt:cOrientations {
  public:
    cOrientationsInt();
    cOrientationsInt(cOrientations parent);
    cOrientationsInt(eOrientation first, eOrientation second = eOrientation::none, eOrientation third = eOrientation::none);
    void push(eOrientation o);
    eOrientation pop();
    eOrientation popFirst();
    static const int s_max_orientations = 3;
  private:
    void findTopElement();
    int m_pop;
    int m_push;
};

std::string getRecordingImagePath(const cRecording *recording);
template<class T>
cTvMedia getEpgImage(const cEvent *event, const cRecording *recording, bool fullPath = false);

class cEpgImagePath: public cToSvConcat<255> {
  public:
    cEpgImagePath() {}
template<class T>
    cEpgImagePath(const T *event, bool createPaths = false) { set(event, createPaths); }
    void set(tEventID eventID, time_t eventStartTime, const tChannelID &channelID, bool createPaths = false);
template<class T>
    void set(const T *event, bool createPaths = false)
      { if (event) set(event->EventID(), event->StartTime(), event->ChannelID(), createPaths); }
};


#endif //__TVSCRAPER_IMAGES_H

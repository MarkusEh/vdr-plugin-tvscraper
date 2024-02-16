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
std::string getExistingEpgImagePath(tEventID eventID, time_t eventStartTime, const tChannelID &channelID);
template<class T>
std::string getEpgImagePath(const T *event, bool createPaths = false);
cTvMedia getEpgImage(const cEvent *event, const cRecording *recording, bool fullPath = false);

#endif //__TVSCRAPER_IMAGES_H

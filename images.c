#include "images.h"

// cImageLevelsInt *********************
cImageLevelsInt::cImageLevelsInt():
  m_pop(0),
  m_push(0)
  {
    m_imageLevels = 0;
  }
cImageLevelsInt::cImageLevelsInt(cImageLevels parent):
  m_pop(0),
  m_push(-1)
  {
    m_imageLevels = parent.m_imageLevels;
  }

cImageLevelsInt::cImageLevelsInt(eImageLevel first, eImageLevel second, eImageLevel third, eImageLevel forth):
  m_pop(0),
  m_push(0)
  {
    if (first  != eImageLevel::none) push(first);
    if (second != eImageLevel::none) push(second);
    if (third  != eImageLevel::none) push(third );
    if (forth  != eImageLevel::none) push(forth );
  }

void cImageLevelsInt::findTopElement() {
  int t = m_imageLevels;
  for (m_push = 0; m_push < s_max_levels; m_push++) {
    if ((t & 7) == 0) return;
    t = t >> 3;
  }
}
void cImageLevelsInt::push(eImageLevel lv) {
  if (m_push < 0) findTopElement();
  m_imageLevels |= ((int)lv) << (m_push * 3);
  m_push++;
}

eImageLevel cImageLevelsInt::popFirst() {
  m_pop = 0;
  return pop();
}
eImageLevel cImageLevelsInt::pop() {
  return (eImageLevel)((m_imageLevels >> (m_pop++ * 3)) & 7);
}

bool cImageLevelsInt::inList(eImageLevel lv, bool (*equal)(eImageLevel lv1, eImageLevel lv2)) {
// return true if lv is in this list (or level equal to lv)
  for (eImageLevel lvInList = popFirst(); lvInList != eImageLevel::none; lvInList = pop() )
    if (equal(lvInList, lv) ) return true;
  return false;
}

cImageLevelsInt cImageLevelsInt::removeDuplicates(bool (*equal)(eImageLevel lv1, eImageLevel lv2)){
// out: list of levels, remove duplicate
  cImageLevelsInt lv_out;
  for (eImageLevel lv_to_add = popFirst(); lv_to_add != eImageLevel::none; lv_to_add = pop() ) {
// add this level to the list, if it was not found (not already in list)
    if (!lv_out.inList(lv_to_add, equal)) lv_out.push(lv_to_add);
  }
  return lv_out;
}

bool cImageLevelsInt::equalTvShows(eImageLevel lv1, eImageLevel lv2) {
  return lv1 == lv2;
}

bool cImageLevelsInt::equalMovies(eImageLevel level1, eImageLevel level2) {
  if (level1 == level2) return true;
  switch (level1) {
    case eImageLevel::episodeMovie: return level2 == eImageLevel::seasonMovie;
    case eImageLevel::seasonMovie: return level2 == eImageLevel::episodeMovie;
    case eImageLevel::tvShowCollection: return level2 == eImageLevel::anySeasonCollection;
    case eImageLevel::anySeasonCollection: return level2 == eImageLevel::tvShowCollection;
    default: return false;
  }
}
// cOrientationsInt *********************
cOrientationsInt::cOrientationsInt():
  m_pop(0),
  m_push(0)
  {
    m_orientations = 0;
  }
cOrientationsInt::cOrientationsInt(cOrientations parent):
  m_pop(0),
  m_push(-1)
  {
    m_orientations = parent.m_orientations;
  }

cOrientationsInt::cOrientationsInt(eOrientation first, eOrientation second, eOrientation third):
  m_pop(0),
  m_push(0)
  {
    if (first  != eOrientation::none) push(first);
    if (second != eOrientation::none) push(second);
    if (third  != eOrientation::none) push(third );
  }

void cOrientationsInt::findTopElement() {
  int t = m_orientations;
  for (m_push = 0; m_push < s_max_orientations; m_push++) {
    if (t & 7 == 0) return;
    t = t >> 3;
  }
}
void cOrientationsInt::push(eOrientation o) {
  if (m_push < 0) findTopElement();
  m_orientations |= ((int)o) << (m_push * 3);
  m_push++;
}

eOrientation cOrientationsInt::popFirst() {
  m_pop = 0;
  return pop();
}
eOrientation cOrientationsInt::pop() {
  return (eOrientation)((m_orientations >> (m_pop++ * 3)) & 7);
}

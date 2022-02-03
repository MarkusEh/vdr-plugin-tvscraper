#ifndef __SEARCH_RESULT_H
#define __SEARCH_RESULT_H

struct searchResultTvMovie {
    int id;
    bool movie;
    int distance;
    int durationDistance; // in minutes. -1: not calculated; -2: no data
    float popularity;
    int year; // set this to 0 if no year matched
    float fastMatch; // without duration, 0-1, higher number is better match
//  float match; // 0-1, higher number is better match
};

#endif //__SEARCH_RESULT_H


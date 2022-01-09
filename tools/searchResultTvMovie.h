#ifndef __SEARCH_RESULT_H
#define __SEARCH_RESULT_H

struct searchResultTvMovie {
    int id;
    bool movie;
    int distance;
    float popularity;
    int year; // set this to 0 if no year matched
};

#endif //__SEARCH_RESULT_H


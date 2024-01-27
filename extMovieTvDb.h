#ifndef __TVSCRAPER_EXTMOVIETVDB_H
#define __TVSCRAPER_EXTMOVIETVDB_H
// interface for external movie or tv database

#include "services.h"
#include <vdr/epg.h>
#include <string>
#include <vector>
#include <memory>

//
// to be implemented by the external movie or tv database ================
//
/*
class cNormedStringsDelim {
  public:
    cNormedStringsDelim(): m_normedStrings(5) {}
    std::vector<std::optional<cNormedString>> m_normedStrings;
    char m_delim;
};
// typedef std::vector<std::optional<cNormedStringsDelim>> cCompareStrings;
class cCompareStrings {
  public:
    cCompareStrings(cSv searchString);
    class iterator {
      private:
        std::vector<std::optional<cNormedStringsDelim>>::iterator m_it;
      public:
        iterator(std::vector<std::optional<cNormedStringsDelim>>::iterator it): m_it(it) {}
        iterator& operator++() { ++m_it; return *this; }
        bool operator!=(iterator other) const { return m_it != other.m_it; }
        char operator*() const { return (*m_it).value().m_delim; } // return delim
    };
    iterator begin() { return iterator(m_normedStringsDelim.begin()); }
    iterator end()   { return iterator(m_normedStringsDelim.end()); }

    int minDistance(char delim, int dist_a, const cNormedString &compareString);
  private:
    std::vector<std::optional<cNormedStringsDelim>> m_normedStringsDelim;
};
*/

class iExtMovieTvDb
{
  public:
    iExtMovieTvDb() {}
    virtual ~iExtMovieTvDb() {}

      // download  / update the movie / TV show with this ID from external db
      // images are NOT downloaded, but information for later download with downloadImages is saved in local db
      // figure out tv_display_language and write it to tv2
      //    note: if translations are available in default language, tv_display_language = default language
      //          otherwise: find a fallback
      // if forceUpdate == true: always download / update all data, including episodes in default language
      // if forceUpdate == false: check if the movie / TV show already exists in local db.
      //   if no, proceed as with forceUpdate == true
      //   if yes, only update episode list in tv_display_language, if this update is required
      //  return -1 if id does not exist in external db
      // TODO: Remove forceUpdate, this is always false!!!
    virtual int download(int id) = 0;

      // do nothing for movies
      // download / update the TV show episodes with ID id, in language lang from external db
      // images are NOT downloaded, but information for later download with downloadImages is saved in local db
      // also download / update some data in tv_s_e and tv2, but incomplete. Don't change "name" in tv2, so we can check this to figure out if data is incomplete
      // if lang == default language, also update descriptions in tv_s_e, and add this language to tv2 (tv_display_language)
      // if tv_display_language is in tv2 && lang == tv_display_language in tv2, also update descriptions in tv_s_e
      // only update if episodeNameUpdateRequired.
      //  return codes:
//   -1 object does not exist (we already called db->DeleteSeriesCache(-id))
//    0 success
//    1 no episode names in this language
//    2 invalid input data: id
//    3 invalid input data: lang
//    5 or > 5: Other error
    virtual int downloadEpisodes(int id, const cLanguage *lang) = 0;

      // check if data required for enhance1 is available. If yes, just return
      // if no, download data from external db
      // note: these are small data (e.g. runtime), which will never be deleted and
      // can repeatedly help to identify the correct object
    virtual void enhance1(searchResultTvMovie &searchResultTvMovie, const cLanguage *lang) = 0;

      // download all images of the movie / TV show with this ID
    virtual int downloadImages(int id, int seasonNumber = 0, int episodeNumber = 0) = 0;

      // search searchString in external database
      // call with isFullSearchString == true if searchString is the complete searchString
      // you should split searchString to several parts, and call with each part to find more results.
      //   In this case, set isFullSearchString == false
    virtual void addSearchResults(vector<searchResultTvMovie> &resultSet, cSv searchString, bool isFullSearchString, const cCompareStrings &compareStrings, const char *shortText, const char *description, const cYears &years, const cLanguage *lang, cSv network) = 0;
};


#endif // __TVSCRAPER_EXTMOVIETVDB_H

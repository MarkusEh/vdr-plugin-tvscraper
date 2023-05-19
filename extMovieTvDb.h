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
    cCompareStrings(std::string_view searchString);
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
      // if forceUpdate == true: always download / update all data, including episodes in default language
      // if forceUpdate == false: check if the movie / TV show already exists in local db.
      //   if no, proceed as with forceUpdate == true
      //   if yes, only update episode list in default language, if this update is required
      //  return -1 if id does not exist in external db
    virtual int download(int id, bool forceUpdate = false) = 0;

      // download / update language specific texts, in given language
      // if lang == default language: call download(id, false);
      // otherwise, text in default language and language independent information will not be downloaded
      // note: currenty language dependent text is only available for episode names
      // if language dependent text is already available: update only if required (there might be changes)
      //  return -1 if id does not exist in external db
    virtual int download(int id, const cLanguage *lang) = 0;

      // check if data required for enhance1 is available. If yes, just return
      // if no, download data from external db
      // note: these are small data (e.g. runtime), which will never be deleted and
      // can repeatedly help to identify the correct object
    virtual void enhance1(int id) = 0;

      // download all images of the movie / TV show with this ID
    virtual int downloadImages(int id, int seasonNumber = 0, int episodeNumber = 0) = 0;

      // search searchString in external database
      // call with isFullSearchString == true if searchString is the complete searchString
      // you should split searchString to several parts, and call with each part to find more results.
      //   In this case, set isFullSearchString == false
    virtual void addSearchResults(cLargeString &buffer, vector<searchResultTvMovie> &resultSet, std::string_view searchString, bool isFullSearchString, const cCompareStrings &compareStrings, const char *description, const cYears &years, const cLanguage *lang) = 0;
};


#endif // __TVSCRAPER_EXTMOVIETVDB_H

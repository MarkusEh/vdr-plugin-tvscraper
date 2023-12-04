#ifndef __TVSCRAPERHELPERS_H
#define __TVSCRAPERHELPERS_H

// years and stringhelper functions specific for tvscraper

#include <cstdarg>
#include <string>
#include <string.h>
#include <vector>
#include <iostream>
#include <chrono>
#include "stringhelpers.h"

inline cSv textAttributeValue(const char *text, const char *attributeName) {
// if attributeName is empty or not found in text: return ""
// else, return text after attributeName to end of line
  if (!text || !attributeName || !*attributeName) return cSv();
  const char *found = strstr(text, attributeName);
  if (!found) return cSv();
  const char *avs = found + strlen(attributeName);
  const char *ave = strchr(avs, '\n');
  if (!ave) return cSv(avs);
  return cSv(avs, ave-avs);
}

// =========================================================
// methods for years =======================================
// =========================================================

class cYears {
  public:
    cYears() { m_years[0] = 0; }
    cYears(const cYears&) = delete;
    cYears &operator= (const cYears &) = delete;
// note: iterate first over exact matches, then near matches
    class iterator {
        const char *m_years;
      public:
//      using iterator_category = std::forward_iterator_tag;
        explicit iterator(const char *years): m_years(years) { }
        iterator& operator++() {
          if (*m_years) m_years++;
          return *this;
        }
        bool operator!=(iterator other) const { return m_years != other.m_years; }
        int operator*() const {
          return (*m_years) + 1900;
        }
      };
    iterator begin() const { return iterator(m_years); }
    const iterator end() const { return iterator(m_years + m_years_p); }
    void addYear(const char *year) { addYear(yearToInt(year)); }
    void addYear(int year) {
      if (m_explicitFound) return;
      if (year <= 1920 || year >= 2100) return;
      addYearInt(year-1900);
    }
    void addYears(const char *str) {
      if (!str || m_explicitFound) return;
      cSv year_sv = textAttributeValue(str, "Jahr: ");
      if (year_sv.length() == 4) {
        int y = yearToInt(year_sv.data() );
        if (!(y <= 1920 || y >= 2100)) {
          m_explicitFound = true;
          m_years_p = 0;
          m_years[m_years_p++] = y-1900;
          m_years[m_years_p] = 0;
          return;
        }
      }
      const char *last;
      for (const char *first = firstDigit(str); *first; first = firstDigit(last) ) {
        last = firstNonDigit(first);
        if (last - first  == 4) addYear(first);
      }
    }
    int find2(int year) const {
// 0 not found
// 1 near  match found
// 2 exact match found
      if (year <= 1920 || year >= 2100) return 0;
      const char *f = strchr(m_years, year-1900);
      if (!f) return 0;
      if (m_years + m_years_e > f) return 2;  // [m_years[0]; m_years[m_years_e]): exact matches
      return 1;
    }
    int size() const { return strlen(m_years); }
    static int yearToInt(const char *year) {
// if the first 4 characters of year are digits, return this number as int
// otherwise (or if year == NULL) return 0;
      if (!year) return 0;
      if (*year < '0' || *year > '9') return 0;
      int result = (*year - '0') * 1000;
      year++;
      if (*year < '0' || *year > '9') return 0;
      result += (*year - '0') * 100;
      year++;
      if (*year < '0' || *year > '9') return 0;
      result += (*year - '0') * 10;
      year++;
      if (*year < '0' || *year > '9') return 0;
      result += (*year - '0');
      return result;
    }
template<class T>
    static int yearToInt(const T &str) {
      if (str.length() < 4 || (str.length() > 4 && isdigit(str[4])) ) return 0;
      return yearToInt(str.data() );
    }
    void finalize() {
// this adds year +-1, at the end
      if (m_years_e != -1) return;  // add this only once ...
      m_years_e = m_years_p;
      for (int i=0; i < m_years_e; i++) {
        addYearInt(m_years[i] + 1);
        addYearInt(m_years[i] - 1);
      }
    }

  private:
    void addYearInt(int year) {
// year must be in internal format !!! (real year - 1900)
      if (m_years_p > 18) return;
      if (strchr(m_years, year) != NULL) return;
      m_years[m_years_p++] = year;
      m_years[m_years_p] = 0;
    }
    static const char *firstDigit(const char *found) {
      for (; ; found++) if (isdigit(*found) || ! *found) return found;
    }
    static const char *firstNonDigit(const char *found) {
      for (; ; found++) if (!isdigit(*found) || ! *found) return found;
    }

    char m_years[20];
    int m_years_p = 0;
    int m_years_e = -1;
    bool m_explicitFound = false;
};

inline bool splitString(cSv str, char delimiter, size_t minLengh, cSv &first, cSv &second) {
  using namespace std::literals::string_view_literals;
  if (delimiter == '-') return splitString(str, " - "sv, minLengh, first, second);
  if (delimiter == ':') return splitString(str, ": "sv, minLengh, first, second);
  std::string delim(1, delimiter);
  return splitString(str, delim, minLengh, first, second);
}

inline int StringRemoveLastPartWithP(const char *str, int len) {
// remove part with (...)
// return -1 if nothing can be removed
// otherwise length of string without ()
  len = StringRemoveTrailingWhitespace(str, len);
  if (len < 3) return -1;
  if (str[len -1] != ')') return -1;
  for (int i = len -2; i; i--) {
    if (!isdigit(str[i]) && str[i] != '/') {
      if (str[i] != '(') return -1;
      int len2 = StringRemoveLastPartWithP(str, i);
      if (len2 == -1 ) return StringRemoveTrailingWhitespace(str, i);
      return len2;
    }
  }
  return -1;
}

inline bool StringRemoveLastPartWithP(std::string &str) {
// remove part with (...)
  int len = StringRemoveLastPartWithP(str.c_str(), str.length() );
  if (len < 0) return false;
  str.erase(len);
  return true;
}
inline cSv removeLastPartWithP(cSv str) {
  int l = StringRemoveLastPartWithP(str.data(), str.length() );
  if (l < 0) return str;
  return cSv(str.data(), l);
}

inline int NumberInLastPartWithPS(cSv str) {
// return number in last part with (./.), 0 if not found / invalid
  if (str.length() < 3 ) return 0;
  if (str[str.length() - 1] != ')') return 0;
  std::size_t found = str.find_last_of("(");
  if (found == std::string::npos) return 0;
  for (std::size_t i = found + 1; i < str.length() - 1; i ++) {
    if (!isdigit(str[i]) && str[i] != '/') return 0; // we ignore (asw), and return only number with digits only
  }
  return parse_unsigned_internal<int>(str.substr(found + 1));
}
inline int NumberInLastPartWithP(cSv str) {
// return number in last part with (...), 0 if not found / invalid
  if (str.length() < 3 ) return 0;
  if (str[str.length() - 1] != ')') return 0;
  std::size_t found = str.find_last_of("(");
  if (found == std::string::npos) return 0;
  for (std::size_t i = found + 1; i < str.length() - 1; i ++) {
    if (!isdigit(str[i])) return 0; // we ignore (asw), and return only number with digits only
  }
  return parse_unsigned_internal<int>(str.substr(found + 1));
}

inline int seasonS(cSv description_part, const char *S) {
// return season, if found at the beginning of description_part
// otherwise, return -1
//  std::cout << "seasonS " << description_part << "\n";
  size_t s_len = strlen(S);
  if (description_part.length() <= s_len ||
     !isdigit(description_part[s_len])   ||
     description_part.compare(0, s_len, S)  != 0) return -1;
  return parse_unsigned<int>(description_part.substr(s_len));
}
inline bool episodeSEp(int &season, int &episode, cSv description, const char *S, const char *Ep) {
// search pattern S<digit> Ep<digits>
// return true if episode was found.
// set season = -1 if season was not found
// set episode = 0 if episode was not found
// find Ep[digit]
  season = -1;
  episode = 0;
  size_t Ep_len = strlen(Ep);
  size_t ep_pos = 0;
  do {
    ep_pos = description.find(Ep, ep_pos);
    if (ep_pos == std::string_view::npos || ep_pos + Ep_len >= description.length() ) return false;  // no Ep[digit]
    ep_pos += Ep_len;
//  std::cout << "ep_pos = " << ep_pos << "\n";
  } while (!isdigit(description[ep_pos]));
// Ep[digit] found
//  std::cout << "ep found at " << description.substr(ep_pos) << "\n";
  episode = parse_unsigned<int>(description.substr(ep_pos));
  if (ep_pos - Ep_len >= 3) season = seasonS(description.substr(ep_pos - Ep_len - 3), S);
  if (season < 0 && ep_pos - Ep_len >= 4) season = seasonS(description.substr(ep_pos - Ep_len - 4), S);
  return true;
}

// =========================================================
// special container:
//   delimiter is '|'
//   if delimiter is missing at start of string: take complete string
// =========================================================

template<class T>
void push_back_new(std::vector<T> &vec, cSv str) {
// add str to vec, but only if str is not yet in vec and if str is not empty
  if (str.empty() ) return;
  if (find (vec.begin(), vec.end(), str) != vec.end() ) return;
  vec.emplace_back(str);
}

template<class T>
void stringToVector(std::vector<T> &vec, const char *str) {
// delimiter is '|', and must be available at start of str.
// if str does not start with '|', don't split, just add str to vec
// otherwise, split str at '|', and add each part to vec
  if (!str || !*str) return;
  if (str[0] != '|') { vec.push_back(str); return; }
  const char *lDelimPos = str;
  for (const char *rDelimPos = strchr(lDelimPos + 1, '|'); rDelimPos != NULL; rDelimPos = strchr(lDelimPos + 1, '|') ) {
    push_back_new<T>(vec, cSv(lDelimPos + 1, rDelimPos - lDelimPos - 1));
    lDelimPos = rDelimPos;
  }
}

inline std::string vectorToString(const std::vector<std::string> &vec) {
  if (vec.size() == 0) return std::string();
  if (vec.size() == 1) return vec[0];
  std::string result("|");
  for (const std::string &str: vec) { result.append(str); result.append("|"); }
  return result;
}

// =========================================================
// special container:
//   set, can contain string or integer or channel
//        or any other object wher methods to convert from/to string are available
// =========================================================

inline std::string objToString(const int &i) { return std::string(cToSvInt(i)); }
inline std::string objToString(const std::string &i) { return i; }
inline std::string objToString(const tChannelID &i) { return std::string(cToSvChannel(i)); }

template<class T, class C=std::set<T>>
std::string getStringFromSet(const C &in, char delim = ';') {
  if (in.size() == 0) return std::string();
  std::string result;
  for (const T &i: in) {
    result.append(objToString(i));
    result.append(1, delim);
  }
  return result;
}

template<class T> T stringToObj(const char *s, size_t len) {
  esyslog("tvscraper: ERROR: template<class T> T stringToObj called");
  return 5; }
template<> inline int stringToObj<int>(const char *s, size_t len) { return parse_unsigned<int>(cSv(s, len)); }
template<> inline std::string stringToObj<std::string>(const char *s, size_t len) { return std::string(s, len); }
template<> inline cSv stringToObj<cSv>(const char *s, size_t len) { return cSv(s, len); }
template<> tChannelID stringToObj<tChannelID>(const char *s, size_t len) {
  char buf[len+1];
  buf[len] = 0;
  memcpy(buf, s, len);
  return tChannelID::FromString(buf);
}

template<class T> void insertObject(std::vector<T> &cont, const T &obj) { cont.push_back(obj); }
template<class T> void insertObject(std::set<T> &cont, const T &obj) { cont.insert(obj); }

template<class T, class C=std::set<T>>
C getSetFromString(const char *str, char delim = ';') {
// split str at delim, and add each part to result
  C result;
  if (!str || !*str) return result;
  const char *lStartPos = str;
  if (delim == '|' && lStartPos[0] == delim) lStartPos++;
  for (const char *rDelimPos = strchr(lStartPos, delim); rDelimPos != NULL; rDelimPos = strchr(lStartPos, delim) ) {
    insertObject<T>(result, stringToObj<T>(lStartPos, rDelimPos - lStartPos));
    lStartPos = rDelimPos + 1;
  }
  const char *rDelimPos = strchr(lStartPos, 0);
  if (rDelimPos != lStartPos) insertObject<T>(result, stringToObj<T>(lStartPos, rDelimPos - lStartPos));
  return result;
}

class cConcatenate
// deprecated. Use function concatenate or stringAppend
{
  public:
    cConcatenate(size_t buf_size = 0) { m_data.reserve(buf_size>0?buf_size:200); }
    cConcatenate(const cConcatenate&) = delete;
    cConcatenate &operator= (const cConcatenate &) = delete;
  template<typename T>
    cConcatenate & operator<<(const T &i) { stringAppend(m_data, i); return *this; }
    std::string moveStr() { return std::move(m_data); }
    operator cSv() const { return cSv(m_data.data(), m_data.length()); }
    const char *getCharS() { return m_data.c_str(); }
  private:
    std::string m_data;
};

#endif // __TVSCRAPERHELPERS_H

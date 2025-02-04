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

#define CV_VA_NUM_ARGS_HELPER(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...)    N
#define CV_VA_NUM_ARGS(...)      CV_VA_NUM_ARGS_HELPER(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define CAT2( A, B ) A ## B
#define SELECT( NAME, NUM ) CAT2( NAME ## _, NUM )
#define VA_SELECT( NAME, ... ) SELECT( NAME, CV_VA_NUM_ARGS(__VA_ARGS__) )(__VA_ARGS__)

// concatenate macro based  ===========================
// deprectaed, use cToSvConcat, which is almost as fast
#define CONCATENATE_START_2(result, s1, s2) \
int result##concatenate_lvls = 0; \
int result##concatenate_lvl1 = stringhelpers_internal::numChars(s1); \
result##concatenate_lvls += result##concatenate_lvl1; \
int result##concatenate_lvl2 = stringhelpers_internal::numChars(s2); \
result##concatenate_lvls += result##concatenate_lvl2;

#define CONCATENATE_START_3(result, s1, s2, s3) \
CONCATENATE_START_2(result, s1, s2) \
int result##concatenate_lvl3 = stringhelpers_internal::numChars(s3); \
result##concatenate_lvls += result##concatenate_lvl3;

#define CONCATENATE_START_4(result, s1, s2, s3, s4) \
CONCATENATE_START_3(result, s1, s2, s3) \
int result##concatenate_lvl4 = stringhelpers_internal::numChars(s4); \
result##concatenate_lvls += result##concatenate_lvl4;

#define CONCATENATE_START_5(result, s1, s2, s3, s4, s5) \
CONCATENATE_START_4(result, s1, s2, s3, s4) \
int result##concatenate_lvl5 = stringhelpers_internal::numChars(s5); \
result##concatenate_lvls += result##concatenate_lvl5;

#define CONCATENATE_START_6(result, s1, s2, s3, s4, s5, s6) \
CONCATENATE_START_5(result, s1, s2, s3, s4, s5) \
int result##concatenate_lvl6 = stringhelpers_internal::numChars(s6); \
result##concatenate_lvls += result##concatenate_lvl6;

#define CONCATENATE_START_7(result, s1, s2, s3, s4, s5, s6, s7) \
CONCATENATE_START_6(result, s1, s2, s3, s4, s5, s6) \
int result##concatenate_lvl7 = stringhelpers_internal::numChars(s7); \
result##concatenate_lvls += result##concatenate_lvl7;

#define CONCATENATE_START_8(result, s1, s2, s3, s4, s5, s6, s7, s8) \
CONCATENATE_START_7(result, s1, s2, s3, s4, s5, s6, s7) \
int result##concatenate_lvl8 = stringhelpers_internal::numChars(s8); \
result##concatenate_lvls += result##concatenate_lvl8;

#define CONCATENATE_START_9(result, s1, s2, s3, s4, s5, s6, s7, s8, s9) \
CONCATENATE_START_8(result, s1, s2, s3, s4, s5, s6, s7, s8) \
int result##concatenate_lvl9 = stringhelpers_internal::numChars(s9); \
result##concatenate_lvls += result##concatenate_lvl9;

#define CONCATENATE_END_ADDCHARS_B(result_concatenate_buf, lvl, s) \
stringhelpers_internal::addChars(result_concatenate_buf, lvl, s); \
result_concatenate_buf += lvl;

#define CONCATENATE_END_2(result, s1, s2) \
char *result##concatenate_buf = result; \
CONCATENATE_END_ADDCHARS_B(result##concatenate_buf, result##concatenate_lvl1, s1); \
CONCATENATE_END_ADDCHARS_B(result##concatenate_buf, result##concatenate_lvl2, s2);

#define CONCATENATE_END_3(result, s1, s2, s3) \
CONCATENATE_END_2(result, s1, s2) \
CONCATENATE_END_ADDCHARS_B(result##concatenate_buf, result##concatenate_lvl3, s3);

#define CONCATENATE_END_4(result, s1, s2, s3, s4) \
CONCATENATE_END_3(result, s1, s2, s3) \
CONCATENATE_END_ADDCHARS_B(result##concatenate_buf, result##concatenate_lvl4, s4);

#define CONCATENATE_END_5(result, s1, s2, s3, s4, s5) \
CONCATENATE_END_4(result, s1, s2, s3, s4) \
CONCATENATE_END_ADDCHARS_B(result##concatenate_buf, result##concatenate_lvl5, s5);

#define CONCATENATE_END_6(result, s1, s2, s3, s4, s5, s6) \
CONCATENATE_END_5(result, s1, s2, s3, s4, s5) \
CONCATENATE_END_ADDCHARS_B(result##concatenate_buf, result##concatenate_lvl6, s6);

#define CONCATENATE_END_7(result, s1, s2, s3, s4, s5, s6, s7) \
CONCATENATE_END_6(result, s1, s2, s3, s4, s5, s6) \
CONCATENATE_END_ADDCHARS_B(result##concatenate_buf, result##concatenate_lvl7, s7);

#define CONCATENATE_END_8(result, s1, s2, s3, s4, s5, s6, s7, s8) \
CONCATENATE_END_7(result, s1, s2, s3, s4, s5, s6, s7) \
CONCATENATE_END_ADDCHARS_B(result##concatenate_buf, result##concatenate_lvl8, s8);

#define CONCATENATE_END_9(result, s1, s2, s3, s4, s5, s6, s7, s8, s9) \
CONCATENATE_END_8(result, s1, s2, s3, s4, s5, s6, s7, s8) \
CONCATENATE_END_ADDCHARS_B(result##concatenate_buf, result##concatenate_lvl9, s9);

#define CONCATENATE(result, ...) \
SELECT( CONCATENATE_START, CV_VA_NUM_ARGS(__VA_ARGS__) )(result, __VA_ARGS__) \
char result[result##concatenate_lvls + 1]; \
result[result##concatenate_lvls] = 0; \
SELECT( CONCATENATE_END, CV_VA_NUM_ARGS(__VA_ARGS__) )(result, __VA_ARGS__) \
*result##concatenate_buf = 0;

// methods for CONCATENATE
// deprecated

namespace stringhelpers_internal {
  inline int numChars(std::string_view s) { return s.length(); }
  inline int numChars(const std::string &s) { return s.length(); }
  inline int numChars(const char *s) { return s?strlen(s):0; }

static const int guess[] = {
    0, 0, 0, 0, 1, 1, 1, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 5, 5, 5,
    6, 6, 6, 6, 7, 7, 7, 8, 8, 8,
    9, 9, 9,
    9, 10, 10, 10, 11, 11, 11,
    12, 12, 12, 12, 13, 13, 13,
    14, 14, 14, 15, 15, 15, 15,
    16, 16, 16, 17, 17, 17,
    18, 18, 18, 18, 19
};

// i > 0 !!! not cheked here !!!!!
inline int usedBinDigits(unsigned char i) {
  return 8*sizeof(unsigned int)-__builtin_clz((unsigned int)i);
}
inline int usedBinDigits(unsigned short int i) {
  return 8*sizeof(unsigned int)-__builtin_clz((unsigned int)i);
}
inline int usedBinDigits(unsigned int i) {
//  return 4*sizeof(unsigned long long int)-__builtin_clzll(0x80000000 | ((unsigned long long int)i << 32));
  return 8*sizeof(unsigned int)-__builtin_clz(i);
}
inline int usedBinDigits(unsigned long int i) {
  return 8*sizeof(unsigned long int)-__builtin_clzl(i);
}
inline int usedBinDigits(unsigned long long int i) {
  return 8*sizeof(unsigned long long int)-__builtin_clzll(i);
}

template<typename T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
  inline int numChars_bi(T i) {
// i > 0 !!! not cheked here !!!!!
    int digits = guess[usedBinDigits(i)];
    return digits + (i > to_chars10_internal::max_int[digits]);
  }
template<typename T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
  inline int numChars(T i) {
  return i?numChars_bi(i):1;
}
template<typename T, std::enable_if_t<std::is_signed_v<T>, bool> = true>
  inline int numChars(T i) {
    typedef std::make_unsigned_t<T> TU;
    if (i > 0) return numChars_bi(static_cast<TU>(i));
    if (i < 0) return numChars_bi(~(static_cast<TU>(i)) + static_cast<TU>(1)) + 1;
    return 1;
  }

typedef char* (*itoaNT64)(char *b, uint64_t i);
static itoaNT64 fa[21] = {
   &stringhelpers_internal::itoaN<0, uint64_t>,
   &stringhelpers_internal::itoaN<1, uint64_t>,
   &stringhelpers_internal::itoaN<2, uint64_t>,
   &stringhelpers_internal::itoaN<3, uint64_t>,
   &stringhelpers_internal::itoaN<4, uint64_t>,
   &stringhelpers_internal::itoaN<5, uint64_t>,
   &stringhelpers_internal::itoaN<6, uint64_t>,
   &stringhelpers_internal::itoaN<7, uint64_t>,
   &stringhelpers_internal::itoaN<8, uint64_t>,
   &stringhelpers_internal::itoaN<9, uint64_t>,
   &stringhelpers_internal::itoaN<10, uint64_t>,
   &stringhelpers_internal::itoaN<11, uint64_t>,
   &stringhelpers_internal::itoaN<12, uint64_t>,
   &stringhelpers_internal::itoaN<13, uint64_t>,
   &stringhelpers_internal::itoaN<14, uint64_t>,
   &stringhelpers_internal::itoaN<15, uint64_t>,
   &stringhelpers_internal::itoaN<16, uint64_t>,
   &stringhelpers_internal::itoaN<17, uint64_t>,
   &stringhelpers_internal::itoaN<18, uint64_t>,
   &stringhelpers_internal::itoaN<19, uint64_t>,
   &stringhelpers_internal::itoaN<20, uint64_t>
};
template<typename T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
  inline void addChars(char *b, int l, T i) {
    stringhelpers_internal::fa[l](b, i);
  }
template<typename T, std::enable_if_t<std::is_signed_v<T>, bool> = true>
  inline void addChars(char *b, int l, T i) {
    if (i < 0) {
      *(b++) = '-';
      typedef std::make_unsigned_t<T> TU;
      stringhelpers_internal::fa[l-1](b, ~(TU(i)) + (TU)1);
    } else
      stringhelpers_internal::fa[l](b, i);
  }

  inline void addChars(char *b, int l, const std::string_view &s) { memcpy(b, s.data(), l); }
  inline void addChars(char *b, int l, const std::string &s) { memcpy(b, s.data(), l); }
  inline void addChars(char *b, int l, const char *s) { if(s) memcpy(b, s, l); }
}

inline cSv textAttributeValue(cSv text, const char *attributeName) {
// if attributeName is empty or not found in text: return ""
// else, return text after attributeName to end of line
  if (!attributeName || !*attributeName) return cSv();
  size_t pos = text.find(attributeName);
  if (pos == std::string::npos) return cSv();
  size_t pos_a = pos + strlen(attributeName);
  size_t pos_e = text.find('\n', pos_a);
  if (pos_e == std::string::npos) return text.substr(pos_a);
  return text.substr(pos_a, pos_e-pos_a);
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
 //   std::cout << "adding year " << year << "\n";
      addYearInt(year-1900);
    }
    void addYears(cSv str) {
      if (m_explicitFound) return;
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
      do {
        size_t pos1 = firstDigit(str);
        if (pos1 == std::string::npos) return;
        str.remove_prefix(pos1);
        size_t pos2 = firstNonDigit(str);
        if (pos2 == 4) addYear(str.data() );
        str.remove_prefix(pos2);
      } while (true);
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
// new: this adds year +1, at the end: EPG: Production year, ext.db: premiere
// back to +-1: Made a check, can also be -1; there seem to be several reasons for deviations ...
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
    static size_t firstDigit(cSv str) {
      for (size_t pos = 0; pos < str.length(); ++pos) if (isdigit(str[pos])) return pos;
      return std::string::npos;
    }
    static size_t firstNonDigit(cSv str) {
      for (size_t pos = 0; pos < str.length(); ++pos) if (!isdigit(str[pos])) return pos;
      return str.length();
    }

    char m_years[20];
    int m_years_p = 0;
    int m_years_e = -1;
    bool m_explicitFound = false;
};

inline bool splitString(cSv str, char delimiter, size_t minLengh, cSv &first, cSv &second) {
//  using namespace std::literals::string_view_literals;
  if (delimiter == '-') return splitString(str, " - ", minLengh, first, second);
  if (delimiter == ':') return splitString(str, ": ", minLengh, first, second);
  char delim[2];
  delim[0] = delimiter;
  delim[1] = 0;
  return splitString(str, delim, minLengh, first, second);
}

inline int StringRemoveTrailingWhitespace(const char *str, int len) {
// return "new" len of string, without whitespaces at the end
  if (!str) return 0;
  for (; len; len--) if (!my_isspace(str[len - 1])) return len;
  return 0;
}

inline cSv removeSuffix(cSv sv, cSv suffix) {
  if (sv.length() < suffix.length()) return sv;
  if (sv.substr(sv.length()-suffix.length()) != suffix) return sv;
  return sv.substr(0, sv.length()-suffix.length());
}

inline void StringRemoveSuffix(std::string &str, cSv suffix) {
  if (str.length() < suffix.length()) return;
  if (cSv(str).substr(str.length()-suffix.length()) != suffix) return;
  str.erase(str.length()-suffix.length());
}

inline utf8_iterator utf8LastLetter(cSv s) {
// return position directly after the last letter
  utf8_iterator it = s.utf8_end();
  while (it != s.utf8_begin() ) {
    --it;
    wint_t cp = it.codepoint();
    if (isdigit(cp)) continue;
    if (std::iswalnum(cp)) { ++it; return it; }
  }
  return it;
}

inline int lenWithoutLastPartWithP(cSv sv) {
// search part with (...)
// return -1 if nothing was found
// otherwise length of sv without ()
  int len = StringRemoveTrailingWhitespace(sv.data(), sv.length() );
  if (len < 3) return -1;
  if (sv[len -1] != ')') return -1;
  for (int i = len -2; i; i--) {
    if (!isdigit(sv[i]) && sv[i] != '/') {
      if (sv[i] != '(') return -1;
      int len2 = lenWithoutLastPartWithP(sv.substr(0, i));
      if (len2 == -1 ) return StringRemoveTrailingWhitespace(sv.data(), i);
      return len2;
    }
  }
  return -1;
}

inline int lenWithoutPartToIgnoreInSearch(cSv sv) {
// we keep the last letter and all digits directly following this letter
  utf8_iterator it = utf8LastLetter(sv);
  size_t found = sv.find(": ", it.pos());
  if (found != std::string_view::npos) return found;
  found = sv.find(" #", it.pos());
  if (found != std::string_view::npos) return found;
  return lenWithoutLastPartWithP(sv);
}

inline bool StringRemoveLastPartWithP(std::string &str) {
// remove part with (...)
  int len = lenWithoutPartToIgnoreInSearch(str);
  if (len < 0) return false;
  str.erase(len);
  return true;
}
inline cSv removeLastPartWithP(cSv str) {
  int l = lenWithoutPartToIgnoreInSearch(str);
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
  if (str[str.length() - 1] == ')') {
    std::size_t found = str.find_last_of("(");
    if (found == std::string::npos) return 0;
    for (std::size_t i = found + 1; i < str.length() - 1; i ++) {
      if (!isdigit(str[i])) return 0; // we ignore (asw), and return only number with digits only
    }
    return parse_unsigned_internal<int>(str.substr(found + 1));
  } else {
    size_t pos = str.length() - 1;
    for (; pos > 0 && isdigit(str[pos]); --pos);
    if (str[pos] == '#') return parse_unsigned_internal<int>(str.substr(pos + 1));
    return 0;
  }
}

inline int Number2InLastPartWithP(cSv str) {
// return second number n in last part with (../n), 0 if not found / invalid
  if (str.length() < 3 ) return 0;
  if (str[str.length() - 1] != ')') return 0;
  std::size_t found = str.find_last_of("(");
  if (found == std::string::npos) return 0;
  found = str.find("/", found+1);
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
// =========== search in char*
// =========================================================

inline const char* removePrefix(const char *s, const char *prefix) {
// if s starts with prefix, return s + strlen(prefix)  (string with prefix removed)
// otherwise, return NULL
  if (!s || !prefix) return NULL;
  size_t len = strlen(prefix);
  if (strncmp(s, prefix, len) != 0) return NULL;
  return s+len;
}

inline const char *strnstr(const char *haystack, const char *needle, size_t len) {
// if len >  0: use only len characters of needle
// if len == 0: use all (strlen(needle)) characters of needle

  if (len == 0) return strstr(haystack, needle);
  for (;(haystack = strchr(haystack, needle[0])); haystack++)
    if (!strncmp(haystack, needle, len)) return haystack;
  return 0;
}

inline const char *strstr_word (const char *haystack, const char *needle, size_t len = 0) {
// as strstr, but needle must be a word (surrounded by non-alphanumerical characters)
// if len >  0: use only len characters of needle
// if len == 0: use strlen(needle) characters of needle
  if (!haystack || !needle || !(*needle) ) return NULL;
  size_t len2 = (len == 0) ? strlen(needle) : len;
  if (len2 == 0) return NULL;
  for (const char *f = strnstr(haystack, needle, len); f && *(f+1); f = strnstr (f + 1, needle, len) ) {
    if (f != haystack   && isalpha(*(f-1) )) continue;
    if (f[len2] != 0 && isalpha(f[len2]) ) continue;
    return f;
  }
  return NULL;
}
inline size_t strstr_word (cSv haystack, cSv needle) {
  size_t pos, pos0;
  for (pos0 = 0; (pos = haystack.find(needle, pos0)) != std::string::npos; pos0 = pos + 1) {
    if (pos > 0 && isalpha(haystack[pos - 1])) continue;
    if (pos + needle.length() < haystack.length() && isalpha(haystack[pos + needle.length()])) continue;
    return pos;
  }
  return std::string::npos;
}

// ================ Channels ============================================

inline bool operator< (const tChannelID &c1, const tChannelID &c2) {
  if (c1.Source() != c2.Source() ) return c1.Source() < c2.Source();
  if (c1.Nid() != c2.Nid() ) return c1.Nid() < c2.Nid();
  if (c1.Tid() != c2.Tid() ) return c1.Tid() < c2.Tid();
  if (c1.Sid() != c2.Sid() ) return c1.Sid() < c2.Sid();
  return c1.Rid() < c2.Rid();
}

template <size_t N>
inline cToSvConcat<N> &stringAppendChannel(cToSvConcat<N> &target, const tChannelID &channelID, char point = '.', char minus = '-') {
  const int st_Mask = 0xFF000000;
  const int st_Pos  = 0x0000FFFF;
  target.concat((char) ((channelID.Source() & st_Mask) >> 24));
  if (int16_t n = channelID.Source() & st_Pos) {
    char ew = 'E';
    if (n < 0) {
      ew = 'W';
      n = -n;
    }
    uint16_t q = (uint16_t)n / 10;
    target.concat(q, point, (char)((uint16_t)n - 10*q + '0'), ew);
  }
  target.concat(minus, channelID.Nid(), minus, channelID.Tid(), minus, channelID.Sid());
  if (channelID.Rid() ) target.concat(minus, channelID.Rid() );
  return target;
}
template <size_t N>
inline cToSvConcat<N>& operator<<(cToSvConcat<N>& s, const tChannelID &channelID) {
  return stringAppendChannel(s, channelID);
}
inline void stringAppend(std::string &str, const tChannelID &channelID) {
  str.append(cToSvConcat(channelID));
}
inline std::ostream& operator<<(std::ostream& os, tChannelID const& id) {
  return os << cToSvConcat(id);
}
// https://stackoverflow.com/questions/26994485/can-conversion-functions-be-non-member-functions
// inline operator tChannelID(cSv sv) { return stringToObj<tChannelID>(sv); }
// tChannelID
template<class T, std::enable_if_t<std::is_same_v<T, tChannelID>, bool> = true>
inline T lexical_cast(cSv sv, T returnOnError = T(), const char *context = nullptr) {
  char buf[sv.length()+1];
  buf[sv.length()] = 0;
  memcpy(buf, sv.data(), sv.length());
  tChannelID ret = tChannelID::FromString(buf);
  if (context && !ret.Valid() )
    isyslog(PLUGIN_NAME_I18N ": WARNING, converted \"%.*s\" to invalid tChannelID, context %s", (int)sv.length(), sv.data(), context);
  return ret;
}
/*
class ctChannelID_iterator: public cSplit::iterator {
  public:
    ctChannelID_iterator(const cSplit::iterator &si): cSplit::iterator(si) {}
    tChannelID operator*() const {
      return lexical_cast<tChannelID>(cSplit::iterator::operator *() );
    }
};
*/

// =========================================================
// special container:
//   set, can contain string or integer or channel
//        or any other object wher methods to convert from/to string are available
// =========================================================

// delimiter at end of string, but not at beginning of string
// used to save data in setup.conf
template<class C>
std::string getStringFromSet(const C &in, char delim = ';') {
  using T = typename std::iterator_traits<typename C::iterator>::value_type;
  cToSvConcat result;
  for (const T &i: in) {
    result << i << delim;
  }
  return std::string(result);
}


template<class T> void insertObject(std::vector<T> &cont, const T &obj) { cont.push_back(obj); }
template<class T> void insertObject(cSortedVector<T> &cont, const T &obj) { cont.insert(obj); }

template<class T, class C=cSortedVector<T>>
C getSetFromString(cSv sv, char delim = ';') {
// split str at delim, and add each part to result
  C result;
  cSplit split(sv, delim, eSplitDelimBeginEnd::optional);
  result.reserve(split.size());
  for (cSv part: split)
    insertObject<T>(result, lexical_cast<T>(part) );
  return result;
}

// =========================================================
// stringToVector:
//   delimiter is '|'. Add only unique, non-empty values
// =========================================================

template<class T>
void stringToVector(std::vector<T> &vec, const char *str) {
// delimiter is '|'. split with eSplitDelimBeginEnd::optional.
  cSplit split(str, '|', eSplitDelimBeginEnd::optional);
  vec.reserve(vec.size() + split.size());
  for (cSv part: split) {
    if (part.empty() ) continue;
    if (find (vec.begin(), vec.end(), part) != vec.end() ) continue;
    vec.emplace_back(part);
  }
}

#endif // __TVSCRAPERHELPERS_H

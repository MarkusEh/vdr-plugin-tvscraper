#ifndef __STRINGHELPERS_H
#define __STRINGHELPERS_H

#include <cstdarg>
#include <string>
#include <string.h>
#include <vector>
#include <algorithm>
#include <set>
#include <iostream>
#include <charconv>
#include <chrono>

#define CONVERT(result, from, fn) \
char result[fn(NULL, from) + 1]; \
fn(result, from);

#define CV_VA_NUM_ARGS_HELPER(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...)    N
#define CV_VA_NUM_ARGS(...)      CV_VA_NUM_ARGS_HELPER(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define CAT( A, B ) A ## B
#define SELECT( NAME, NUM ) CAT( NAME ## _, NUM )
#define VA_SELECT( NAME, ... ) SELECT( NAME, CV_VA_NUM_ARGS(__VA_ARGS__) )(__VA_ARGS__)

// concatenate macro based, and fastest ===============
#define CONCATENATE_START_2(result, s1, s2) \
int result##concatenate_lvls = 0; \
int result##concatenate_lvl1 = concat::numChars(s1); \
result##concatenate_lvls += result##concatenate_lvl1; \
int result##concatenate_lvl2 = concat::numChars(s2); \
result##concatenate_lvls += result##concatenate_lvl2;

#define CONCATENATE_START_3(result, s1, s2, s3) \
CONCATENATE_START_2(result, s1, s2) \
int result##concatenate_lvl3 = concat::numChars(s3); \
result##concatenate_lvls += result##concatenate_lvl3;

#define CONCATENATE_START_4(result, s1, s2, s3, s4) \
CONCATENATE_START_3(result, s1, s2, s3) \
int result##concatenate_lvl4 = concat::numChars(s4); \
result##concatenate_lvls += result##concatenate_lvl4;

#define CONCATENATE_START_5(result, s1, s2, s3, s4, s5) \
CONCATENATE_START_4(result, s1, s2, s3, s4) \
int result##concatenate_lvl5 = concat::numChars(s5); \
result##concatenate_lvls += result##concatenate_lvl5;

#define CONCATENATE_START_6(result, s1, s2, s3, s4, s5, s6) \
CONCATENATE_START_5(result, s1, s2, s3, s4, s5) \
int result##concatenate_lvl6 = concat::numChars(s6); \
result##concatenate_lvls += result##concatenate_lvl6;

#define CONCATENATE_START_7(result, s1, s2, s3, s4, s5, s6, s7) \
CONCATENATE_START_6(result, s1, s2, s3, s4, s5, s6) \
int result##concatenate_lvl7 = concat::numChars(s7); \
result##concatenate_lvls += result##concatenate_lvl7;

#define CONCATENATE_START_8(result, s1, s2, s3, s4, s5, s6, s7, s8) \
CONCATENATE_START_7(result, s1, s2, s3, s4, s5, s6, s7) \
int result##concatenate_lvl8 = concat::numChars(s8); \
result##concatenate_lvls += result##concatenate_lvl8;

#define CONCATENATE_START_9(result, s1, s2, s3, s4, s5, s6, s7, s8, s9) \
CONCATENATE_START_8(result, s1, s2, s3, s4, s5, s6, s7, s8) \
int result##concatenate_lvl9 = concat::numChars(s9); \
result##concatenate_lvls += result##concatenate_lvl9;

#define CONCATENATE_END_ADDCHARS_B(result_concatenate_buf, lvl, s) \
concat::addChars(result_concatenate_buf, lvl, s); \
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
SELECT( CONCATENATE_END, CV_VA_NUM_ARGS(__VA_ARGS__) )(result, __VA_ARGS__) \
*result##concatenate_buf = 0;

// =========================================================
// UTF8 string utilities ****************
// =========================================================

int AppendUtfCodepoint(char *&target, wint_t codepoint){
  if (codepoint <= 0x7F) {
    if (target) {
      *(target++) = (char) (codepoint);
      *target = 0;
    }
    return 1;
  }
  if (codepoint <= 0x07FF) {
    if (target) {
      *(target++) =( (char) (0xC0 | (codepoint >> 6 ) ) );
      *(target++) =( (char) (0x80 | (codepoint & 0x3F)) );
      *target = 0;
    }
    return 2;
  }
  if (codepoint <= 0xFFFF) {
    if (target) {
      *(target++) =( (char) (0xE0 | ( codepoint >> 12)) );
      *(target++) =( (char) (0x80 | ((codepoint >>  6) & 0x3F)) );
      *(target++) =( (char) (0x80 | ( codepoint & 0x3F)) );
      *target = 0;
    }
    return 3;
  }
    if (target) {
      *(target++) =( (char) (0xF0 | ((codepoint >> 18) & 0x07)) );
      *(target++) =( (char) (0x80 | ((codepoint >> 12) & 0x3F)) );
      *(target++) =( (char) (0x80 | ((codepoint >>  6) & 0x3F)) );
      *(target++) =( (char) (0x80 | ( codepoint & 0x3F)) );
      *target = 0;
    }
  return 4;
}

void stringAppendUtfCodepoint(std::string &target, wint_t codepoint){
  if (codepoint <= 0x7F){
     target.push_back( (char) (codepoint) );
     return;
  }
  if (codepoint <= 0x07FF) {
     target.push_back( (char) (0xC0 | (codepoint >> 6 ) ) );
     target.push_back( (char) (0x80 | (codepoint & 0x3F)) );
     return;
  }
  if (codepoint <= 0xFFFF) {
     target.push_back( (char) (0xE0 | ( codepoint >> 12)) );
     target.push_back( (char) (0x80 | ((codepoint >>  6) & 0x3F)) );
     target.push_back( (char) (0x80 | ( codepoint & 0x3F)) );
     return;
  }
     target.push_back( (char) (0xF0 | ((codepoint >> 18) & 0x07)) );
     target.push_back( (char) (0x80 | ((codepoint >> 12) & 0x3F)) );
     target.push_back( (char) (0x80 | ((codepoint >>  6) & 0x3F)) );
     target.push_back( (char) (0x80 | ( codepoint & 0x3F)) );
     return;
}

inline int utf8CodepointIsValid(const char *p){
// In case of invalid UTF8, return 0
// otherwise, return number of characters for this UTF codepoint
  static const uint8_t LEN[] = {2,2,2,2,3,3,4,0};

  int len = ((*p & 0xC0) == 0xC0) * LEN[(*p >> 3) & 7] + ((*p | 0x7F) == 0x7F);
  for (int k=1; k < len; k++) if ((p[k] & 0xC0) != 0x80) len = 0;
  return len;
}

inline wint_t Utf8ToUtf32(const char *&p, int len) {
// assumes, that uft8 validity checks have already been done. len must be provided. call utf8CodepointIsValid first
// change p to position of next codepoint (p = p + len)
  static const uint8_t FF_MSK[] = {0xFF >>0, 0xFF >>0, 0xFF >>3, 0xFF >>4, 0xFF >>5, 0xFF >>0, 0xFF >>0, 0xFF >>0};
  wint_t val = *p & FF_MSK[len];
  const char *q = p + len;
  for (p++; p < q; p++) val = (val << 6) | (*p & 0x3F);
  return val;
}

inline wint_t getUtfCodepoint(const char *p) {
// get next codepoint
// 0 is returned at end of string
  if(!p || !*p) return 0;
  int l = utf8CodepointIsValid(p);
  if( l == 0 ) return '?';
  const char *s = p;
  return Utf8ToUtf32(s, l);
}

inline wint_t getNextUtfCodepoint(const char *&p){
// get next codepoint, and increment p
// 0 is returned at end of string, and p will point to the end of the string (0)
  if(!p || !*p) return 0;
  int l = utf8CodepointIsValid(p);
  if( l == 0 ) { p++; return '?'; }
  return Utf8ToUtf32(p, l);
}
inline int stringAppendAllASCIICharacters(std::string &target, const char *str) {
// append all characters > 31 (signed !!!!). Unsigned: 31 < character < 128
// return number of appended characters
  int i = 0;
//  for (const signed char *strs = reinterpret_cast<const signed char *>(str); strs[i] > 31; i++);
  for (; reinterpret_cast<const signed char*>(str)[i] > 31; i++);
  target.append(str, i);
  return i;
}
void StringRemoveTrailingWhitespace(std::string &str);
void stringAppendRemoveControlCharacters(std::string &target, const char *str) {
  for(;;) {
    str += stringAppendAllASCIICharacters(target, str);
    wint_t cp = getNextUtfCodepoint(str);
    if (cp == 0) { StringRemoveTrailingWhitespace(target); return; }
    if (cp > 31) stringAppendUtfCodepoint(target, cp);
    else target.append(" ");
  }
}
void stringAppendRemoveControlCharactersKeepNl(std::string &target, const char *str) {
  for(;;) {
    str += stringAppendAllASCIICharacters(target, str);
    wint_t cp = getNextUtfCodepoint(str);
    if (cp == 0) { StringRemoveTrailingWhitespace(target); return; }
    if (cp == ' ' && str[1] == '\n') target.append("\n");
    else if (cp > 31 || cp == '\n') stringAppendUtfCodepoint(target, cp);
    else target.append(" ");
  }
}

// =========================================================
// methods for char *s, make sure that s==NULL is just an empty string ========
// =========================================================
inline std::string charPointerToString(const unsigned char *s) {
  return s?reinterpret_cast<const char *>(s):"";
}
inline std::string_view charPointerToStringView(const unsigned char *s) {
  return s?reinterpret_cast<const char *>(s):std::string_view();
}
inline std::string_view charPointerToStringView(const char *s) {
  return s?s:std::string_view();
}
inline std::string charPointerToString(const char *s) {
  return s?s:"";
}

bool stringEqual(const char *s1, const char *s2) {
// return true if texts are identical (or both texts are NULL)
  if (s1 && s2) return strcmp(s1, s2) == 0;
  if (!s1 && !s2 ) return true;
  if (!s1 && !*s2 ) return true;
  if (!*s1 && !s2 ) return true;
  return false;
}

// =========================================================
// some performance improvemnt, to get string presentation for channel ===========
// you can also use channelID.ToString()
// =========================================================

void sourceToBuf(char *buffer, int Code) {
//char buffer[16];
  int st_Mask = 0xFF000000;
  char *q = buffer;
  *q++ = (Code & st_Mask) >> 24;
  if (int n = cSource::Position(Code)) {
     q += snprintf(q, 14, "%u.%u", abs(n) / 10, abs(n) % 10); // can't simply use "%g" here since the silly 'locale' messes up the decimal point
     *q++ = (n < 0) ? 'W' : 'E';
     }
  *q = 0;
}
void channelToBuf(char *buffer, const tChannelID &channelID) {
// char buffer[256];
  char buffer_src[16];
  sourceToBuf(buffer_src, channelID.Source());
  snprintf(buffer, 256, channelID.Rid() ? "%s-%d-%d-%d-%d" : "%s-%d-%d-%d",
     buffer_src, channelID.Nid(), channelID.Tid(), channelID.Sid(), channelID.Rid() );
}
std::string channelToString(const tChannelID &channelID) {
  char buffer[256];
  channelToBuf(buffer, channelID);
  return buffer;
}

// =========================================================
// =========== concatenate ===========================================
// =========================================================

std::string concatenate(const char *s1, const char *s2) {
  if (!s1 && !s2) return "";
  if (!s1) return s2;
  if (!s2) return s1;
  std::string result;
  result.reserve(strlen(s1) + strlen(s2));
  result.append(s1);
  result.append(s2);
  return result;
}

std::string concatenate(const char *s1, const char *s2, const char *s3) {
  if (!s1) return concatenate(s2, s3);
  if (!s2) return concatenate(s1, s3);
  if (!s3) return concatenate(s1, s2);
  std::string result;
  result.reserve(strlen(s1) + strlen(s2) + strlen(s3) );
  result.append(s1);
  result.append(s2);
  result.append(s3);
  return result;
}

namespace concat {
  template<class T> inline int numCharsU(T i) {
// note: i must be >= 0!!!!
    if (i < 10) return 1;
    int numChars;
    for (numChars = 0; i; i /= 10) numChars++;
    return numChars;
  }
  inline int numChars(const std::string_view &s) {
    return s.length();
  }
  inline int numChars(const std::string &s) {
    return s.length();
  }
  inline int numChars(const char *s) {
    return s?strlen(s):0;
  }
  inline int numChars(int i) {
    if (i >=0 ) return numCharsU(i);
    return numCharsU(-1*i) + 1;
  }
  inline void addCharsU(char *b, int l, unsigned int i) {
    if (i < 10) *b = '0' + i;
    else for (b += l; i; i /= 10) *(--b) = '0' + (i%10);
  }
  inline void addChars(char *b, int l, int i) {
    if (i >=0 ) addCharsU(b, l, i);
    else {
      *b = '-';
      addCharsU(++b, --l, -1*i);
    }
  }
  inline void addChars(char *b, int l, const std::string_view &s) {
    memcpy(b, s.data(), l);
  }
  inline void addChars(char *b, int l, const std::string &s) {
    memcpy(b, s.data(), l);
  }
  inline void addChars(char *b, int l, const char *s) {
    memcpy(b, s, l);
  }

// methods to append to std::strings ========================
  template<typename T>
  inline void stringAppendU(std::string &str, T i) {
// for integer types i >= 0 !!!! This is not checked !!!!!
    char buf[21]; // unsigned int 64: max. 20. (18446744073709551615) signed int64: max. 19 (+ sign)
    char *bufferEnd = buf+20;
    *bufferEnd = 0;
    if (i < 10) *(--bufferEnd) = '0' + i;
    else for (; i; i /= 10) *(--bufferEnd) = '0' + (i%10);
    str.append(bufferEnd);
  }
  template<typename T>
  inline void stringAppendI(std::string &str, T i) {
// for integer types i
    if (i >= 0) stringAppendU(str, i);
    else { str.append("-"); stringAppendU(str, -1*i); }
  }
}
inline void stringAppend(std::string &str, unsigned int i) { concat::stringAppendU(str, i); }
inline void stringAppend(std::string &str, unsigned long int i) { concat::stringAppendU(str, i); }
inline void stringAppend(std::string &str, unsigned long long  int i) { concat::stringAppendU(str, i); }

inline void stringAppend(std::string &str, int i) { concat::stringAppendI(str, i); }
inline void stringAppend(std::string &str, long int i) { concat::stringAppendI(str, i); }
inline void stringAppend(std::string &str, long long int i) { concat::stringAppendI(str, i); }

// strings
inline void stringAppend(std::string &str, const char *s) { str.append(s); }
inline void stringAppend(std::string &str, const std::string &s) { str.append(s); }
inline void stringAppend(std::string &str,       std::string_view s) { str.append(s); }

// tChannelID
inline void stringAppend(std::string &str, const tChannelID &channelID) {
  char buffer[256];
  channelToBuf(buffer, channelID);
  str.append(buffer);
}

template<typename T, typename... Args>
void stringAppend(std::string &str, const T &n, const Args&... args) {
  stringAppend(str, n);
  stringAppend(str, args...);
}
template<typename... Args>
std::string concatenate(const Args&... args) {
  std::string result;
  result.reserve(200);
//stringAppend(result, std::forward<Args>(args)...);
  stringAppend(result, args...);
  return result;
}

// __attribute__ ((format (printf, 2, 3))) can not be used, but should work starting with gcc 13.1
template<typename... Args>
void stringAppendFormated(std::string &str, const char *fmt, const Args&... args) {
  size_t size = 1024;
  char buf[size];
  int needed = snprintf (&buf[0], size, fmt, args...);
  if (needed < 0) return; // error in snprintf
  if ((size_t)needed < size) {
    str.append(buf);
  } else {
    char buf2[needed + 1];
    sprintf (&buf2[0], fmt, args...);
    str.append(buf2);
  }
}
class cConcatenate
{
  public:
    cConcatenate(size_t buf_size = 0) { m_data.reserve(buf_size>0?buf_size:200); }
    cConcatenate(const cConcatenate&) = delete;
    cConcatenate &operator= (const cConcatenate &) = delete;
  template<typename T>
    cConcatenate & operator<<(const T &i) { stringAppend(m_data, i); return *this; }
    std::string moveStr() { return std::move(m_data); }
    const std::string &getStrRef() { return m_data; }
    const char *getCharS() { return m_data.c_str(); }
  private:
    std::string m_data;
};

// whitespace ================================================================
void StringRemoveTrailingWhitespace(std::string &str) {
  const char*  whitespaces = " \t\f\v\n\r";

  std::size_t found = str.find_last_not_of(whitespaces);
  if (found!=std::string::npos)
    str.erase(found+1);
  else
    str.clear();            // str is all whitespace
}

int StringRemoveTrailingWhitespace(const char *str, int len) {
// return "new" len of string, without whitespaces at the end
  if (!str || !*str) return 0;
  const char*  whitespaces = " \t\f\v\n\r";
  for (; len; len--) if (strchr(whitespaces, str[len - 1]) == NULL) return len;
  return 0;
}
void StringRemoveTrailingWhitespace(std::string_view &str) {
  str.remove_suffix(str.length() - StringRemoveTrailingWhitespace(str.data(), str.length()));
}

const char *strnstr(const char *haystack, const char *needle, size_t len) {
// if len >  0: use only len characters of needle
// if len == 0: use all (strlen(needle)) characters of needle

  if (len == 0) return strstr(haystack, needle);
  for (;(haystack = strchr(haystack, needle[0])); haystack++)
    if (!strncmp(haystack, needle, len)) return haystack;
  return 0;
}

const char *strstr_word (const char *haystack, const char *needle, size_t len = 0) {
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

std::string_view textAttributeValue(const char *text, const char *attributeName) {
  if (!text || !attributeName) return std::string_view();
  const char *found = strstr(text, attributeName);
  if (!found) return std::string_view();
  const char *avs = found + strlen(attributeName);
  const char *ave = strchr(avs, '\n');
  if (!ave) return std::string_view();
  return std::string_view(avs, ave-avs);
}

bool splitString(std::string_view str, std::string_view delim, size_t minLengh, std::string_view &first, std::string_view &second) {
// true if delim is part of str, and length of first & second >= minLengh
  std::size_t found = str.find(delim);
  size_t first_len = 0;
  while (found != std::string::npos) {
    first_len = StringRemoveTrailingWhitespace(str.data(), found);
    if (first_len >= minLengh) break;
    found = str.find(delim, found + 1);
  }
//  std::cout << "first_len " << first_len << " found " << found << "\n";
  if(first_len < minLengh) return false; // nothing found

  std::size_t ssnd;
  for(ssnd = found + delim.length(); ssnd < str.length() && str[ssnd] == ' '; ssnd++);
  if(str.length() - ssnd < minLengh) return false; // nothing found, second part to short

  second = str.substr(ssnd);
  first = str.substr(0, first_len);
  return true;
}

bool splitString(std::string_view str, char delimiter, size_t minLengh, std::string_view &first, std::string_view &second) {
  using namespace std::literals::string_view_literals;
  if (delimiter == '-') return splitString(str, " - "sv, minLengh, first, second);
  if (delimiter == ':') return splitString(str, ": "sv, minLengh, first, second);
  std::string delim(1, delimiter);
  return splitString(str, delim, minLengh, first, second);
}

std::string_view SecondPart(std::string_view str, std::string_view delim, size_t minLengh) {
// return second part of split string if delim is part of str, and length of first & second >= minLengh
// otherwise, return ""
  std::string_view first, second;
  if (splitString(str, delim, minLengh, first, second)) return second;
  else return "";
}

std::string_view SecondPart(std::string_view str, std::string_view delim) {
// Return part of str after first occurence of delim
// if delim is not in str, return ""
  size_t found = str.find(delim);
  if (found == std::string::npos) return std::string_view();
  std::size_t ssnd;
  for(ssnd = found + delim.length(); ssnd < str.length() && str[ssnd] == ' '; ssnd++);
  return str.substr(ssnd);
}

int StringRemoveLastPartWithP(const char *str, int len) {
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

bool StringRemoveLastPartWithP(std::string &str) {
// remove part with (...)
  int len = StringRemoveLastPartWithP(str.c_str(), str.length() );
  if (len < 0) return false;
  str.erase(len);
  return true;
}
std::string_view removeLastPartWithP(std::string_view str) {
  int l = StringRemoveLastPartWithP(str.data(), str.length() );
  if (l < 0) return str;
  return std::string_view(str.data(), l);
}

int NumberInLastPartWithPS(std::string_view str) {
// return number in last part with (./.), 0 if not found / invalid
  if (str.length() < 3 ) return 0;
  if (str[str.length() - 1] != ')') return 0;
  std::size_t found = str.find_last_of("(");
  if (found == std::string::npos) return 0;
  for (std::size_t i = found + 1; i < str.length() - 1; i ++) {
    if (!isdigit(str[i]) && str[i] != '/') return 0; // we gnore (asw), and return only number with digits only
  }
  return atoi(str.data() + found + 1);
}
int NumberInLastPartWithP(std::string_view str) {
// return number in last part with (...), 0 if not found / invalid
  if (str.length() < 3 ) return 0;
  if (str[str.length() - 1] != ')') return 0;
  std::size_t found = str.find_last_of("(");
  if (found == std::string::npos) return 0;
  for (std::size_t i = found + 1; i < str.length() - 1; i ++) {
    if (!isdigit(str[i])) return 0; // we ignore (asw), and return only number with digits only
  }
  return atoi(str.data() + found + 1);
}

// methods for years =============================================================

class cYears {
  public:
    cYears() { m_years[0] = 0; }
    cYears(const cYears&) = delete;
    cYears &operator= (const cYears &) = delete;
// note: iterate first over exact matches, then near matches
//    class iterator: public std::iterator<std::forward_iterator_tag, int, int, const int*, int>
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
      std::string_view year_sv = textAttributeValue(str, "Jahr: ");
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

// convert containers to strings, and strings to containers ==================
template<class T>
void push_back_new(std::vector<T> &vec, const T &str) {
// add str to vec, but only if str is not yet in vec and if str is not empty
  if (str.empty() ) return;
  if (find (vec.begin(), vec.end(), str) != vec.end() ) return;
  vec.push_back(str);
}

template<class T>
void stringToVector(std::vector<T> &vec, const char *str) {
// if str does not start with '|', don't split, just add str to vec
// otherwise, split str at '|', and add each part to vec
  if (!str || !*str) return;
  if (str[0] != '|') { vec.push_back(str); return; }
  const char *lDelimPos = str;
  for (const char *rDelimPos = strchr(lDelimPos + 1, '|'); rDelimPos != NULL; rDelimPos = strchr(lDelimPos + 1, '|') ) {
    push_back_new(vec, T(lDelimPos + 1, rDelimPos - lDelimPos - 1));
    lDelimPos = rDelimPos;
  }
}

std::string vectorToString(const std::vector<std::string> &vec) {
  if (vec.size() == 0) return "";
  if (vec.size() == 1) return vec[0];
  std::string result("|");
  for (const std::string &str: vec) { result.append(str); result.append("|"); }
  return result;
}

std::string objToString(const int &i) { return std::to_string(i); }
std::string objToString(const std::string &i) { return i; }
std::string objToString(const tChannelID &i) { return channelToString(i); }

template<class T, class C=std::set<T>>
std::string getStringFromSet(const C &in, char delim = ';') {
  if (in.size() == 0) return "";
  std::string result;
  if (delim == '|') result.append(1, delim);
  for (const T &i: in) {
    result.append(objToString(i));
    result.append(1, delim);
  }
  return result;
}

template<class T> T stringToObj(const char *s, size_t len) {
  esyslog("tvscraper: ERROR: template<class T> T stringToObj called");
  return 5; }
template<> int stringToObj<int>(const char *s, size_t len) { return atoi(s); }
template<> std::string stringToObj<std::string>(const char *s, size_t len) { return std::string(s, len); }
template<> std::string_view stringToObj<std::string_view>(const char *s, size_t len) { return std::string_view(s, len); }
template<> tChannelID stringToObj<tChannelID>(const char *s, size_t len) {
  return tChannelID::FromString(std::string(s, len).c_str());
}

template<class T> void insertObject(std::vector<T> &cont, const T &obj) { cont.push_back(obj); }
template<class T> void insertObject(std::set<T> &cont, const T &obj) { cont.insert(obj); }

template<class T, class C=std::set<T>>
C getSetFromString(const char *str, char delim = ';') {
// split str at delim';', and add each part to result
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

const char *strchr_se(const char *ss, const char *se, char ch) {
  for (const char *sc = ss; sc < se; sc++) if (*sc == ch) return sc;
  return NULL;
}

template<class T, class C=std::set<T>>
C getSetFromString(std::string_view str, char delim, const std::set<std::string_view> &ignoreWords, int max_len = 0) {
// split str at delim, and add each part to result
  C result;
  if (str.empty() ) return result;
  const char *lCurrentPos = str.data();
  const char *lEndPos = str.data() + str.length();
  const char *lSoftEndPos = max_len == 0?lEndPos:str.data() + max_len;
  for (const char *rDelimPos = strchr_se(lCurrentPos, lEndPos, delim); rDelimPos != NULL; rDelimPos = strchr_se(lCurrentPos, lEndPos, delim) ) {
    if (ignoreWords.find(std::string_view(lCurrentPos, rDelimPos - lCurrentPos)) == ignoreWords.end()  )
      insertObject<T>(result, stringToObj<T>(lCurrentPos, rDelimPos - lCurrentPos));
    lCurrentPos = rDelimPos + 1;
    if (lCurrentPos >= lSoftEndPos) break;
  }
  if (lCurrentPos < lEndPos && lCurrentPos < lSoftEndPos) insertObject<T>(result, stringToObj<T>(lCurrentPos, lEndPos - lCurrentPos));
  return result;
}

// simple XML parser =========================================================
class cXmlString {
  public:
    cXmlString(const cXmlString &xmlString, const char *tag) {
      initialize(xmlString.m_start, xmlString.m_end, tag);
    }
    cXmlString(const char *start, const char *tag) {
      if(!start) return;
      initialize(start, start + strlen(start), tag);
    }
    cXmlString(const cXmlString&) = delete;
    cXmlString &operator= (const cXmlString &) = delete;
    std::string getString() const {
      if (!m_start || !m_end) return "";
      return std::string(m_start, m_end - m_start);
    }
    const char *data() { return m_start?m_start:""; }
    int length() { return m_start?(m_end - m_start):0; }
    int getInt() const {
      if (!m_start) return 0;
      return atoi(m_start);
    }
    long long getIntll() const {
      if (!m_start) return 0;
      char *end;
      return strtoll(m_start, &end, 10);
    }
    bool isValid() const { return m_start != NULL;}
    bool operator== (const char *sec) const {
      if (!m_start || !sec) return false;
      if (strlen(sec) != (size_t)(m_end - m_start) ) return false;
      return memcmp(m_start, sec, m_end - m_start) == 0;
    }
    bool operator== (const cXmlString &sec) const {
      return *this == sec.m_start;
    }
  private:
    void initialize(const char *start, const char *end, const char *tag) {
      if(!start || !end || !tag) return;
      size_t tag_len = strlen(tag);
      m_start = startTag(start, end, tag, tag_len);
  //    cout << "m_start: " << m_start << endl;
      if (!m_start) return;
      m_end = endTag(m_start, end, tag, tag_len);
  //    cout << "m_end: " << m_end << endl;
      if (!m_end) m_start = NULL;
    }
    const char *startTag(const char *start, const char *end, const char *tag, size_t tag_len) const {
      for (; start < end - tag_len - 1; start++)
        if (*start == '<' && strncmp(start + 1, tag, tag_len) == 0 && start[1 + tag_len] == '>') return start + 2 + tag_len;
      return NULL;
    }
    const char *endTag(const char *start, const char *end, const char *tag, size_t tag_len) const {
      for (; start < end - tag_len - 1; start++)
        if (*start == '<' && start[1] == '/' && strncmp(start + 2, tag, tag_len) == 0 && start[2 + tag_len] == '>') return start;
      return NULL;
    }
    const char *m_start = NULL;
    const char *m_end = NULL;
};

const char* removePrefix(const char *s, const char *prefix) {
// if s starts with prefix, return s + strlen(prefix)  (string with prefix removed)
// otherwise, rturn NULL
  if (!s || !prefix) return NULL;
  size_t len = strlen(prefix);
  if (strncmp(s, prefix, len) != 0) return NULL;
  return s+len;
}

class cSplit {
  public:
    cSplit(std::string_view sv, char delim): m_sv(sv), m_delim(delim), m_end(std::string_view(), m_delim) {}
    cSplit(const char *s, char delim): m_sv(charPointerToStringView(s)), m_delim(delim), m_end(std::string_view(), m_delim) {}
    cSplit(const cSplit&) = delete;
    cSplit &operator= (const cSplit &) = delete;
    class iterator {
        std::string_view m_remainingParts;
        char m_delim;
        size_t m_next_delim;
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::string_view;
        using difference_type = int;
        using pointer = const std::string_view*;
        using reference = std::string_view;

        explicit iterator(std::string_view r, char delim): m_delim(delim) {
          if (!r.empty() && r[0] == delim) m_remainingParts = r.substr(1);
          else m_remainingParts = r;
          m_next_delim = m_remainingParts.find(m_delim);
        }
        iterator& operator++() {
          if (m_next_delim == std::string_view::npos) {
            m_remainingParts = std::string_view();
          } else {
            m_remainingParts = m_remainingParts.substr(m_next_delim + 1);
            m_next_delim = m_remainingParts.find(m_delim);
          }
          return *this;
        }
        bool operator!=(iterator other) const { return m_remainingParts != other.m_remainingParts; }
        bool operator==(iterator other) const { return m_remainingParts == other.m_remainingParts; }
        std::string_view operator*() const {
          if (m_next_delim == std::string_view::npos) return m_remainingParts;
          else return m_remainingParts.substr(0, m_next_delim);
        }
      };
      iterator begin() { return iterator(m_sv, m_delim); }
      const iterator &end() { return m_end; }
      iterator find(std::string_view sv) {
        if (m_sv.find(sv) == std::string_view::npos) return m_end;
        return std::find(begin(), end(), sv);
      }
    private:
      const std::string_view m_sv;
      const char m_delim;
      const iterator m_end;
};

class cContainer {
  public:
    cContainer(char delim = '|'): m_delim(delim) { }
    cContainer(const cContainer&) = delete;
    cContainer &operator= (const cContainer &) = delete;
    bool find(std::string_view sv) {
      if (!sv.empty() ) {
        size_t f = m_buffer.find(sv);
        if (f == std::string_view::npos || f== 0 || f + sv.length() == m_buffer.length() ) return false;
        if (m_buffer[f-1] == m_delim && m_buffer[f+sv.length()] == m_delim) return true;
      }
//      std::cout << " second check ";
      CONCATENATE(ns, "|", sv, "|");
      size_t f = m_buffer.find(ns);
      return f != std::string_view::npos;
    }
    bool insert(std::string_view sv) {
// true, if already in buffer (will not insert again ...)
// else: false
      if (m_buffer.empty() ) {
        m_buffer.reserve(300);
        m_buffer.append(1, m_delim);
      } else if (find(sv)) return true;
      m_buffer.append(sv);
      m_buffer.append(1, m_delim);
      return false;
    }
    bool insert(const char *s) {
      if (!s) return true;
      return insert(std::string_view(s));
    }
    std::string moveBuffer() { return std::move(m_buffer); }
    const std::string &getBufferRef() { return m_buffer; }
  private:
    char m_delim;
    std::string m_buffer;
};

class cMeasureTime {
  public:
    void start() { begin = std::chrono::high_resolution_clock::now(); }
    void stop()  {
      std::chrono::duration<double> timeNeeded = std::chrono::high_resolution_clock::now() - begin;
      maxT = std::max(maxT, timeNeeded);
      sumT += timeNeeded;
      ++numCalls;
    }
    void reset() {
      sumT = std::chrono::duration<double>(0);
      maxT = std::chrono::duration<double>(0);
      numCalls = 0;
    }
    void add(const cMeasureTime &other) {
      maxT = std::max(maxT, other.maxT);
      sumT += other.sumT;
      numCalls += other.numCalls;
    }
    void print(const char *context) const {
      if (numCalls == 0) return;
      if (!context) context = "cMeasureTime";
      esyslog("tvscraper: %s num = %5i, time = %9.5f, average %f, max = %f", context, numCalls, sumT.count(), sumT.count()/numCalls, maxT.count());
    }

  private:
    int numCalls = 0;
    std::chrono::duration<double> sumT = std::chrono::duration<double>(0.);
    std::chrono::duration<double> maxT = std::chrono::duration<double>(0.);
    std::chrono::time_point<std::chrono::high_resolution_clock> begin;
};
int getAsInt(std::string_view sv) {
// read int in sv
// return 0 is there is no int
// ignore everything in sv after the first non-digit
  int val = 0;
  for (size_t p = 0; isdigit(sv[p]) && p < sv.length(); ++p) val = val*10 + (sv[p]-'0');
  return val;
}
int seasonS(std::string_view description_part, const char *S) {
// return season, if found at the beginning of description_part
// otherwise, return -1
  size_t s_len = strlen(S);
  if (description_part.compare(0, s_len, S)  != 0) return -1;
//  std::cout << "seasonS " << description_part << "\n";
  if (description_part.length() <= s_len || !isdigit(description_part[s_len]) ) return -1;
  return getAsInt(description_part.substr(s_len));
}
bool episodeSEp(int &season, int &episode, std::string_view description, const char *S, const char *Ep) {
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
    if (ep_pos == std::string_view::npos || ep_pos + Ep_len > description.length() ) return false;  // no Ep[digit]
    ep_pos += Ep_len;
//  std::cout << "ep_pos = " << ep_pos << "\n";
  } while (!isdigit(description[ep_pos]));
// Ep[digit] found
//  std::cout << "ep found at " << description.substr(ep_pos) << "\n";
  episode = getAsInt(description.substr(ep_pos));
  if (ep_pos - Ep_len >= 3) season = seasonS(description.substr(ep_pos - Ep_len - 3), S);
  if (season < 0 && ep_pos - Ep_len >= 4) season = seasonS(description.substr(ep_pos - Ep_len - 4), S);
  return true;
}

#endif // __STRINGHELPERS_H

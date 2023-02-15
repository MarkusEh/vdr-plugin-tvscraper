#include <cstdarg>
#include <string>
#include <string.h>
#include <vector>
#include <algorithm>
#include <set>
#include <iostream>

#define CONCAT(result, fmt, ...) \
char result[concat::sprint(NULL, fmt, __VA_ARGS__) + 1]; \
concat::sprint(result, fmt, __VA_ARGS__);

#define CONVERT(result, from, fn) \
char result[fn(NULL, from) + 1]; \
fn(result, from);


using namespace std;

// UTF8 string utilities ****************
/*
int sprintWchar(char *to, wint_t cp) {
}
*/
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

void AppendUtfCodepoint(std::string &target, wint_t codepoint){
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

int utf8CodepointIsValid(const char *p){
// In case of invalid UTF8, return 0
// otherwise, return number of characters for this UTF codepoint
  static const uint8_t LEN[] = {2,2,2,2,3,3,4,0};

  int len = ((*p & 0xC0) == 0xC0) * LEN[(*p >> 3) & 7] + ((*p | 0x7F) == 0x7F);
  for (int k=1; k < len; k++) if ((p[k] & 0xC0) != 0x80) len = 0;
  return len;
}

wint_t Utf8ToUtf32(const char *&p, int len) {
// assumes, that uft8 validity checks have already been done. len must be provided. call utf8CodepointIsValid first
// change p to position of next codepoint (p = p + len)
  static const uint8_t FF_MSK[] = {0xFF >>0, 0xFF >>0, 0xFF >>3, 0xFF >>4, 0xFF >>5, 0xFF >>0, 0xFF >>0, 0xFF >>0};
  wint_t val = *p & FF_MSK[len];
  const char *q = p + len;
  for (p++; p < q; p++) val = (val << 6) | (*p & 0x3F);
  return val;
}

wint_t getUtfCodepoint(const char *p) {
// get next codepoint
// 0 is returned at end of string
  if(!p || !*p) return 0;
  int l = utf8CodepointIsValid(p);
  if( l == 0 ) return '?';
  const char *s = p;
  return Utf8ToUtf32(s, l);
}

wint_t getNextUtfCodepoint(const char *&p){
// get next codepoint, and increment p
// 0 is returned at end of string, and p will point to the end of the string (0)
  if(!p || !*p) return 0;
  int l = utf8CodepointIsValid(p);
  if( l == 0 ) { p++; return '?'; }
  return Utf8ToUtf32(p, l);
}
void appendRemoveControlCharacters(std::string &target, const char *str) {
  for(;;) {
    wint_t cp = getNextUtfCodepoint(str);
    if (cp == 0) return;
    if (cp > 31) AppendUtfCodepoint(target, cp);
    else target.append(" ");
  }
}
void appendRemoveControlCharactersKeepNl(std::string &target, const char *str) {
  for(;;) {
    wint_t cp = getNextUtfCodepoint(str);
    if (cp == 0) return;
    if (cp == ' ' && str[1] == '\n') target.append("\n");
    else if (cp > 31 || cp == '\n') AppendUtfCodepoint(target, cp);
    else target.append(" ");
//     else { target.append("#"); target.append(to_string(cp)); target.append(";"); AppendUtfCodepoint(target, cp); }
  }
}

// methods for char *s, make sure that s==NULL is just an empty string ========
std::string charPointerToString(const unsigned char *s) {
  return s?(const char *)s:"";
}
std::string charPointerToString(const char *s) {
  return s?s:"";
}

bool stringEqual(const char *s1, const char *s2) {
// return true if texts are identical (or both texts are NULL)
  if (s1 && s2) {
    if (strcmp(s1, s2) == 0) return true;
    else return false;
  }
  if (!s1 && !s2 ) return true;
  if (!s1 && !*s2 ) return true;
  if (!*s1 && !s2 ) return true;
  return false;
}

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

// std::string_view to_sv : increase type safety in ... methods  ================
inline std::string_view to_sv(const char *s) {
  if (s) return std::string_view(s);
  return std::string_view();
}
inline std::string_view to_sv(const std::string &S) {
  return S;
}
inline std::string_view to_sv(const std::string_view &V) {
  return V;
}

namespace concat {
  template<class T> int toCharsU(char *buffer, T i0) {
// notes:
//    i0 must be unsigned !!!!
//    return number of characters written to buffer  (don't count 0 terminator)
//    if buffer==NULL: don't write anything, just return the number
//    sizeof(buffer) must be >= this return value + 1 !! This is not checked ....
    int numChars;
    int i = i0;
    if (i < 10) numChars = 1;
    else for (numChars = 0; i; i /= 10) numChars++;
    if (!buffer) return numChars;
    char *bufferEnd = buffer + numChars;
    i = i0;
    *bufferEnd = 0;
    if (i < 10) *(--bufferEnd) = '0' + i;
    else for (; i; i /= 10) *(--bufferEnd) = '0' + (i%10);
    return numChars;
  }
  template<class T> int toCharsI(char *buffer, T i) {
    if (i >= 0) return toCharsU(buffer, i);
    if (buffer) *(buffer++) = '-';
    return toCharsU(buffer, -1*i) + 1;
  }

  typedef int (*tSprint)(char *to, std::string_view from);
// convert from to to
// return number of characters written to to  (don't count 0 terminator)
// if to==NULL: don't write anything, just return the number

  int vsprint(char *buffer, const char *fmt, va_list &vl) {
  // return number of characters required to concatenate
  // note: sizeof(buffer) must be this number + 1 (terminating 0)
  // va_list &vl: list of objects to concatenate
  // fmt:
  //    s: const char *        must be zero terminated
  //    V: std::string_view    use this and to_sv(...) for strings
  //    u: unsigned int
  //    i: int
  //    t: time_t
  //    w: 2 parameters: const char *, conversion function type tSprint
  //    W: 2 parameters: string_view , conversion function type tSprint
    if (!fmt) { if (buffer) *buffer = 0; return 0; }
    int numChars = 0;
    char *s;
    std::string_view sv;
    tSprint wf;
    for (; *fmt; fmt++) {
      switch (*fmt) {
        case 's':
          s = va_arg(vl,char*);
          if (s) {
            if (buffer) strcpy(buffer + numChars, s);
            numChars += strlen(s);
          }
          break;
        case 'V':
          sv = va_arg(vl,std::string_view);
          if (buffer) strncpy(buffer + numChars, sv.data(), sv.length());
          numChars += sv.length();
          break;
        case 'i':
          numChars += toCharsI(buffer?buffer + numChars:NULL, va_arg(vl,int));
          break;
        case 'u':
          numChars += toCharsU(buffer?buffer + numChars:NULL, va_arg(vl,unsigned int));
          break;
        case 't':
          numChars += toCharsU(buffer?buffer + numChars:NULL, va_arg(vl,time_t));
          break;
        case 'W':
          sv = va_arg(vl,std::string_view);
          wf = va_arg(vl,tSprint);
          numChars += (*wf)(buffer?buffer + numChars:NULL, sv);
          break;
        case 'w':
          s = va_arg(vl,char*);
          wf = va_arg(vl,tSprint);
          if (s) numChars += (*wf)(buffer?buffer + numChars:NULL, std::string_view(s));
          break;
        default:
          esyslog("tvscraper: ERROR in stringhelpers->concat::vsprint, fmt: %s, unknown %c", fmt, *fmt);
          va_arg(vl, void*);
      }
    }
    if (buffer) *(buffer + numChars) = 0;
    return numChars;
  }
  int sprint(char *buffer, const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    int result = vsprint(buffer, fmt, vl);
    va_end(vl);
    return result;
  }
}
std::string concatenateAll(const char *fmt, ...) {
// see numCharsAll for fmt
  if (!fmt) return "";
  va_list vl1, vl2;
  va_start(vl1, fmt);
  va_copy(vl2, vl1);
  char buffer[concat::vsprint(NULL, fmt, vl1) + 1];
  va_end(vl1);
  concat::vsprint(buffer, fmt, vl2);
  va_end(vl2);
  return buffer;
}

// whitespace ================================================================
void StringRemoveTrailingWhitespace(string &str) {
  std::string whitespaces (" \t\f\v\n\r");

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

  second = std::string_view(str).substr(ssnd);
  first = std::string_view(str).substr(0, first_len);
  return true;
}

bool splitString(std::string_view str, char delimiter, size_t minLengh, std::string_view &first, std::string_view &second) {
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
  if (found == std::string::npos) return "";
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

bool StringRemoveLastPartWithP(string &str) {
// remove part with (...)
  int len = StringRemoveLastPartWithP(str.c_str(), str.length() );
  if (len < 0) return false;
  str.erase(len);
  return true;
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

int yearToInt(const char *year) {
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

int stringToYear(const string &str) {
  if (str.length() > 4 && isdigit(str[4]) ) return 0;
  return yearToInt(str.c_str() );
}

const char *firstDigit(const char *found) {
  for (; ; found++) if (isdigit(*found) || ! *found) return found;
}
const char *firstNonDigit(const char *found) {
  for (; ; found++) if (!isdigit(*found) || ! *found) return found;
}

void AddYear(vector<int> &years, int year) {
  if (year <= 1920 || year >= 2100) return;
  if (find(years.begin(), years.end(), year) == years.end()) years.push_back(year);
  int yearp1 = ( year + 1 ) * (-1);
  if (find(years.begin(), years.end(), yearp1) == years.end()) years.push_back(yearp1);
  int yearm1 = ( year - 1 ) * (-1);
  if (find(years.begin(), years.end(), yearm1) == years.end()) years.push_back(yearm1);
}

void AddYears(vector<int> &years, const char *str) {
  if (!str) return;
  std::string_view year_sv = textAttributeValue(str, "Jahr: ");
  if (year_sv.length() == 4) {
    AddYear(years, yearToInt(year_sv.data()));
//  if (config.enableDebug) esyslog("tvscraper, Jahr: %i", yearToInt(year_sv.data()));
    return;
  }
  const char *last;
  for (const char *first = firstDigit(str); *first; first = firstDigit(last) ) {
    last = firstNonDigit(first);
    if (last - first  == 4) AddYear(years, yearToInt(first));
  }
}

// convert containers to strings, and strings to containers ==================
void push_back_new(std::vector<std::string> &vec, const std::string str) {
// add str to vec, but only if str is not yet in vec and if str is not empty
  if (str.empty() ) return;
  if (find (vec.begin(), vec.end(), str) != vec.end() ) return;
  vec.push_back(str);
}

void stringToVector(std::vector<std::string> &vec, const char *str) {
// if str does not start with '|', don't split, just add str to vec
// otherwise, split str at '|', and add each part to vec
  if (!str || !*str) return;
  if (str[0] != '|') { vec.push_back(str); return; }
  const char *lDelimPos = str;
  for (const char *rDelimPos = strchr(lDelimPos + 1, '|'); rDelimPos != NULL; rDelimPos = strchr(lDelimPos + 1, '|') ) {
    push_back_new(vec, string(lDelimPos + 1, rDelimPos - lDelimPos - 1));
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
  return tChannelID::FromString(string(s, len).c_str());
}

template<class T> void insertObject(vector<T> &cont, const T &obj) { cont.push_back(obj); }
template<class T> void insertObject(set<T> &cont, const T &obj) { cont.insert(obj); }

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
      const char *end;
      for (end = start; *end; end++);
      initialize(start, end, tag);
    }
    std::string getString() const {
      if (!m_start || !m_end) return "";
      return std::string(m_start, m_end - m_start);
    }
    int getInt() const {
      if (!m_start) return 0;
      return atoi(m_start);
    }
    bool isValid() const { return m_start != NULL;}
    bool operator== (const cXmlString &sec) const {
      if (!m_start || !sec.m_start) return false;
      const char *s=sec.m_start;
      for (const char *f=m_start; s<m_end; f++, s++) if (*f != *s) return false;
      return true;
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

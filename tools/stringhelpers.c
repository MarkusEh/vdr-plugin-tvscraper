#include <string>
#include <vector>
#include <algorithm>
using namespace std;
std::string charPointerToString(const unsigned char *s) {
  return s?(const char *)s:"";
}
string charPointerToString(const char *s) {
  return s?s:"";
}

//replace all "search" with "replace" in "subject"
string str_replace(const string& search, const string& replace, const string& subject) {
    string str = subject;
    size_t pos = 0;
    while((pos = str.find(search, pos)) != string::npos) {
        str.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return str;
}
//cut string after first "search"
string str_cut(const string& search, const string& subject) {
    string str = subject;
    string strCutted = "";
    size_t found = str.find_first_of(search);
    if (found != string::npos) {
        strCutted = str.substr(0, found);
        size_t foundSpace = strCutted.find_last_of(" ");
        if ((foundSpace != string::npos) && (foundSpace == (strCutted.size()-1))) {
            strCutted = strCutted.substr(0, strCutted.size()-1);
        }
    }
    return strCutted;
}

void StringRemoveTrailingWhitespace(string &str) {
  std::string whitespaces (" \t\f\v\n\r");

  std::size_t found = str.find_last_not_of(whitespaces);
  if (found!=std::string::npos)
    str.erase(found+1);
  else
    str.clear();            // str is all whitespace
}

const char *strstr_word (const char *str1, const char *str2) {
// as strstr, but str2 must be a word (surrounded by non-alphanumerical characters
  if (!str1 || !str2 || !(*str2) ) return NULL;
  int len = strlen(str2);
  for (const char *f = strstr(str1, str2); f && *(f+1); f = strstr (f + 1, str2) ) {
    if (f != str1   && isalpha(*(f-1) )) continue;
    if (f[len] != 0 && isalpha(f[len]) ) continue;
    return f;
  }
  return NULL;
}

bool splitString(const std::string &str, char delimiter, size_t minLengh, std::string &first, std::string &second) {
  std::size_t found = str.find(delimiter);
  if(found == std::string::npos) return false; // nothing found
  while (found <= minLengh && found != std::string::npos) found = str.find(delimiter, found + 1);
  if(found == std::string::npos || found <= minLengh) return false; // nothing found
  std::size_t ssnd;
  for(ssnd = found + 1; ssnd < str.length() && str[ssnd] == ' '; ssnd++);
  if(str.length() - ssnd <= minLengh - 2) return false; // nothing found, second part to short

  second = str.substr(ssnd);
  first = str.substr(0, found);
  StringRemoveTrailingWhitespace(first);
  return true;
}

bool StringRemoveLastPartWithP(string &str) {
// remove part with (...)
  if (str.length() < 2 ) return false;
  StringRemoveTrailingWhitespace(str);
  if (str[str.length() -1] != ')') return false;
  std::size_t found = str.find_last_of("(");
  if (found == std::string::npos) return false;
  if (!isdigit(str[found +1])) return false; // we don't remove (asw), but (149) or (4/13)
  str.erase(found);
  StringRemoveLastPartWithP(str);
  return true;
}
int NumberInLastPartWithPS(const string &str) {
// return number in last part with (./.), 0 if not found / invalid
  if (str.length() < 3 ) return 0;
  if (str[str.length() - 1] != ')') return 0;
  std::size_t found = str.find_last_of("(");
  if (found == std::string::npos) return 0;
  for (std::size_t i = found + 1; i < str.length() - 1; i ++) {
    if (!isdigit(str[i]) && str[i] != '/') return 0; // we gnore (asw), and return only number with digits only
  }
  return atoi(str.c_str() + found + 1);
}
int NumberInLastPartWithP(const string &str) {
// return number in last part with (...), 0 if not found / invalid
  if (str.length() < 3 ) return 0;
  if (str[str.length() - 1] != ')') return 0;
  std::size_t found = str.find_last_of("(");
  if (found == std::string::npos) return 0;
  for (std::size_t i = found + 1; i < str.length() - 1; i ++) {
    if (!isdigit(str[i])) return 0; // we gnore (asw), and return only number with digits only
  }
  return atoi(str.c_str() + found + 1);
}
string SecondPart(const string &str, const std::string &delim) {
// Return part of str after first occurence of delim
// if delim is not in str, return ""
  size_t found = str.find(delim);
  if (found == std::string::npos) return "";
  std::size_t ssnd;
  for(ssnd = found + delim.length(); ssnd < str.length() && str[ssnd] == ' '; ssnd++);
  return str.substr(ssnd);
}

const char *firstDigit(const char *found) {
  for (; ; found++) if (isdigit(*found) || ! *found) return found;
}
const char *firstNonDigit(const char *found) {
  for (; ; found++) if (!isdigit(*found) || ! *found) return found;
}

void AddYears(vector<int> &years, const char *str) {
  if (!str) return;
  const char *last;
  for (const char *first = firstDigit(str); *first; first = firstDigit(last) ) {
    last = firstNonDigit(first);
    if (last - first  == 4) {
      int year = atoi(first);
      if (year > 1920 && year < 2100) {
        if (find(years.begin(), years.end(), year) == years.end()) years.push_back(year);
        int yearp1 = ( year + 1 ) * (-1);
        if (find(years.begin(), years.end(), yearp1) == years.end()) years.push_back(yearp1);
        int yearm1 = ( year - 1 ) * (-1);
        if (find(years.begin(), years.end(), yearm1) == years.end()) years.push_back(yearm1);
      }
    }
  }
}

void push_back_new(std::vector<std::string> &vec, const std::string str) {
// add str to vec, but only if str is not yet in vec and if str is not empty
  if (str.empty() ) return;
  if (find (vec.begin(), vec.end(), str) != vec.end() ) return;
  vec.push_back(str);
}

void stringToVector(std::vector<std::string> &vec, const char *str) {
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

int stringToYear(const string &str) {
  if (str.length() < 4) return 0;
  int i;
  for (i = 0; i < 4 && isdigit(str[i]); i++);
  if (i != 4) return 0;
  if (str.length() > 4 && isdigit(str[4]) ) return 0;
  return atoi(str.c_str() );
}

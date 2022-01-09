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
bool StringRemoveLastPartWithP(string &str) {
  StringRemoveTrailingWhitespace(str);
  if(str[str.length() -1] != ')') return false;
  std::size_t found = str.find_last_of("(");
  if(found == std::string::npos) return false;
  str.erase(found);
  StringRemoveLastPartWithP(str);
  return true;
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
      if (year > 1920 && year < 2100) if (find(years.begin(), years.end(), year) == years.end()) years.push_back(year);
    }
  }
}

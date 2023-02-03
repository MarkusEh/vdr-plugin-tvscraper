#include <iterator>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <math.h>
#include <set>
 
std::set<std::string_view> ignoreWords = {"der", "die", "das", "the", "I", "-"};
std::string normString(const char *s, int len = -1);
int normStringC(char *to, std::string_view from);
int sentence_distance_normed_strings(std::string_view str1, std::string_view str2);
class cNormedString {
  public:
    cNormedString(int malus, const std::string &normedString): m_malus(malus), m_normedString(normedString) {}
    int minDistanceNormedString(int distance, std::string_view normedString) const {
      return std::min(distance, sentence_distance_normed_strings(m_normedString, normedString) + m_malus); }
  private:
    int m_malus;
  public:
    std::string m_normedString;
};

int minDistanceNormedStrings(int distance, const std::vector<cNormedString> &normedStrings, const char *string, std::string *r_normedString = NULL) {
  int len = StringRemoveLastPartWithP(string, strlen(string));
  std::string_view sv;
  if (len == -1) sv = string;
  else sv = std::string_view(string, len);
  CONVERT(normedString, sv, normStringC);

  for (const cNormedString &i_normedString: normedStrings)
    distance = i_normedString.minDistanceNormedString(distance, normedString);

  if (r_normedString) *r_normedString = normedString;
  return distance;
}

// see https://stackoverflow.com/questions/15416798/how-can-i-adapt-the-levenshtein-distance-algorithm-to-limit-matches-to-a-single#15421038
template<typename T, typename C>
size_t seq_distance(const T& seq1, const T& seq2, const C& cost,
             const typename T::value_type& empty = typename T::value_type()) {
  const size_t size1 = seq1.size();
  const size_t size2 = seq2.size();
 
  std::vector<size_t> curr_col(size2 + 1);
  std::vector<size_t> prev_col(size2 + 1);
 
  // Prime the previous column for use in the following loop:
  prev_col[0] = 0;
  for (size_t idx2 = 0; idx2 < size2; ++idx2) {
    prev_col[idx2 + 1] = prev_col[idx2] + cost(empty, seq2[idx2]);
  }
 
  curr_col[0] = 0;
  for (size_t idx1 = 0; idx1 < size1; ++idx1) {
    curr_col[0] = curr_col[0] + cost(seq1[idx1], empty);
 
    for (size_t idx2 = 0; idx2 < size2; ++idx2) {
      curr_col[idx2 + 1] = std::min(std::min(
        curr_col[idx2] + cost(empty, seq2[idx2]),
        prev_col[idx2 + 1] + cost(seq1[idx1], empty)),
        prev_col[idx2] + cost(seq1[idx1], seq2[idx2]));
    }
 
    curr_col.swap(prev_col);
    curr_col[0] = prev_col[0];
  }
 
  return prev_col[size2];
}
 
size_t letter_distance(char letter1, char letter2) {
  return letter1 != letter2 ? 1 : 0;
}
 
size_t word_distance(std::string_view word1, std::string_view word2) {
  return seq_distance(word1, word2, &letter_distance);
}
 
std::vector<std::string_view> word_list(std::string_view sentence, int &len, int max_len = 0) {
// return list of words
// in len, return sum of chars in all words (this is usually shorter then sentence.length()
//std::cout << "sentence = \"" << sentence << "\" ";
  std::vector<std::string_view> result = getSetFromString<std::string_view, std::vector<std::string_view>>(sentence, ' ', ignoreWords, max_len);
  len = 0;
  for (const std::string_view &word:result) len += word.length();
  return result;
}

size_t sentence_distance_int(std::string_view sentence1, std::string_view sentence2, int &len1, int &len2, int max_len = 0) {
  return seq_distance(word_list(sentence1, len1, max_len), word_list(sentence2, len2, max_len), &word_distance);
}

int rom_char_val(char c) {
  switch(tolower(c)) {
    case 'm': return 1000;
    case 'd': return  500;
    case 'c': return  100;
    case 'l': return   50;
    case 'x': return   10;
    case 'v': return    5;
    case 'i': return    1;
  }
  return 0;
}

int romToArab(const char *&s, const char *se) {
// if *s is a roman number, terminated by a non alphanum character:
//     return the arab number
//     increase s to the last char after the roman number
// else: return 0, don't change s
  const char *sc = s; // sc points to current char
  int ret = 0, cur_val, next_val, prev_val = 1000;
  for (cur_val = rom_char_val(*sc); sc < se - 1 && cur_val; sc++, cur_val = next_val) {
    next_val = rom_char_val(sc[1]);
    if(cur_val < next_val) {
      cur_val = next_val - cur_val;
      sc++;
      next_val = sc + 1 < se?rom_char_val(sc[1]):0;
    }
    if (prev_val < cur_val) return 0;
    prev_val = cur_val;
    ret += cur_val;
  } 
  if (cur_val) {
    if (prev_val < cur_val) return 0;
    ret += cur_val;
    sc++;
  }
  if (sc != se && std::iswalnum(getUtfCodepoint(sc)) ) return 0;
  s = sc;
  return ret;
}

int normStringC(char *to, std::string_view from) {
// return number of characters written to to  (don't count 0 terminator)
// if to==NULL: don't write anything, just return the number

// replace invalid UTF8 characters with ' '
// replace all non-alphanumeric characters with ' '
// replace roman numbers with arabic numbers
  if (from.length() == 0) { if (to) *to = 0; return 0; }
  int numChars = 0;
  const char *s  = from.data();       // s points to current char
  const char *se = s + from.length(); // se points to char after last char (*se == 0 for c strings)
  bool wordStart = true;
  while (s < se) {
    if (wordStart) {
// At the beginning of a word, check for roman number
      int i = romToArab(s, se); // this also increases s, if a roman number was found.
      if (i > 0) {
        int il = concat::toCharsU(to, (unsigned int)i);
        if (to) to += il;
        numChars += il;
        continue;
      }
    }
    int l = utf8CodepointIsValid(s);
    wint_t cChar;
    if (l == 0) { cChar = ' '; s++; l = 1; }  // invalid utf
    else cChar = Utf8ToUtf32(s, l); // this also increases s
    if (!std::iswalnum(cChar) ) { cChar = ' '; l = 1; }  // change to " "
    if (cChar != ' ' || !wordStart) {
      if (to) AppendUtfCodepoint(to, towlower(cChar));  // also increases to
      numChars += l;
      wordStart = cChar == ' ';
    }
  }
  return numChars;
}
std::string normString(const char *s, int len) {
// replace invalid UTF8 characters with ' '
// replace all non-alphanumeric characters with ' '
// replace roman numbers with arabic numbers
// if len >= 0: this is the length of the string, otherwise: 0 terminated
// return the result
  std::string out;
  if (!s || !*s || len == 0) return out;
  if (len < 0) len = strlen(s);
  out.reserve(len);
//const char *sc = s;       // sc points to current char
  const char *se = s + len; // se points to char after last char (*se == 0 for c strings)
  while (s < se) {
    if (out.empty() || out.back() == ' ') {
// At the beginning of a word, check for roman number
      int i = romToArab(s, se); // this also increases s, if a roman number was found.
      if ( i > 0) {
        out.append(std::to_string(i));
        continue;
      }
    }
    wint_t cChar = getNextUtfCodepoint(s);  // this also increases s
    if (std::iswalnum(cChar) ) AppendUtfCodepoint(out, towlower(cChar));
    else {
      if (!out.empty() && out.back() != ' ') out.append(" ");
    }
  }
  return out;
}
inline std::string normString(const std::string &s) {
  return normString(s.c_str(), s.length() );
}
inline std::string normString(const std::string_view &s) {
  return normString(s.data(), s.length() );
}

int removeRomanNumC(char *to, std::string_view from) {
// replace invalid UTF8 characters with ' '
// replace roman numbers I - IX with ' '
// return number of characters written to to  (don't count 0 terminator)
// if to==NULL: don't write anything, just return the number

  int numChars = 0;
  const char *s  = from.data();
  const char *se = s + from.length(); // se points to char after last char (*se == 0 for c strings)
  bool wordStart = true;
  while (s < se) {
    if (wordStart) {
// Beginning of a word
      int i = romToArab(s, se); // this also increases s, if a roman number was found.
      if ( i > 0) continue;
    }
    int l = utf8CodepointIsValid(s);
    wint_t cChar;
    if (l == 0) { cChar = '?'; s++; l = 1; }  // invalid utf
    else cChar = Utf8ToUtf32(s, l); // this also increases s
    if (to) AppendUtfCodepoint(to, cChar); // also increases to
    numChars += l;
    wordStart = cChar == ' ';
  }
  return numChars;
}

std::vector<cNormedString> getNormedStrings(std::string_view searchString, std::string_view &searchString1, std::string_view &searchString2, std::string_view &searchString3, std::string_view &searchString4) {
// input: searchString: string to search for
// return list of normed strings
  std::vector<cNormedString> normedStrings;
  normedStrings.push_back(cNormedString(0, normString(searchString)));
  searchString1 = SecondPart(searchString, ": ", 4);
  searchString2 = SecondPart(searchString, "'s ", 4);
  if (!searchString1.empty() ) normedStrings.push_back(cNormedString(50, normString(searchString1)));
  if (!searchString2.empty() ) normedStrings.push_back(cNormedString(50, normString(searchString2)));
  bool split = splitString(searchString, " - ", 4, searchString3, searchString4);
  if (split) {
    normedStrings.push_back(cNormedString(70, normString(searchString3)));
    normedStrings.push_back(cNormedString(70, normString(searchString4)));
  } else {
    searchString3 = "";
    searchString4 = "";
  }
  return normedStrings;
}

// find longest common substring
// https://iq.opengenus.org/longest-common-substring/
int lcsubstr(std::string_view s1, std::string_view s2)
{
  int len1=s1.length(),len2=s2.length();
  int dp[2][len2+1];
  int curr=0, res=0;
//  int end=0;
  for(int i=0; i<=len1; i++) {
    for(int j=0; j<=len2; j++) {
      if(i==0 || j==0) dp[curr][j]=0;
      else if(s1[i-1] == s2[j-1]) {
        dp[curr][j] = dp[1-curr][j-1] + 1;
        if(res < dp[curr][j]) {
          res = dp[curr][j];
//          end = i-1;
        }
      } else {
          dp[curr][j] = 0;
      }
    }
    curr = 1-curr;
  }
//  std::cout << s1 << " " << s2 << " length= " << res << " substr= " << s1.substr(end-res+1,res) << std::endl;
  return res;
}

float normMatch(float x) {
// input:  number between 0 and infinity
// output: number between 0 and 1
// normMatch(1) = 0.5
// you can call normMatch(x/n), which will return 0.5 for x = n
  if (x <= 0) return 0;
  if (x <  1) return sqrt (x)/2;
  return 1 - 1/(x+1);
}
int normMatch(int i, int n) {
// return number between 0 and 1000
// for i == n, return 500
  if (n == 0) return 1000;
  return normMatch((float)i / (float)n) * 1000;
}

int dist_norm_fuzzy(std::string_view str_longer, std::string_view str_shorter, int max_length) {
// return 0 - 1000
// 0:    identical
// 1000: no match
  int l1, l2;
  int dist = sentence_distance_int(str_longer, str_shorter, l1, l2, max_length);
//std::cout << "l1 = " << l1 << " l2 = " << l2 << " dist = " << dist << std::endl;
  int max_dist = std::max(std::max(l1, l2), dist);
  if (max_dist == 0) return 1000; // no word found -> no match
  return 1000 * dist / max_dist;
}

int sentence_distance_normed_strings(std::string_view str1, std::string_view str2) {
// return 0-1000
// 0: Strings are equal
// before calling: strings must be prepared with normString
//  std::cout << "str1 = " << str1 << " str2 = " << str2 << std::endl;
  if (str1 == str2) {
    if (str1.empty() ) return 1000;
    return 0;
  }
  int s1l = str1.length();
  int s2l = str2.length();
  int minLen = std::min(s1l, s2l);
  int maxLen = std::max(s1l, s2l);
  if (minLen == 0) return 1000;

  int slengt = lcsubstr(str1, str2);
  int upper = 9; // match of more than upper characters: good
  if (slengt == minLen) slengt *= 2; // if the shorter string is part of the longer string
  if (slengt == maxLen) slengt *= 2; // the strings are identical, add some points for this
  int max_dist_lcsubstr = 1000;
  int dist_lcsubstr = 1000 - normMatch(slengt, upper);
  int dist_lcsubstr_norm = 1000 * dist_lcsubstr / max_dist_lcsubstr;

  int max1 = std::max(15, 2*minLen);
  int max2 = std::max( 9, minLen + minLen/2);
  int dist_norm = dist_norm_fuzzy(str1, str2, max1);
  if (maxLen > max2 && max1 != max2) dist_norm = std::min(dist_norm, dist_norm_fuzzy(str1, str2, max2));
//  std::cout << "dist_lcsubstr_norm = " << dist_lcsubstr_norm << " dist_norm = " << dist_norm << std::endl;
  return dist_norm / 2 + dist_lcsubstr_norm / 2;
}

int sentence_distance(const char *sentence1a, const char *sentence2a) {
  if (strcmp(sentence1a, sentence2a) == 0) return 0;
  CONVERT(ns1, std::string_view(sentence1a), normStringC);
  CONVERT(ns2, std::string_view(sentence2a), normStringC);
  return sentence_distance_normed_strings(ns1, ns2);
}

int sentence_distance(std::string_view sentence1a, std::string_view sentence2a) {
  if (sentence1a == sentence2a) return 0;
  CONVERT(ns1, sentence1a, normStringC);
  CONVERT(ns2, sentence2a, normStringC);
  return sentence_distance_normed_strings(ns1, ns2);
}

int minDistanceStrings(const char *str, const char *strToCompare, char delim = '~') {
// split str at ~, and compare all parts with strToCompare
// ignore last part after last ~ (in other words: ignore name of recording)
  int minDist = 1000;
  if (!str || !*str || !strToCompare || !*strToCompare) return minDist;
  const char *rDelimPos = strchr(str, delim);
  if (!rDelimPos) return minDist;  // there is no ~ in str
  CONVERT(stringToCompare, std::string_view(strToCompare), normStringC);
  for (; rDelimPos; rDelimPos = strchr(str, delim) ) {
    CONVERT(ns, std::string_view(str, rDelimPos - str), normStringC);
    minDist = std::min(minDist, sentence_distance_normed_strings(stringToCompare, ns));
    str = rDelimPos + 1;
  }
  return minDist;
}

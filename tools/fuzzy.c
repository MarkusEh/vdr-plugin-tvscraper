#include <iterator>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <math.h>
#include <set>
 
std::set<std::string> ignoreWords = {"der", "die", "das", "the", "I", "-"};
int sentence_distance_normed_strings(const std::string& str1, const std::string& str2);
class cNormedString {
  public:
    cNormedString(int malus, const std::string &normedString): m_malus(malus), m_normedString(normedString) {}
    int minDistanceNormedString(int distance, const std::string &normedString) const {
      return std::min(distance, sentence_distance_normed_strings(m_normedString, normedString) + m_malus); }
  private:
    int m_malus;
    std::string m_normedString;
};

int minDistanceNormedStrings(int distance, const std::vector<cNormedString> &normedStrings, const std::string &normedString) {
  for (const cNormedString &i_normedString: normedStrings)
    distance = i_normedString.minDistanceNormedString(distance, normedString);
  return distance;
}

// see https://stackoverflow.com/questions/15416798/how-can-i-adapt-the-levenshtein-distance-algorithm-to-limit-matches-to-a-single#15421038
template<typename T, typename C>
size_t
seq_distance(const T& seq1, const T& seq2, const C& cost,
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
 
size_t
letter_distance(char letter1, char letter2) {
  return letter1 != letter2 ? 1 : 0;
}
 
size_t
word_distance(const std::string& word1, const std::string& word2) {
  return seq_distance(word1, word2, &letter_distance);
}
 
std::vector<std::string> word_list(const std::string &sentence, int &len, int max_len = 0) {
// return list of words
// in len, return sum of chars in all words (this is usually shorter then sentence.length()
// if max_len is provided, stop adding words once len >= max_len (which means, len can be larger than max_len)
//std::cout << "sentence = \"" << sentence << "\" ";
  len = 0;
  std::vector<std::string> words;
  std::istringstream iss(sentence);
  for(std::istream_iterator<std::string> it(iss), end; ; ) {
  if (ignoreWords.find(*it) == ignoreWords.end()  )
    {
      words.push_back(*it);
      len += it->length();
//    std::cout << "word = \"" << *it << "\" ";
    }
    if (++it == end) break;
    if (max_len && len >= max_len) break;
  }
//std::cout << std::endl;
  return words;
}

size_t
sentence_distance_int(const std::string& sentence1, const std::string& sentence2, int &len1, int &len2, int max_len = 0) {
  return seq_distance(word_list(sentence1, len1, max_len), word_list(sentence2, len2, max_len), &word_distance);
}

// UTF8 string utilities ****************
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
// get next codepoint 0 is returned at end of string
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

std::string normString(const char *s, int len = -1) {
// replace invalid UTF8 characters with ' '
// replace all non-alphanumeric characters with ' '
// replace roman numbers I - IX with 1-9
// if len >= 0: this is the length of the string, otherwise: 0 terminated
// return the result
  std::string out;
  if(!s || !*s || len == 0) return out;
  if (len < 0 ) len = strlen(s);
  out.reserve(len);
//  const char *sc = s; // sc points to current char
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

std::string removeRomanNum(const char *s, int len = -1) {
// replace invalid UTF8 characters with ' '
// replace roman numbers I - IX with ' '
// len: num of character in s. If len == -1: s is 0 terminated
// return the result
  std::string out;
  if(!s || !*s || len == 0) return out;
  if (len < 0 ) len = strlen(s);
  out.reserve(len);
  const char *se = s + len; // se points to char after last char (*se == 0 for c strings)
  while (s < se) {
    if (out.empty() || out.back() == ' ') {
// Beginning of a word
      int i = romToArab(s, se); // this also increases s, if a roman number was found.
      if ( i > 0) continue;
    }
    wint_t cChar = getNextUtfCodepoint(s);  // this also increases s
    AppendUtfCodepoint(out, cChar);
  }
  return out;
}

// find longest common substring
// https://iq.opengenus.org/longest-common-substring/
int lcsubstr( const std::string &s1, const std::string &s2)
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

/*
void resizeStringWordEnd(std::string &str, int len) {
  if ((int)str.length() <= len) return;
  const size_t found = str.find_first_of(" .,;:!?", len);
  if (found == std::string::npos) return;
  str.resize(found);
}
*/

int dist_norm_fuzzy(const std::string &str_longer, const std::string &str_shorter, int max_length) {
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

int sentence_distance_normed_strings(const std::string& str1, const std::string& str2) {
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
  return sentence_distance_normed_strings(normString(sentence1a), normString(sentence2a));
}

int sentence_distance(const std::string &sentence1a, const std::string &sentence2a) {
  if (sentence1a == sentence2a) return 0;
  return sentence_distance_normed_strings(normString(sentence1a.c_str() ), normString(sentence2a.c_str() ));
}

int minDistanceStrings(const char *str, const char *strToCompare, char delim = '~') {
// split str at ~, and compare all parts with strToCompare
// ignore last part after last ~ (in other words: ignore name of recording)
  int minDist = 1000;
  if (!str || !*str || !strToCompare || !*strToCompare) return minDist;
  const char *rDelimPos = strchr(str, delim);
  if (!rDelimPos) return minDist;  // there is no ~ in str
  std::string stringToCompare = normString(strToCompare);
  for (; rDelimPos; rDelimPos = strchr(str, delim) ) {
    minDist = std::min(minDist, sentence_distance_normed_strings(stringToCompare, normString(str, rDelimPos - str)));
    str = rDelimPos + 1;
  }
  return minDist;
}


#include <iterator>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <math.h>
#include <set>
 
// see https://stackoverflow.com/questions/15416798/how-can-i-adapt-the-levenshtein-distance-algorithm-to-limit-matches-to-a-single#15421038
size_t word_distance(std::string_view seq1, std::string_view seq2) {
  const size_t size1 = seq1.size();
  const size_t size2 = seq2.size();
 
  size_t col_1[size2 + 1];
  size_t col_2[size2 + 1];
  size_t *curr_col = col_1;
  size_t *prev_col = col_2;
 
  // Prime the previous column for use in the following loop:
  for (size_t idx2 = 0; idx2 < size2 + 1; ++idx2) {
    prev_col[idx2] = idx2;
  }
 
  for (size_t idx1 = 0; idx1 < size1; ++idx1) {
    curr_col[0] = idx1 + 1;
 
    for (size_t idx2 = 0; idx2 < size2; ++idx2) {
      const size_t compare = seq1[idx1] == seq2[idx2] ? 0 : 1;
      curr_col[idx2 + 1] = std::min(std::min(curr_col[idx2] + 1,
                                             prev_col[idx2 + 1] + 1),
                                    prev_col[idx2] + compare);
    }
    std::swap(curr_col, prev_col);
  }
  return prev_col[size2];
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
    if (l == 0) { cChar = '?'; s++; }  // invalid utf
    else cChar = Utf8ToUtf32(s, l); // this also increases s
    numChars += AppendUtfCodepoint(to, cChar); // also increases to
    wordStart = cChar == ' ';
  }
  return numChars;
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

std::set<std::string_view> m_ignoreWords = {"der", "die", "das", "the", "I", "-"};
// =====================================================================================================
// class cNormedString  ================================================================================
// =====================================================================================================

class cNormedString {
  public:
    cNormedString(int malus = 0): m_malus(malus) {}
//    cNormedString(int malus, const std::string &normedString): m_malus(malus), m_normedString(normedString) {}
    cNormedString(const char *s, int malus = 0): m_malus(malus) { reset(charPointerToStringView(s)); }
    cNormedString(const std::string_view &s, int malus = 0): m_malus(malus) { reset(s); }
    cNormedString(const std::string      &s, int malus = 0): m_malus(malus) { reset(s); }
    cNormedString &reset(std::string_view str) {
// if there was any "old" noremed string: remove
// replace invalid UTF8 characters with ' '
// replace all non-alphanumeric characters with ' '
// replace roman numbers with arabic numbers
// write the result to m_normedString
      m_normedString.clear();
      m_normedString.reserve(str.length() );
      m_wordList.clear();
      m_wordList.reserve(20);
      size_t wordStart = 0;
      for (std::string_view word: cSplit(str, ' ')) {
        if (word.empty() ) continue;
// check for roman number
        const char *s  = word.data();
        int i = romToArab(s, s + word.length() ); // this also increases s, if a roman number was found.
        if ( i > 0) {
          stringAppend(m_normedString, i);
        } else {
          for (s = word.data(); s < word.data() + word.length(); ) {
            wint_t cChar = getNextUtfCodepoint(s);  // this also increases s
            if (std::iswalnum(cChar) ) stringAppendUtfCodepoint(m_normedString, towlower(cChar));
            else if (cChar == '&') m_normedString.append(1, cChar);
          }
        }
        if (wordStart < m_normedString.length() ) {
// something was appended
          std::string_view lWord(m_normedString.data() + wordStart, m_normedString.length() - wordStart);
          if (lWord == "und" || lWord == "and") {
            m_normedString.erase(wordStart);
            m_normedString.append(1, '&');
//          lWord = std::string_view(m_normedString.data() + wordStart, 1);
          }
          m_normedString.append(1, ' ');
          wordStart = m_normedString.length();
        }
      }
// remove spaces at end
      while (!m_normedString.empty() && m_normedString.back() == ' ') m_normedString.pop_back();
// create list of words
// must be one here, there must be no change to m_normedString after this
// any change to m_normedString can result in changed buffer location, invaldating the string_view word list
      for (std::string_view lWord: cSplit(m_normedString, ' ')) {
        if (m_ignoreWords.find(lWord) == m_ignoreWords.end() ) m_wordList.push_back(lWord);
      }
//      std::cout << str << " -> \"" << m_normedString << "\"\n";
//      std::cout << str << " -> \"" << m_normedString << "\" -> \"" << normString(str) << "\"\n";
//      std::cout << "words m_normedString:";
//      for (std::string_view sv:m_wordList) std::cout << " \"" << sv << "\"";
//      std::cout << "\n";
      return *this;
    }
  private:
    size_t numWords(const std::vector<std::string_view> &wordList, size_t &len, size_t maxChars) const {
// return number of words
// in len, return number of characters in the words
      len = 0;
      for (size_t size = 0; size < wordList.size(); ++size) {
        len += wordList[size].length();
        if (len + size + 1 >= maxChars) return size+1;
      }
      return wordList.size();
    }
    size_t sentence_distance_int(const cNormedString &other, size_t &len1, size_t &len2, size_t maxChars) const {
// how many words should we take into account?
      const size_t size1 = numWords(m_wordList, len1, maxChars);
      const size_t size2 = numWords(other.m_wordList, len2, maxChars);
      size_t col_1[size2 + 1];
      size_t col_2[size2 + 1];
      size_t *curr_col = col_1;
      size_t *prev_col = col_2;
// Prime the previous column for use in the following loop:
      prev_col[0] = 0;
      for (size_t idx2 = 0; idx2 < size2; ++idx2) {
        prev_col[idx2 + 1] = prev_col[idx2] + other.m_wordList[idx2].length();
      }
      curr_col[0] = 0;
      for (size_t idx1 = 0; idx1 < size1; ++idx1) {
        curr_col[0] = curr_col[0] + word_distance(m_wordList[idx1], std::string_view());
     
        for (size_t idx2 = 0; idx2 < size2; ++idx2) {
          curr_col[idx2 + 1] = std::min(std::min(
            curr_col[idx2] + other.m_wordList[idx2].length(),
            prev_col[idx2 + 1] + m_wordList[idx1].length() ),
            prev_col[idx2] + word_distance(m_wordList[idx1], other.m_wordList[idx2]));
        }
        std::swap(curr_col, prev_col);
        curr_col[0] = prev_col[0];
      }
//      std::cout << "sentence_distance_int new result " << prev_col[size2] << " size1 " << size1 << " size2 " << size2 << "\n";
      return prev_col[size2];
    }
    int dist_norm_fuzzy(const cNormedString &other, int max_length) const {
// return 0 - 1000
// 0:    identical
// 1000: no match
      size_t l1, l2;
      size_t dist = sentence_distance_int(other, l1, l2, max_length);
//std::cout << "l1 = " << l1 << " l2 = " << l2 << " dist = " << dist << std::endl;
      size_t max_dist = std::max(std::max(l1, l2), dist);
      if (max_dist == 0) return 1000; // no word found -> no match
      return 1000 * dist / max_dist;
    }


  public:
    int sentence_distance(const cNormedString &other, int curDistance = 1000) const {
// return 0-1000, or 0-curDistance
// if there is already a curDistance, return the smaller one (std::min(result of this, curDistance))
// 0: Strings are equal
//  std::cout << "str1 = " << str1 << " str2 = " << str2 << std::endl;
      if (m_normedString == other.m_normedString) {
        if (m_normedString.empty() ) return 1000;
        return 0;
      }
      int s1l = m_normedString.length();
      int s2l = other.m_normedString.length();
      int minLen = std::min(s1l, s2l);
      int maxLen = std::max(s1l, s2l);
      if (minLen == 0) return 1000;

      int slengt = lcsubstr(m_normedString, other.m_normedString);
      int upper = 9; // match of more than upper characters: good
      if (slengt == minLen) slengt *= 2; // if the shorter string is part of the longer string
      if (slengt == maxLen) slengt *= 2; // the strings are identical, add some points for this
      int max_dist_lcsubstr = 1000;
      int dist_lcsubstr = 1000 - normMatch(slengt, upper);
      int dist_lcsubstr_norm = 1000 * dist_lcsubstr / max_dist_lcsubstr;

      int max1 = std::max(15, 2*minLen);
      int max2 = std::max( 9, minLen + minLen/2);
      int dist_norm = dist_norm_fuzzy(other, max1);
      if (maxLen > max2 && max1 != max2) dist_norm = std::min(dist_norm, dist_norm_fuzzy(other, max2));
    //  std::cout << "dist_lcsubstr_norm = " << dist_lcsubstr_norm << " dist_norm = " << dist_norm << std::endl;
      return std::min(curDistance, dist_norm / 2 + dist_lcsubstr_norm / 2 + m_malus + other.m_malus);
    }
    int minDistanceNormedStrings(const std::vector<cNormedString> &normedStrings, int curDistance, std::string *r_normedString = NULL) const {
      for (const cNormedString &i_normedString: normedStrings)
        curDistance = sentence_distance(i_normedString, curDistance);

      if (r_normedString) *r_normedString = m_normedString;
      return curDistance;
    }
    int minDistanceStrings(const char *str, char delim = '~') {
    // split str at ~, and compare all parts with this
    // ignore last part after last ~ (in other words: ignore name of recording)
      int minDist = 1000;
      std::string_view prev;
      for (std::string_view part: cSplit(str, delim)) {
        if (!prev.empty()) minDist = sentence_distance(prev, minDist);
        prev = part;
      }
      return minDist;
    }
    int m_malus = 0;
    std::string m_normedString;
    std::vector<std::string_view> m_wordList;
};

std::vector<cNormedString> getNormedStrings(std::string_view searchString, std::string_view &searchString1, std::string_view &searchString2, std::string_view &searchString3, std::string_view &searchString4) {
// input: searchString: string to search for
// return list of normed strings
  std::vector<cNormedString> normedStrings;
  normedStrings.push_back(cNormedString(searchString));
  searchString1 = SecondPart(searchString, ": ", 4);
  searchString2 = SecondPart(searchString, "'s ", 4);
  if (!searchString1.empty() ) normedStrings.push_back(cNormedString(searchString1, 50));
  if (!searchString2.empty() ) normedStrings.push_back(cNormedString(searchString2, 50));
  bool split = splitString(searchString, " - ", 4, searchString3, searchString4);
  if (split) {
    normedStrings.push_back(cNormedString(searchString3, 70));
    normedStrings.push_back(cNormedString(searchString4, 70));
  } else {
    searchString3 = "";
    searchString4 = "";
  }
  return normedStrings;
}


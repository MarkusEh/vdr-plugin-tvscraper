#include <iterator>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <math.h>
#include <set>
#include <optional>
#include "tvscraperhelpers.h"
 
// see https://stackoverflow.com/questions/15416798/how-can-i-adapt-the-levenshtein-distance-algorithm-to-limit-matches-to-a-single#15421038
size_t word_distance(cSv seq1, cSv seq2) {
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

void removeTeil(char *s, cSv teil) {
// check if "teil" is in the string, and remove if reasonable
  cSv sv(s);
  size_t found = sv.rfind(teil);
  if (found == std::string_view::npos) return; // not found
  if (found < 5) return; // wee need some minimum entropy for search
  if (sv[found-1] != ' ') return;     // begin word
  size_t p = found + teil.length();
  if (p < sv.length() && s[p] != ' ') return;  // end word
  for (int na = 0; p < sv.length(); ++p) if (isalpha(s[p])) {
    if (++na > 1) return;  // more than 1 alphabetic character after teil
  }
// remove teil
  s[found-1] = 0;
}

int removeRomanNumC(char *to, cSv from) {
// intended to be called before passing "from" to the external search API, to find more results

// replace invalid UTF8 characters with ' '
// replace roman numbers I - IX with ' '
// return number of characters written to to  (don't count 0 terminator)
// if to==NULL: don't write anything, just return the (maximum) number
// otherwise, also remove "teil" / "part" from the end of the string

  char *to_0 = to;
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
    numChars += AppendUtfCodepoint(to, towlower(cChar) ); // also increases to
    wordStart = cChar == ' ';
  }
  if (to) {
    *to = 0;
    removeTeil(to_0, "teil");
    removeTeil(to_0, "part");
  }
  return numChars;
}

// find longest common substring
// https://iq.opengenus.org/longest-common-substring/
int lcsubstr(cSv s1, cSv s2)
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

std::set<cSv> const_ignoreWords = {"der", "die", "das", "the", "I", "-", "in", "to"};
// =====================================================================================================
// class cNormedString  ================================================================================
// =====================================================================================================

class cNormedString {
  public:
    cNormedString(int malus = 0): m_malus(malus) {}
    cNormedString(cSv s, int malus = 0): m_malus(malus) { reset(s); }
    cNormedString(const cNormedString&) = delete;
    cNormedString &operator= (const cNormedString &other) {
      m_malus = other.m_malus;
      m_normedString = other.m_normedString;
      m_wordList.clear();
      return *this;
    }
    cNormedString(cNormedString &&other) {
      m_malus = other.m_malus;
      m_normedString = std::move(other.m_normedString);
      m_wordList = std::move(other.m_wordList);
      m_wordList.clear();
    }
    cNormedString &operator= (cNormedString &&other) {
      m_malus = other.m_malus;
      m_normedString = std::move(other.m_normedString);
      m_wordList = std::move(other.m_wordList);
      m_wordList.clear();
      return *this;
    }

    bool empty() const { return m_normedString.empty(); }
    cNormedString &reset(cSv str) {
// if there was any "old" normed string: remove
// replace invalid UTF8 characters with ' '
// replace all non-alphanumeric characters with ' '
// replace roman numbers with arabic numbers
// write the result to m_normedString
      m_normedString.clear();
      m_normedString.reserve(str.length() );
      m_wordList.clear();
// 1st step: sanitize UTF8, to_lower, replace non-alphanumeric with " "
      for (const char *s = str.data(); s < str.data() + str.length(); ) {
        wint_t cChar = getNextUtfCodepoint(s);  // this also increases s
        if (std::iswalnum(cChar) ) stringAppendUtfCodepoint(m_normedString, towlower(cChar));
        else switch (cChar) {
          case '&':
            m_normedString.append(1, cChar);
            break;
          default:
            if (!m_normedString.empty() && m_normedString.back() != ' ') m_normedString.append(1, ' '); // -> all "unknown" non-alphanum characters -> " "
            break;
        }
      }
// remove spaces at end
      while (!m_normedString.empty() && m_normedString.back() == ' ') m_normedString.pop_back();
// next step: change words. Cannot use cSplit, as we change the string
      bool changed;
      do {
        changed = false;
        size_t we = 0;
        for (size_t currentWordStart = 0; !changed && we != m_normedString.length(); currentWordStart = we + 1) {
          we = m_normedString.find(" ", currentWordStart);
          if (we == std::string::npos) we = m_normedString.length();
          cSv lWord = cSv(m_normedString).substr(currentWordStart, we - currentWordStart);
          if (lWord == "und" || lWord == "and") {
            m_normedString.erase(currentWordStart, lWord.length() );
            m_normedString.insert(currentWordStart, 1, '&');
            changed = true;
          } else {
// check for roman number
            const char *s  = lWord.data();
            int i = romToArab(s, s + lWord.length() ); // this also increases s, if a roman number was found.
            if ( i > 0 && s == lWord.data() + lWord.length() ) {
              m_normedString.erase(currentWordStart, lWord.length() );
              char buf[21]; // unsigned int 64: max. 20. (18446744073709551615) signed int64: max. 19 (+ sign)
              char *bufferEnd = buf+20;
              for (*bufferEnd = 0; i; i /= 10) *(--bufferEnd) = '0' + (i%10);
              m_normedString.insert(currentWordStart, bufferEnd);
              changed = true;
            }
          }
        } // for size_t currentWordStart = 0; ...
      } while (changed);
//      std::cout << str << " -> \"" << m_normedString << "\"\n";
      return *this;
    }
  private:
    void createWordList() const {
// create list of words
// must be one here, there must be no change to m_normedString after this
// also std::move invalidates this list, as the string address might change
// any change to m_normedString can result in changed buffer location, invaldating the string_view word list
      if (m_wordList.size() > 0) return;
      m_wordList.reserve(20);
      for (cSv lWord: cSplit(m_normedString, ' ')) {
// we have to include very short words (1 char) here, to take digits like in part 1 / part 2 into account
        if (const_ignoreWords.find(lWord) == const_ignoreWords.end() ) m_wordList.push_back(lWord);
      }
//      std::cout << "words m_normedString: " << getWords() << "\n";
    }
    size_t numWords(size_t &len, size_t maxChars) const {
// return number of words
// in len, return number of characters in the words
      createWordList ();
      len = 0;
      for (size_t size = 0; size < m_wordList.size(); ++size) {
        len += m_wordList[size].length();
        if (len + size + 1 >= maxChars) return size+1;
      }
      return m_wordList.size();
    }
    size_t sentence_distance_int(const cNormedString &other, size_t &len1, size_t &len2, size_t maxChars) const {
// how many words should we take into account?
      const size_t size1 =       numWords(len1, maxChars);
      const size_t size2 = other.numWords(len2, maxChars);
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
        curr_col[0] = curr_col[0] + word_distance(m_wordList[idx1], cSv());
     
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
    int sentence_distance(cSv other, int curDistance = 1000) const {
      return sentence_distance(cNormedString(other), curDistance);
    }
    int sentence_distance(const cNormedString &other, int curDistance = 1000) const {
// return 0-1000, or 0-curDistance
// if there is already a curDistance, return the smaller one (std::min(result of this, curDistance))
// 0: Strings are equal
//  std::cout << "str1 = " << str1 << " str2 = " << str2 << std::endl;
      if (m_normedString == other.m_normedString) {
        if (m_normedString.empty() ) return std::min(curDistance, 1000);
        return std::min(curDistance, m_malus + other.m_malus);
      }
      int s1l = m_normedString.length();
      int s2l = other.m_normedString.length();
      int minLen = std::min(s1l, s2l);
      int maxLen = std::max(s1l, s2l);
      if (minLen == 0) return std::min(curDistance, 1000);

      int slengt = lcsubstr(m_normedString, other.m_normedString);
      int upper = 9; // match of more than upper characters: good
      if (slengt == minLen) slengt *= 2; // if the shorter string is part of the longer string
      if (slengt == maxLen) slengt *= 2; // the strings are identical, add some points for this
      int max_dist_lcsubstr = 1000;
      int dist_lcsubstr = 1000 - normMatch(slengt, upper);
      int dist_lcsubstr_norm = 1000 * dist_lcsubstr / max_dist_lcsubstr;

      int max1 = std::max(15, 2*minLen);
      int max2 = std::max( 9, 2*minLen);
      int dist_norm = dist_norm_fuzzy(other, max1);
//      std::cout << "dist_lcsubstr_norm = " << dist_lcsubstr_norm << " dist_norm = " << dist_norm << std::endl;
      if (maxLen > max2 && max1 != max2) dist_norm = std::min(dist_norm, dist_norm_fuzzy(other, max2));
//      std::cout << "dist_lcsubstr_norm = " << dist_lcsubstr_norm << " dist_norm = " << dist_norm << std::endl;
      return std::min(curDistance, dist_norm / 2 + dist_lcsubstr_norm / 2 + m_malus + other.m_malus);
    }
    int minDistanceNormedStrings(const std::vector<std::optional<cNormedString>> &normedStrings, int curDistance) const {
      for (const std::optional<cNormedString> &i_normedString: normedStrings)
        if (i_normedString.has_value()) curDistance = sentence_distance(i_normedString.value(), curDistance);

      return curDistance;
    }
    int minDistanceStrings(const char *str, char delim = '~') const {
    // split str at ~, and compare all parts with this
    // ignore last part after last ~ (in other words: ignore name of recording)
      int minDist = 1000;
      cSv prev;
      for (cSv part: cSplit(str, delim)) {
        if (!prev.empty()) minDist = sentence_distance(prev, minDist);
        prev = part;
      }
      return minDist;
    }
    std::string getWords() const {
      if (m_wordList.size() == 0) return "no words";
      std::string result;
      for (const auto &w: m_wordList)
        stringAppend(result, "\"", w, "\" ");
      return result;
    }
    int m_malus = 0;
    std::string m_normedString;
    mutable std::vector<cSv> m_wordList;
};

class cNormedStringsDelim;
class cCompareStrings {
  public:
    cCompareStrings(cSv searchString);
    void add(cSv searchString, char delim);
    class iterator {
      private:
        std::vector<cNormedStringsDelim>::const_iterator m_it;
      public:
        iterator(std::vector<cNormedStringsDelim>::const_iterator it): m_it(it) {}
        iterator& operator++();
        bool operator!=(iterator other) const;
        char operator*() const; // return delim
    };
    iterator begin() const { return iterator(m_normedStringsDelim.begin()); }
    iterator end()   const { return iterator(m_normedStringsDelim.end()); }

    int minDistance(char delim, const cNormedString &compareString, int curDistance = 1000) const;
  private:
    std::vector<cNormedStringsDelim> m_normedStringsDelim;
};
class cNormedStringsDelim {
  public:
    cNormedStringsDelim(cSv searchString, char delim = 0);
    int minDistance(const cNormedString &compareString, int curDistance = 1000) const;
  private:
    std::vector<std::optional<cNormedString>> m_normedStrings;
  public:
    const char m_delim;
};

// implementation cNormedStringsDelim
cNormedStringsDelim::cNormedStringsDelim(cSv searchString, char delim):
  m_normedStrings(5), m_delim(delim)
{
// input: searchString: string to search for
  m_normedStrings[0].emplace(searchString);
  cSv searchString1 = SecondPart(searchString, ": ", 4);
  if (!searchString1.empty() ) m_normedStrings[1].emplace(searchString1, 50);
  cSv searchString2 = SecondPart(searchString, "'s ", 4);
  if (!searchString2.empty() ) m_normedStrings[2].emplace(searchString2, 50);
  bool split = splitString(searchString, " - ", 4, searchString1, searchString2);
  if (split) {
    if (searchString1.length() > 5) m_normedStrings[3].emplace(searchString1, 70);
    if (searchString2.length() > 5) m_normedStrings[4].emplace(searchString2, 70);
  }
}
int cNormedStringsDelim::minDistance(const cNormedString &compareString, int curDistance) const {
  for (const std::optional<cNormedString> &i_normedString: m_normedStrings)
    if (i_normedString.has_value()) curDistance = compareString.sentence_distance(i_normedString.value(), curDistance);
  return curDistance;
}
// implementation cCompareStrings
cCompareStrings::iterator &cCompareStrings::iterator::operator++() { ++m_it; return *this; }
bool cCompareStrings::iterator::operator!=(iterator other) const { return m_it != other.m_it; }
char cCompareStrings::iterator::operator*() const { return m_it->m_delim; }

cCompareStrings::cCompareStrings(cSv searchString)
{
  m_normedStringsDelim.emplace_back(searchString, 0);
} 
void cCompareStrings::add(cSv searchString, char delim) {
  cSv TVshowName, episodeSearchString;
  if (!splitString(searchString, delim, 4, TVshowName, episodeSearchString) ) return;
  m_normedStringsDelim.emplace_back(TVshowName, delim);
}
int cCompareStrings::minDistance(char delim, const cNormedString &compareString, int curDistance) const {
  for (const cNormedStringsDelim &normedStringsDelim: m_normedStringsDelim) 
    if (normedStringsDelim.m_delim == delim) curDistance = normedStringsDelim.minDistance(compareString, curDistance);
  return curDistance;
}

#include <iterator>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <math.h>
#include <set>
 
std::set<std::string> ignoreWords = {"der", "die", "das", "the", "I"};
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
 
std::vector<std::string> word_list(const std::string &sentence, int &len) {
// return list of words
  len = 0;
  std::vector<std::string> words;
  std::istringstream iss(sentence);
  for(std::istream_iterator<std::string> it(iss), end; ; ) {
  if (ignoreWords.find(*it) == ignoreWords.end()  )
    {
      words.push_back(*it);
      len += it->length();
//      std::cout << "word = \"" << *it << "\" ";
    }
    if(++it == end) break;
  }
//  std::cout << std::endl;
/*
  std::copy(std::istream_iterator<std::string>(iss),
            std::istream_iterator<std::string>(),
            std::back_inserter(words));
*/
  return words;
}

size_t
sentence_distance_int(const std::string& sentence1, const std::string& sentence2, int &len1, int &len2) {
  return seq_distance(word_list(sentence1, len1), word_list(sentence2, len2), &word_distance);
}
std::string stripExtra(const std::string &in) {
  std::string out;
  out.reserve(in.length() );
  for (const char &c: in) {
    if ( (c > 0 && c < 46) || c == 46 || c == 47 || ( c > 57 && c < 65 ) || ( c > 90 && c < 97 ) || ( c > 122  && c < 128) ) {
      out.append(1, ' ');
    } else out.append(1, c);
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

int sentence_distance(const std::string& sentence1a, const std::string& sentence2a) {
// return 0-1000
// 0: Strings are equal
  std::string sentence1 = stripExtra(sentence1a);
  std::string sentence2 = stripExtra(sentence2a);
//  std::cout << "sentence1 = " << sentence1 << " sentence2 = " << sentence2 << std::endl;
  size_t s1l = sentence1.length();
  size_t s2l = sentence2.length();
  if (s1l == 0 || s2l == 0) return 1000;
  int min_s1s2 = (int)std::min(s1l, s2l);
  int max_s1s2 = (int)std::max(s1l, s2l);

  int slengt = lcsubstr(sentence1, sentence2);
  int upper = 10; // match of more than upper characters: good
  if (slengt == min_s1s2) upper = std::min(std::max(min_s1s2/3, 2), 9);  // if the shorter string is part of the longer string: reduce upper
  if (slengt == max_s1s2) slengt *= 2; // the strings are identical, add some points for this
  int max_dist_lcsubstr = 1000;
  int dist_lcsubstr = 1000 - normMatch(slengt, upper);
  int dist_lcsubstr_norm = 1000 * dist_lcsubstr / max_dist_lcsubstr;

  int max_20_2times_min_s1s2 = std::max(20, 2*min_s1s2);
  if (max_s1s2 > max_20_2times_min_s1s2) {
    if (s1l > s2l) sentence1.resize(max_20_2times_min_s1s2);
    else           sentence2.resize(max_20_2times_min_s1s2);
  }
  int l1, l2; // nambe of characters != ' ', i.e. characters actually compared
  int dist = sentence_distance_int(sentence1, sentence2, l1, l2);
//  std::cout << "l1 = " << l1 << " l2 = " << l2 << " dist = " << dist << std::endl;
  int max_dist = std::max(std::max(l1, l2), dist);
  int dist_norm = 1000 * dist / max_dist;
//  std::cout << "dist_lcsubstr_norm = " << dist_lcsubstr_norm << " dist_norm = " << dist_norm << std::endl;
  return dist_norm / 2 + dist_lcsubstr_norm / 2;
}

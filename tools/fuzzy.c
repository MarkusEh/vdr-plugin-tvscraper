#include <iterator>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
 
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
 
size_t
sentence_distance_int(const std::string& sentence1, const std::string& sentence2) {
  std::vector<std::string> words1;
  std::vector<std::string> words2;
  std::istringstream iss1(sentence1);
  std::istringstream iss2(sentence2);
  for(std::istream_iterator<std::string> it(iss1), end; ; ) {
    words1.push_back(*it);
    if(++it == end)
      break;
    words1.push_back(" ");
  }
  for(std::istream_iterator<std::string> it(iss2), end; ; ) {
    words2.push_back(*it);
    if(++it == end)
      break;
    words2.push_back(" ");
  }
  return seq_distance(words1, words2, &word_distance);
}
std::string stripExtra(const std::string &in) {
  std::string out;
  out.reserve(in.length() );
  for (const char &c: in) {
    if ( (c > 0 && c < 48) || ( c > 57 && c < 65 ) || ( c > 90 && c < 97 ) || ( c > 122  && c < 128) ) {
      out.append(1, ' ');
    } else out.append(1, c);
  }
  return out;
}
int sentence_distance(const std::string& sentence1, const std::string& sentence2) {
// return 0-1000
// 0: Strings are equal
  size_t s1l = sentence1.length();
  size_t s2l = sentence2.length();
  if (s1l == 0 || s2l == 0) return 1000;
  size_t max_dist = std::max(s1l, s2l);
  if (s1l > s2l) {
    if (sentence1.find(sentence2) != std::string::npos) return 400 * (s1l - s2l) / max_dist;
  } else {
    if (sentence2.find(sentence1) != std::string::npos) return 400 * (s2l - s1l) / max_dist;
  }

  size_t dist = sentence_distance_int(stripExtra(sentence1), stripExtra(sentence2) );
  return 1000 * dist / max_dist;
}

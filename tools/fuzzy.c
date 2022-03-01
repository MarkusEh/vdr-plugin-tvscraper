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
    if ( (c > 0 && c < 46) || c == 46 || c == 47 || ( c > 57 && c < 65 ) || ( c > 90 && c < 97 ) || ( c > 122  && c < 128) ) {
      out.append(1, ' ');
    } else out.append(1, c);
  }
  return out;
}
size_t sentenceFind(const std::string& sentence1, const std::string& sentence2) {
  size_t len2 = sentence2.length();
  if (len2 == 0 || sentence1.find(sentence2) !=std::string::npos) return len2;
// not found
  std::size_t il, im, ih, n_found = 0;

  il = 0;
  ih = len2;
  im = ih;
  while(ih - il > 1) {
    im = (ih + il)/2; // (ih - il)/2 + il;
    if (im == 0) return 0;
    n_found = sentence1.find(sentence2.substr(0, im) );
    if (n_found == std::string::npos)
           ih = im;  // not found, continue in lower half
      else il = im;  // found, continue in upper half
  }
  if (n_found == std::string::npos)
         return im-1;
    else return im;
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
  size_t max_dist = std::max(s1l, s2l);
  int match_find;
  if (s1l > s2l) {
    match_find = sentenceFind(sentence1, sentence2);
  } else {
    match_find = sentenceFind(sentence2, sentence1);
  }
  if (match_find < 5) match_find *= 100;
  else if (match_find <= 15 ) match_find = 500 + (match_find-5) * 40;
  else match_find = 1000;  // number between 0 & 1000, higher is better

  size_t dist = sentence_distance_int(sentence1, sentence2);
  if (dist > max_dist) max_dist = dist;
//  std::cout << "match_find = " << match_find << " dist = " << dist << " max_dist = " << max_dist << std::endl;
  return (600 * dist / max_dist) + (1000 - match_find) * 400 / 1000;
}

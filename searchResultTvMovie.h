#include <string>
#include <vector>

struct sMatchWeight {
  float match;
  float weight;
};
class searchResultTvMovie {

public:
  searchResultTvMovie(int id, bool movie, const char *year);
  ~searchResultTvMovie();
  searchResultTvMovie(const searchResultTvMovie&) = delete;
  searchResultTvMovie &operator= (const searchResultTvMovie &) = delete;
  searchResultTvMovie(searchResultTvMovie &&) = default;
  searchResultTvMovie &operator= (searchResultTvMovie &&) = default;
  void log(std::string_view title) const;
  static float normMatch(float x);
  float getMatch() const;
  bool operator< (const searchResultTvMovie &srm) const;
  void setMatchText(int distance) { m_matches[0].match = (1000 - distance) / 1000.; }
  void setMatchTextMin(int distance) {
    m_matches[0].match = std::max(m_matches[0].match, (float)((1000 - distance) / 1000.)); }
  void setMatchTextMin(int distance, const cNormedString &normedName) {
    float match = (1000 - distance) / 1000.;
    if (match > m_matches[0].match) {
       m_matches[0].match = match;
       m_normedName = normedName;
    }
  }
  void updateMatchText(int distance);
  void setPopularity(float popularity, float vote_average, int vote_count);
  void setPopularity(float vote_average, int vote_count);
  void setScore(int score);
  void setDuration(int durationDeviation) { m_matches[3].match = 1 - normMatch(durationDeviation / 15.); }
  void setActors(int numMatches)  { m_matches[4].match = normMatch(numMatches/16.); }
  void setDirectorWriter(int numMatches)  { m_matches[5].match = normMatch(numMatches/2.); }
  void setMatchYear(const cYears &years, int durationInSec);
  void setMatchEpisode(int distance) { m_matches[6].match = (1000 - distance) / 1000.; }
  bool getMatchEpisode() const { return m_matches[6].match > 0.; }
  float getMatchText() const { return m_matches[0].match; }
  void setBaseNameEquShortText() { m_matches[7].match = 1.0; }
  void setPositionInExternalResult(int positionInExternalResult) { m_matches[8].match = 1.0 - normMatch(positionInExternalResult); }

  void setDelim(char delim) { m_delim = delim; }
  int delim(void) const { return m_delim; }
  int id() const { return m_id; }
  bool movie() const { return m_movie; }
  int year() const { return m_year; }
  int m_yearMatch = 0;
  cNormedString m_normedName;
private:
  int m_id;
  bool m_movie;
  int m_year;
  sMatchWeight m_matches[9];
  char m_delim = 0;
};

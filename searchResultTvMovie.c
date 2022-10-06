#include <vector>
#include <algorithm>
#include <math.h>       /* sqrt */

searchResultTvMovie::searchResultTvMovie(int id, bool movie, const std::string &year):
     m_id(id), m_movie(movie) {
// year
  if (year.length() >= 4) m_year = atoi(year.substr(0,4).c_str() );
              else m_year = 0;
// defaults for m_matches
  for (size_t i=0; i < sizeof(m_matches)/sizeof(m_matches[0]); i++) m_matches[i].match = -1.;
  m_matches[0].weight = 0.6; // match text
  m_matches[1].weight = 0.2; // match year
  m_matches[2].weight = 0.2; // match popularity, vote and vote_count
  m_matches[3].weight = 0.2; // match duration
  m_matches[4].weight = 0.3; // match actors
  m_matches[5].weight = 0.1; // match director writer
  m_matches[6].weight = 0.3; // match episode (for tv shows with episode only)
  m_matches[7].weight = 0.3; // baseNameEquShortText -> extra points for series
  m_matches[8].weight = 0.0001; // positionInExternalResult
}
searchResultTvMovie::~searchResultTvMovie() {
//  if (m_id == 689390) log("Merlin movie 1998-04-26");
//  if (m_id == -83269) log("Merlin series Sam Neill");
//  if (m_id == -72449) log("Stargate SG-1");
//  if (m_id == 138103) log("The Expendables 3");
//  if (m_id == 27578) log("The Expendables");
//  if (m_id == -364093) log("Star Trek: Picard");
//  if (m_id ==  -76107) log("Doctor Who (1963) classic");
//  if (m_id ==  -78804) log("Doctor Who (2005) was wir jetzt sehen");
//  if (m_id == -112671) log("Doctor Who (2009), series of short fan films");
//  if (m_id == -329847) log("Doctor Who (DDK Productions), youTube");
//  if (m_id == -383868) log("Doctor Who: Origin kein year, ... wohl irrelevant");
//  if (m_id == -72037) log("Mastermind BBC");
//  if (m_id == 5548) log("RoboCop");
}

void searchResultTvMovie::log(const char *title) const {
  esyslog("tvscraper: searchResultTvMovie::log, id: %i, title: \"%s\"", m_id, title);
  for (size_t i=0; i < sizeof(m_matches)/sizeof(m_matches[0]); i++) if (m_matches[i].match >= 0) {
    const char *d;
    switch (i) {
      case 0: d = "Text"; break;
      case 1: d = "Year"; break;
      case 2: d = "Vote, .."; break;
      case 3: d = "Duration"; break;
      case 4: d = "Actors"; break;
      case 5: d = "Director Writer"; break;
      case 6: d = "Episode"; break;
      case 7: d = "BaseNameEquShortText"; break;
      case 8: d = "PositionInExternalResult"; break;
      default: d = "ERROR!!!!";
    }
    esyslog("tvscraper: searchResultTvMovie::log, i: %lu, match: %f, weight %f, desc: %s", i, m_matches[i].match, m_matches[i].weight, d);
  }
  esyslog("tvscraper: searchResultTvMovie::log, getMatch(): %f, delim: %c", getMatch(), m_delim?m_delim:' ' );
}

bool searchResultTvMovie::operator< (const searchResultTvMovie &srm) const {
// true if this has a better match than srm
  if (srm.id() == 0) return true;
  return getMatch() > srm.getMatch();
}

void searchResultTvMovie::updateMatchText(int distance) {
// input: distance between 0 and 1000, lower values are better
// update, if new match is better than old value
  float newMatch = (1000 - distance) / 1000.;
  if (newMatch > m_matches[0].match) m_matches[0].match = newMatch;
}
void searchResultTvMovie::setPopularity(float vote_average, int vote_count) {
// note: this is for thetvdb, vote_average -> Rating -> <values between 0 and 10>
  float voteCountFactor = normMatch(vote_count / 100.);
  m_matches[2].match = 0.5*voteCountFactor + 0.5*vote_average/10.;
}

void searchResultTvMovie::setPopularity(float popularity, float vote_average, int vote_count) {
// this is for themoviedb; vote_average -> <values between 0 and 10>
  float popularityFactor = normMatch(popularity / 5.);
  float voteCountFactor = normMatch(vote_count / 100.);
  m_matches[2].match = 0.4*voteCountFactor + 0.3*vote_average/10. + 0.3*popularityFactor;
}

void searchResultTvMovie::setScore(int score) {
// this is for thetvdb. Int score (range unclear)
/* Examples:
Tatort:  2044
Enterprise: 50616
"name": "Merlin (2008)", "objectID": "series-83123", "score": 429780,
"tvdb_id": "83269","Merlin (1998)" "score": 2922,
"objectID": "series-263785", "name": "Merlin (2012)", "score": 1130,
"objectID": "series-166831","name": "Merlin: Secrets & Magic", "score": 302,
"objectID": "series-77272", "name": "Mr. Merlin", "Max Merlin is an auto mechanic who keeps his magic powers a secret from everybody except his teenage apprentice." "score": 212,
"objectID": "series-362037","name": "Merlin of the Crystal Cave", "score": 151,
"objectID": "series-252374","name": "The Boy Merlin", "score": 131,
"objectID": "series-82915","name": "Merlina", "score": 94,

"objectID": "series-93221","name": "Um Himmels Willen" "score": 446,
*/
  m_matches[2].match = normMatch(score / 400.);
}

void setScore(int score);


void searchResultTvMovie::setMatchYear(const std::vector<int> &years, int durationInSec) {
// input: list of years in texts
  if (m_year <= 0) { m_matches[1].match = 0.; return; }
  if (!m_movie &&  durationInSec < 80*60) m_matches[1].weight = 0.1; // for a series, this matching is more irrelevant. Except a min series with long episodes
  if (std::find (years.begin(), years.end(), m_year    ) != years.end() ) { m_yearMatch =  1; m_matches[1].match = 1.; return; }
  if (std::find (years.begin(), years.end(), m_year *-1) != years.end() ) { m_yearMatch = -1; m_matches[1].match = .8; return; }
  m_matches[1].match = .3; // some points for the existing year ...
}

float searchResultTvMovie::getMatch() const {
// return value between 0 and 1
// higher values are better match
  float sumWeight = 0.;
  float sumMatch  = 0.;
  for (size_t i=0; i < sizeof(m_matches)/sizeof(m_matches[0]); i++) if (m_matches[i].match >= 0) {
    sumWeight += m_matches[i].weight;
    sumMatch  += m_matches[i].match  * m_matches[i].weight;
  }
  return sumMatch/sumWeight;
}

float searchResultTvMovie::normMatch(float x) {
// input:  number between 0 and infinity
// output: number between 0 and 1
// normMatch(1) = 0.5
// you can call normMatch(x/n), which will return 0.5 for x = n 
  if (x <= 0) return 0;
  if (x <  1) return sqrt (x)/2;
  return 1 - 1/(x+1);
}


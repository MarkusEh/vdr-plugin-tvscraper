#include "example2.h"
#include <tools/curlfuncs.h>
#include <tools/filesystem.c>
#include <tools/fuzzy.c>

//***************************************************************************
// Field
//***************************************************************************

const uint64_t epoch = 4675708794760826425LL;
const double factor = 95443717 / 60.0;            // Number of TVM-ticks in per second.
const unsigned int unixTimeEpoch = 1233270600;

class Field
{
  public:

    enum ContentType
    {
       ctInt      = 3,
       ctDateTime = 7,
       ctString   = 8,
       ctTwoByte  = 11
    };

    Field(FILE* file)        { read(file); }

    const std::string& getString() const  { return stringContent; }
    uint64_t getNumeric()     const  { return numeric; }
    bool isNumeric()          const  { return type == ctInt || type == ctTwoByte || type == ctDateTime; }
    bool isString()           const  { return type == ctString; }

  private:

    void read(FILE* file)
    {
      type = (ContentType)readInt(file, 2);

      switch (type)
      {
        case ctString:  readString(file);            break;
        case ctInt:     numeric = readInt(file, 4);  break;

        case ctDateTime:
        {
          uint64_t l_numeric = readInt(file, 8);
          unixTime = unixTimeEpoch + (int)round((l_numeric-epoch)/factor) - 1;
          unixTime += 60*60;
// this if GMT. Convert to local time used by VDR
          struct tm tm_r;
          struct tm *time = gmtime_r(&unixTime, &tm_r);
          time->tm_isdst = -1;  // let mktime find out if daylight saving time (DST) is in effect
          numeric = mktime(time);

          break;
        }

        case ctTwoByte:
        {
           numeric = readInt(file, 2);
           if (numeric == 0xFFFF) numeric = 1;

           break;
        }
      }
    }

    void readString(FILE* file)
    {
       int length = readInt(file, 2);

       cToSvConcat result;
       for (int i = 0; i < length; i++)
       {
          int ch = fgetc(file);
          if (ch<128) result.append(1, ch);
          else {
            result.append(1, 0xc0 | ch >> 6);
            result.append(1, 0x80 | (ch & 0x3f));
          }
       }
       result.replaceAll("<br>", "\n");
       replaceUtf16(result);
       stringContent = result;
    }

    static uint64_t readInt(FILE* file, int bytes)
    {
       uint64_t l_numeric = 0;

       for (int i = 0; i < bytes; i++)
       {
          int byte = fgetc(file);
          l_numeric += (uint64_t)byte << (8*i);
       }

       return l_numeric;
    }

    ContentType type;
    std::string stringContent;
    uint64_t numeric;
    time_t unixTime;
};
// cTvmEvent ***********************************************

// cTvmEpg   ***********************************************
cTvmEpg::cTvmEpg(cSv extChannelId, bool debug):
  iExtEpgForChannel(),
  m_extChannelId(extChannelId),
  m_debug(debug),
  m_events(std::make_shared<std::vector<cTvmEvent>>() )
{
// path to tmp / in mem file system
  if (CheckDirExistsRam("/dev/shm/")) m_tmp_path = "/dev/shm/tvscraper_tvm";
  else m_tmp_path = "/tmp/tvscraper_tvm";
  m_error = !CreateDirectory(m_tmp_path);
  if (m_error) {
    esyslog("tvscraper tvm, ERROR creating folder %s", m_tmp_path);
    return;
  }

  if (!downloadAndUnzip(cToSvConcat("tvdaten-premium-", extChannelId, ".cftv"))) return;
  downloadAndUnzip(cToSvConcat("tvbilder-premium-", extChannelId, ".cftv"));
  m_error = false; // ignore errors for image downloads

  std::map<int, cTvmEvent> table;
  struct stat buffer;
  for (int num = 0; num < 2; num++) {

    cToSvConcat filename_u(m_tmp_path, "/", extChannelId, ".tv", num+1);
    m_error = stat(filename_u.c_str(), &buffer) != 0;
    if (m_error) {
      esyslog("tvscraper tvm, ERROR extracting zip, file \"%s\", does not exist", filename_u.c_str());
      return;
    }

    FILE *file = fopen(filename_u.c_str() , "r");
    m_error = !file;
    if (m_error) {
      esyslog("tvscraper tvm, ERROR opening \"%s\", status was '%s'", filename_u.c_str(), strerror(errno));
      return;
    }

    fseek(file, 0, SEEK_SET);

    int ch;
    int row = 0, col = 0;
    for (int i = 0; i < 10; i++)
    {
       row *= 10;
       row += fgetc(file) - '0';
    }

    while ((ch = fgetc(file)) != '\f')
    {
       col *= 10;
       col += ch - '0';
    }

    fseek(file, 40, SEEK_SET);                    // seek to data section

    for (int c = 0; c < col; c++)
    {
      Field header(file);
      auto l_attr_i = g_TvmAttributes.find(cSv(header.getString()));
      if (l_attr_i == g_TvmAttributes.end()) {
        esyslog("tvscraper tvm, ERROR field %s not known", header.getString().c_str() );
        continue;
      }
      eTvmAttributes l_attr = l_attr_i->m_arribute;

      for (int r = 0; r < row; r++) {
         Field field(file);

         if (field.isNumeric()) table[r].setAttribute(l_attr, field.getNumeric() );
         else if (field.isString()) table[r].setAttribute(l_attr, field.getString());
      }
    }

    fclose(file);
//  removeFile(filename_u);
  }
  for (const auto& [key, value] : table) m_events->push_back(value);
  if (m_events->size() != 0) {
    std::sort(m_events->begin(), m_events->end());
//    cTvmEvent *tvmEvent = &(*m_events)[std::min(m_events->size()-1, (size_t)40)];
//    dsyslog("tvscraper tvm, extchannlid %.*s event start %lld, end %lld, Titel \"%s\"", (int)extChannelId.length(), extChannelId.data() , (long long)tvmEvent->m_startTime, (long long)tvmEvent->m_endTime, tvmEvent->getString(eTvmAttributes::Titel).c_str() );
  } else {
    esyslog("tvscraper tvm, extchannlid %.*s no events found", (int)extChannelId.length(), extChannelId.data() );
  }
}

bool cTvmEpg::downloadAndUnzip(cSv filename) {
// in case of error: return false and set m_error to true
// download and extract to m_tmp_path
  cToSvConcat url("http://www.clickfinder.de/daten/onlinedata/cftv520/", filename);
  cToSvConcat l_filename(m_tmp_path, "/", filename);

  std::string error_text;
  m_error = cCurl().GetUrlFile(url, l_filename, &error_text) != 0;
  if (m_error) {
    esyslog("tvscraper tvm, ERROR downloading %s to %s, error message: %s", url.c_str(), l_filename.c_str(), error_text.c_str()  );
    return false;
  }
  cToSvFileN<30> file_content(l_filename); // read max 30 bytes
  m_error = cSv(file_content).length() < 30;
  if (m_error) {
    esyslog("tvscraper tvm, ERROR downloading %s, file content: \"%s\", too short", url.c_str(), file_content.c_str() );
    return false;
  }
  cSv http_response = "<!DOCTYPE HTML";
  m_error = cSv(file_content).compare(0, http_response.length(), http_response) == 0;
  if (m_error) {
    esyslog("tvscraper tvm, ERROR downloading %s, file content: \"%.*s\", contains DOCTYPE HTML, extchannlid %.*s seems to be invalid", url.c_str(), 30, file_content.c_str(), (int)m_extChannelId.length(), m_extChannelId.data() );
    return false;
  }
  cToSvConcat command("unzip -o -qq -P ");
  command.append(7, (char)0xb7);
  command.concat(" -d ", m_tmp_path, " ", l_filename);
  m_error = system(command.c_str() ) < 0;
  if (m_error) {
    esyslog("tvscraper tvm, ERROR extracting zip, command \"%s\", failed", command.c_str());
    return false;
  }
  return true;
}

bool cTvmEpg::findTvmEvent(std::vector<cTvmEvent>::const_iterator &event_it, const cStaticEvent *event) const {
// return true if event was found
  if (m_events->size() == 0) return false;
//  dsyslog("tvscraper: cTvmEpg::findTvmEvent, event->StartTime() %d", (int)event->StartTime() );
  event_it = std::lower_bound(m_events->begin(), m_events->end(), event->StartTime(),
    [](const cTvmEvent &e1, time_t en)
      { return e1.m_startTime < en; });
  if (event_it == m_events->end() ) {
    event_it--;
    return eventMatch(event_it, event) < c_never_accepted_deviation;
  }
  int dh = eventMatch(event_it, event);
  if (dh == 0) return true; // exact match found
  if (event_it == m_events->begin() ) return dh < c_never_accepted_deviation;
  event_it--;
  int dl = eventMatch(event_it, event);
  if (dh < dl) { dl = dh; event_it++; }
  return dl < c_never_accepted_deviation;
}

int cTvmEpg::eventMatch(std::vector<cTvmEvent>::const_iterator event_it, const cStaticEvent *event) const {
  int deviationStart = std::abs(event_it->m_startTime - event->StartTime() );
//   int deviationEnd   = std::abs(event_it->m_endTime - event->EndTime() );  // end is not reliable on TVM data
  int deviation = deviationStart;
  if (deviation <= c_always_accepted_deviation || deviation >= c_never_accepted_deviation) return deviation;
// compare event->Title() with event_it title
  if (!event_it->isString(eTvmAttributes::Titel)) return deviation;

  if (event_it->getString(eTvmAttributes::Titel).empty() || !event->Title() ) return deviation;
  int sd = cNormedString(event->Title() ).sentence_distance(event_it->getString(eTvmAttributes::Titel) );  // sd == 0 if titles are identical
  if (sd > 600) return c_never_accepted_deviation;
  return c_always_accepted_deviation + (deviation - c_always_accepted_deviation) * sd / 600;
}
void cTvmEpg::enhanceEvent(cStaticEvent *event, std::vector<cTvMedia> &extEpgImages) {

  std::vector<cTvmEvent>::const_iterator event_it;
  if (!findTvmEvent(event_it, event)) {
//    if (event->StartTime() < time(0) + 13*24*60*60 && m_extChannelId == "1") dsyslog("tvscraper tvm: no match found: event->StartTime() %lld event->EndTime() %lld, event->Title() \"%s\"", (long long)event->StartTime(), (long long)event->EndTime(), event->Title() );
    return;
  }
//     if (m_extChannelId == "1") dsyslog("tvscraper tvm: match found: event->StartTime() %lld event_it->m_startTime %lld, event_it->Title \"%s\"", (long long)event->StartTime(), (long long)event_it->m_startTime, event_it->getString(eTvmAttributes::Titel).c_str() );

// event found, set the event description
// note: VDR will remove control charaters (always): *p == 0x86 || *p == 0x87 || *p == 0x0D
//  if Setup.EPGBugfixLevel > 1: VDR will remove "space", following the first "space" character. "space" is everything < 31
  std::string descr;
  descr.reserve(700);

// Kurzkritik
  if (event_it->isStringNotEmpty(eTvmAttributes::Kurzkritik) ) { stringAppendRemoveControlCharacters(descr, event_it->getString(eTvmAttributes::Kurzkritik).c_str() ); descr += "\n"; }
// KurzBeschreibung
  if (event_it->isStringNotEmpty(eTvmAttributes::KurzBeschreibung) ) { stringAppendRemoveControlCharacters(descr, event_it->getString(eTvmAttributes::KurzBeschreibung).c_str() ); descr += "\n"; }
// block: Genre, Kategorie, Land, Jahr

  if (event_it->isStringNotEmpty(eTvmAttributes::Genre)) stringAppend(descr, "Genre: ", event_it->getString(eTvmAttributes::Genre), "\n");
  if (event_it->isStringNotEmpty(eTvmAttributes::Keywords)) stringAppend(descr, "Kategorie: ", event_it->getString(eTvmAttributes::Keywords), "\n");

  if (event_it->isStringNotEmpty(eTvmAttributes::Herstellungsland)) stringAppend(descr, "Land: ", event_it->getString(eTvmAttributes::Herstellungsland), "\n");
  if (event_it->isStringNotEmpty(eTvmAttributes::Herstellungsjahr)) stringAppend(descr, "Jahr: ", event_it->getString(eTvmAttributes::Herstellungsjahr), "\n");
  if (event_it->isStringNotEmpty(eTvmAttributes::Originaltitel)) stringAppend(descr, "Originaltitel: ", event_it->getString(eTvmAttributes::Originaltitel), "\n");
  if (event_it->isInt(eTvmAttributes::FSK)) stringAppend(descr, "FSK: ", event_it->getInt(eTvmAttributes::FSK), "\n");
  descr += "\n";
  if (event_it->isInt(eTvmAttributes::Bewertung)) {
    int bew = event_it->getInt(eTvmAttributes::Bewertung);
    stringAppend(descr, "Bewertung: ", bew);
    if (bew == 6) stringAppend(descr, " Toptipp");
    if (bew == 6 || bew == 5) stringAppend(descr, " Sehr empfehlenswert");
    if (bew == 4) stringAppend(descr, " Empfehlenswert");
    stringAppend(descr, "\n");
  }
  if (event_it->isStringNotEmpty(eTvmAttributes::Bewertungen)) stringAppend(descr, "Bewertungen: ", event_it->getString(eTvmAttributes::Bewertungen), "\n");


  if (event_it->isStringNotEmpty(eTvmAttributes::Beschreibung)) { descr += "\n"; stringAppendRemoveControlCharactersKeepNl(descr, event_it->getString(eTvmAttributes::Beschreibung).c_str() ); descr += "\n";}
  if (event_it->isStringNotEmpty(eTvmAttributes::Darsteller)) { descr += "\nDarsteller: "; stringAppendRemoveControlCharactersKeepNl(descr, event_it->getString(eTvmAttributes::Darsteller).c_str() ); descr += "\n";}

  if (event_it->isStringNotEmpty(eTvmAttributes::Regie)) stringAppend(descr, "\nRegisseur: ", event_it->getString(eTvmAttributes::Regie), "\n");

  descr += "\nQuelle: tvm";
  if (!event->ShortText() || !*event->ShortText() &&
       event->Description() && strlen(event->Description()) > 3 && cSv(event->Description()).substr(0, 3) == "..." ) {
      event->SetShortText(event->Description() );
  }
  event->SetDescription(descr.c_str());

  if (!event->ShortText() || !*event->ShortText() ) {
// add a short text only if no short text is available from EIT (the TV station)
    std::string shortText;
    if (event_it->isStringNotEmpty(eTvmAttributes::Originaltitel) ) {
      stringAppendRemoveControlCharacters(shortText, event_it->getString(eTvmAttributes::Originaltitel).c_str() );
      event->SetShortText(shortText.c_str() );
    }
    else if (event_it->isStringNotEmpty(eTvmAttributes::Kurzkritik) ) {
      stringAppendRemoveControlCharacters(shortText, event_it->getString(eTvmAttributes::Kurzkritik).c_str() );
      event->SetShortText(shortText.c_str() );
    }
    else if (event_it->isStringNotEmpty(eTvmAttributes::KurzBeschreibung) ) {
      stringAppendRemoveControlCharacters(shortText, event_it->getString(eTvmAttributes::KurzBeschreibung).c_str() );
      event->SetShortText(shortText.c_str() );
    } else {
      stringAppendRemoveControlCharacters(shortText, event_it->getString(eTvmAttributes::Genre).c_str() );
      if (event_it->isStringNotEmpty(eTvmAttributes::Herstellungsland)) { if (!shortText.empty()) shortText += " / "; shortText += event_it->getString(eTvmAttributes::Herstellungsland); }
      if (event_it->isStringNotEmpty(eTvmAttributes::Herstellungsjahr)) { if (!shortText.empty()) shortText += " / "; shortText += event_it->getString(eTvmAttributes::Herstellungsjahr); }
      event->SetShortText(shortText.c_str() );
    }
  }

// image from external EPG provider
  if (event_it->isStringNotEmpty(eTvmAttributes::Bilddateiname)) {
    cToSvConcat img_path(m_tmp_path, "/", event_it->getString(eTvmAttributes::Bilddateiname));
    struct stat buffer;
    if (stat(img_path.c_str(), &buffer) != 0) esyslog("tvscraper tvm, ERROR image path %s does not exist", img_path.c_str() );
    else {
      cTvMedia tvMedia;
      tvMedia.width = 200;
      tvMedia.height = 132;
      tvMedia.path = img_path;
      extEpgImages.push_back(tvMedia);
    }
  }

  return;
}

//***************************************************************************

extern "C" iExtEpg* extEpgPluginCreator(bool debug) {
// debug indicates to write more messages to syslog
  return new cExtEpgTvm(debug);
}

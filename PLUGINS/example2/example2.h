#define PLUGIN_NAME_I18N "tvscraper-plugin-example"
#include <memory>
#include "../../extEpgPlugin.h"
#include "../../tools/stringhelpers.h"
#include "../../tools/jsonHelpers.c"
#include "../../tools/curlfuncs.h"
#include "../../config.h"
#include "../../sEvent.h"


enum class eTvmAttributes {
  Beschreibung,
  Bewertung,
  Bewertungen,
  Bilddateiname,
  Darsteller,
  FSK,
  Genre,
  Herstellungsjahr,
  Herstellungsland,
  HtmlSeite,
  Kategorietext,
  Keywords,
  KurzBeschreibung,
  Kurzkritik,
  Kz16zu9,
  KzFilm,
  KzLive,
  KzSchwarzweis,
  Originaltitel,
  Regie,
  Titel,
  list_end,
  Beginn,
  Ende,
  Aktualisierung,
  Dauer,
  InhaltID,
  KzAudiodescription,
  KzBilddateiHeruntergeladen,
  KzDolby,
  KzDolbyDigital,
  KzDolbySurround,
  KzHDTV,
  KzImArchiv,
  KzInFavoritenliste,
  KzStereo,
  KzTelefon,
  KzUncoded,
  KzUntertitel,
  KzWiederholung,
  KzZweikanalton,
  Pos,
  SenderKennung,
  SendungID,
  Showview,
  ShowviewVPS,
  VPS,
};
class cTvmAttribute {
  public:
    cTvmAttribute(cSv name, eTvmAttributes arribute): m_name(name), m_arribute(arribute) {}
    cSv m_name;
    eTvmAttributes m_arribute;
};
bool operator==(const cTvmAttribute &a1, const cTvmAttribute &a2) { return a1.m_arribute == a2.m_arribute; }
bool operator< (const cTvmAttribute &a1, const cTvmAttribute &a2) { return a1.m_name < a2.m_name; }
bool operator< (const cSv &name, const cTvmAttribute &a2) { return name < a2.m_name; }
bool operator< (const cTvmAttribute &a1, const cSv &name) { return a1.m_name < name; }

inline static const cSortedVector<cTvmAttribute, std::less<>> g_TvmAttributes = {
{ "Aktualisierung", eTvmAttributes::Aktualisierung },
{ "Beginn", eTvmAttributes::Beginn },
{ "Beschreibung", eTvmAttributes::Beschreibung },
{ "Bewertung", eTvmAttributes::Bewertung },
{ "Bewertungen", eTvmAttributes::Bewertungen },
{ "Bilddateiname", eTvmAttributes::Bilddateiname },
{ "Darsteller", eTvmAttributes::Darsteller },
{ "Dauer", eTvmAttributes::Dauer },
{ "Ende", eTvmAttributes::Ende },
{ "FSK", eTvmAttributes::FSK },
{ "Genre", eTvmAttributes::Genre },
{ "Herstellungsjahr", eTvmAttributes::Herstellungsjahr },
{ "Herstellungsland", eTvmAttributes::Herstellungsland },
{ "HtmlSeite", eTvmAttributes::HtmlSeite },
{ "InhaltID", eTvmAttributes::InhaltID },
{ "Kategorietext", eTvmAttributes::Kategorietext },
{ "Keywords", eTvmAttributes::Keywords },
{ "KurzBeschreibung", eTvmAttributes::KurzBeschreibung },
{ "Kurzkritik", eTvmAttributes::Kurzkritik },
{ "Kz16zu9", eTvmAttributes::Kz16zu9 },
{ "KzAudiodescription", eTvmAttributes::KzAudiodescription },
{ "KzBilddateiHeruntergeladen", eTvmAttributes::KzBilddateiHeruntergeladen },
{ "KzDolby", eTvmAttributes::KzDolby },
{ "KzDolbyDigital", eTvmAttributes::KzDolbyDigital },
{ "KzDolbySurround", eTvmAttributes::KzDolbySurround },
{ "KzFilm", eTvmAttributes::KzFilm },
{ "KzHDTV", eTvmAttributes::KzHDTV },
{ "KzImArchiv", eTvmAttributes::KzImArchiv },
{ "KzInFavoritenliste", eTvmAttributes::KzInFavoritenliste },
{ "KzLive", eTvmAttributes::KzLive },
{ "KzSchwarzweis", eTvmAttributes::KzSchwarzweis },
{ "KzStereo", eTvmAttributes::KzStereo },
{ "KzTelefon", eTvmAttributes::KzTelefon },
{ "KzUncoded", eTvmAttributes::KzUncoded },
{ "KzUntertitel", eTvmAttributes::KzUntertitel },
{ "KzWiederholung", eTvmAttributes::KzWiederholung },
{ "KzZweikanalton", eTvmAttributes::KzZweikanalton },
{ "Originaltitel", eTvmAttributes::Originaltitel },
{ "Pos", eTvmAttributes::Pos },
{ "Regie", eTvmAttributes::Regie },
{ "SenderKennung", eTvmAttributes::SenderKennung },
{ "SendungID", eTvmAttributes::SendungID },
{ "Showview", eTvmAttributes::Showview },
{ "ShowviewVPS", eTvmAttributes::ShowviewVPS },
{ "Titel", eTvmAttributes::Titel },
{ "VPS", eTvmAttributes::VPS },
};

class cTvmEvent
{
  public:
    cTvmEvent() { for (uint64_t &i: m_content_i) i = 0; for (int &i: m_content_t) i = 0; }
    void setAttribute(eTvmAttributes attr, const std::string &s) {
      if ((int)attr < (int)eTvmAttributes::list_end) {
        m_content_t[(int)attr] = 1; // 1 -> string
        m_content_s[(int)attr] = s;
      }
    }
    void setAttribute(eTvmAttributes attr, uint64_t i) {
      if ((int)attr < (int)eTvmAttributes::list_end) {
        m_content_t[(int)attr] = 2; // 2 -> int
        m_content_i[(int)attr] = i;
      } else {
        if (attr == eTvmAttributes::Beginn) m_startTime = i;
        if (attr == eTvmAttributes::Ende)   m_endTime = i;
      }
    }
    bool isAvailabel(eTvmAttributes attr) const { return (int)attr < (int)eTvmAttributes::list_end?m_content_t[(int)attr] != 0:false; }
    bool isString(eTvmAttributes attr) const { return (int)attr < (int)eTvmAttributes::list_end?m_content_t[(int)attr] == 1:false; }
    bool isStringNotEmpty(eTvmAttributes attr) const { return isString(attr)?!m_content_s[(int)attr].empty():false; }
    bool isInt(eTvmAttributes attr) const { return (int)attr < (int)eTvmAttributes::list_end?m_content_t[(int)attr] == 2:false; }
    const std::string &getString(eTvmAttributes attr) const { return m_content_s[(int)attr]; }
    uint64_t getInt(eTvmAttributes attr) const { return m_content_i[(int)attr]; }

    time_t m_startTime = 0;
    time_t m_endTime = 0;
    std::array<int,         (int)eTvmAttributes::list_end> m_content_t;
    std::array<uint64_t,    (int)eTvmAttributes::list_end> m_content_i;
    std::array<std::string, (int)eTvmAttributes::list_end> m_content_s;
};
bool operator<(const cTvmEvent &e1, const cTvmEvent &e2) { return e1.m_startTime < e2.m_startTime; }

class cTvmEpg: public iExtEpgForChannel
{
  public:
    cTvmEpg(cSv extChannelId, bool debug);
    virtual void enhanceEvent(cStaticEvent *event, std::vector<cTvMedia> &extEpgImages);
    int eventMatch(std::vector<cTvmEvent>::const_iterator event_it, const cStaticEvent *event) const;
    bool findTvmEvent(std::vector<cTvmEvent>::const_iterator &event_it, const cStaticEvent *event) const;
    bool downloadAndUnzip(cSv filename);
  private:
    cSv m_extChannelId;
    bool m_debug;
    bool m_error;
    std::shared_ptr<std::vector<cTvmEvent>> m_events;
    const int c_always_accepted_deviation = 60;  // seconds
    const int c_never_accepted_deviation = 60 * 30;  // seconds
    const char *m_tmp_path;
};

class cExtEpgTvm: public iExtEpg
{
  public:
    cExtEpgTvm(bool debug): m_debug(debug) {}
    virtual const char *getSource() { return "tvm"; }
    virtual bool myDescription(const char *description) {
// return true if this description was created by us
      if (!description) return false;
// the descriptions created by us contain "tvm"
      return strstr(description, "tvm") != NULL;
    }
    virtual std::shared_ptr<iExtEpgForChannel> getHandlerForChannel(const std::string &extChannelId) {
      return std::make_shared<cTvmEpg>(extChannelId, m_debug);
    }
  private:
     bool m_debug;
};

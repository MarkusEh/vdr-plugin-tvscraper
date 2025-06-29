#include <fstream>
#include <string.h>

void removeChars(std::string& str, const char* ignore);

//***************************************************************************
// Load Channelmap
//***************************************************************************

int loadChannelmap(std::vector<sChannelMapEpg> &channelMap, const std::vector<std::shared_ptr<iExtEpg>> &extEpgs) {
  std::ifstream cmfile;
  std::string s;
  size_t p;
  int line = 0;
  int count = 0;
  int status = 1;

  enum ConfIndex
  {
    ciSource,
    ciExtid,
    ciMerge,
    ciVps,

    ciCount
  };

  CONCATENATE(path, cPlugin::ConfigDirectory(PLUGIN_NAME_I18N), "/channelmap.conf");
  cmfile.open(path);

  if (cmfile.fail())
  {
    esyslog("tvscraper, '%s' does not exist, external EPG disabled (see README.md). Message: '%s'", path, strerror(errno));
    return 0;
  }

  esyslog("tvscraper, loading '%s'", path);

  while (!cmfile.eof())
  {
    char* left = 0;
    char* right = 0;
    char* extid = 0;
    char* source = 0;
    char* pc;
    int index;
    int vps = 0;
    int merge = 1;

    getline(cmfile, s);
    line++;

    removeChars(s, " \f\n\r\t\v");

    // remove comments

    p = s.find_first_of("//");

    if (p != string::npos)
       s.erase(p);

    if (s.empty())
       continue;

    // split line at '='

    p = s.find_first_of("=");

    if ((p == string::npos) || !s.substr(p+1).length())
    {
      esyslog("tvscraper: ERROR parsing '%s' at line %d!", path, line);
      status = 0;
      break;
    }

    left = strdup(s.substr(0, p).c_str());
    right = strdup(s.substr(p+1).c_str());

    for (index = 0, pc = strtok(left, ":"); pc; pc = strtok(0, ":"), index++)
    {
      switch (index)
      {
        case ciSource: source = strdup(pc);              break;
        case ciExtid:  extid = strdup(pc);               break;
        case ciMerge:  merge = atoi(pc);                 break;
        case ciVps:    vps = strchr("1yY", *pc) != 0;    break;
      }
    }

    free(left);

    if (!right || !source || !extid)
    {
      esyslog("tvscraper: ERROR: Syntax error in '%s' at line %d!", path, line);

      free(right);
      free(source);
      free(extid);
      status = 0;
      break;
    }

    sChannelMapEpg channelMapEpg;
// find plugin that can handle source
    for (auto &extEpg: extEpgs) {
      if (!extEpg->getSource() ) continue;
      if (strcmp(extEpg->getSource(), source) != 0) continue;
      channelMapEpg.extEpg = extEpg;
      break;
    }
    if (!channelMapEpg.extEpg) {
      esyslog("tvscraper: ERROR parsing '%s' at line %d, no plugin for source %s found", path, line, source);
    } else if (merge == 2) {
      esyslog("tvscraper: ERROR parsing '%s' at line %d, merge == 2 not supported by tvscraper, this line is ignored", path, line);
    } else if (merge != 1) {
      isyslog("tvscraper: INFO parsing '%s' at line %d, merge == %d not supported by tvscraper, this line is ignored", path, line, merge);
    } else {
      channelMapEpg.extid = extid;   // channel ID of external EPG provider
      channelMapEpg.source = source; // id of external EPG provider
      channelMapEpg.merge = merge;
      channelMapEpg.vps = vps;

  // read channels separated by commas
      for (index = 0, pc = strtok(right, ","); pc; pc = strtok(0, ","), index++)
      {
        channelMapEpg.channelID = tChannelID::FromString(pc);  // VDR channel ID
        if (!channelMapEpg.channelID.Valid() ) {
          esyslog("tvscraper: ERROR parsing '%s' at line %d, channel %s not valid", path, line, pc);
        } else {
          channelMap.push_back(channelMapEpg);
          count++;
        }
      } // for read channels separated by commas
    } // plugin for source is available

    free(right);
    free(source);
    free(extid);
  }

  cmfile.close();

  std::sort(channelMap.begin(), channelMap.end());

// check duplicate channels
  const char* cpath = path;
  auto v_begin = channelMap.begin();
  auto found = channelMap.begin();
  while ((found = std::adjacent_find(v_begin, channelMap.end() )) != channelMap.end()) {
    esyslog2("two or more entries for channel ", found->channelID, " in '", cpath, "'. All but one will be deleted. Please clean up '", cpath, "' and make sure to have only the correct entry for this channel");
    v_begin = found;
    ++v_begin;
  }
  channelMap.erase(std::unique(channelMap.begin(), channelMap.end()), channelMap.end());

// check channels in VDRs channel.conf
  for (auto &entry: channelMap) {
    bool cfound = false;
    {
      LOCK_CHANNELS_READ;
      for (const cChannel *channel = Channels->First(); channel; channel = Channels->Next(channel)) {
        if (!channel->GroupSep() && channel->GetChannelID() == entry.channelID) { cfound = true; break; }
      }
    }
    if (!cfound) esyslog2("Channel ", entry.channelID, " not found in VDR's channel list. Please clean up '", cpath, "'");
  }

  isyslog("tvscraper: %zu channel mappings read.", channelMap.size() );

  return status;
}

void removeChars(std::string& str, const char* ignore)
{
   const char* s = str.c_str();
   int lenSrc = str.length();
   int lenIgn = strlen(ignore);

   char* dest = (char*)malloc(lenSrc+1); *dest = 0;
   char* d = dest;

   int csSrc;  // size of character
   int csIgn;  //

   for (int ps = 0; ps < lenSrc; ps += csSrc)
   {
      int skip = 0;

      csSrc = std::max(mblen(&s[ps], lenSrc-ps), 1);

      for (int pi = 0; pi < lenIgn; pi += csIgn)
      {
         csIgn = std::max(mblen(&ignore[pi], lenIgn-pi), 1);

         if (csSrc == csIgn && strncmp(&s[ps], &ignore[pi], csSrc) == 0)
         {
            skip = 1;
            break;
         }
      }

      if (!skip)
      {
         for (int i = 0; i < csSrc; i++)
            *d++ = s[ps+i];
      }
   }

   *d = 0;

   str = dest;
   free(dest);
}

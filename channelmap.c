#include <fstream>
#include <string.h>

void removeChars(std::string& str, const char* ignore);

//***************************************************************************
// Load Channelmap
//***************************************************************************

int loadChannelmap(vector<sChannelMapEpg> &channelMap) {
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

  std::string path = concatenate(cPlugin::ConfigDirectory(PLUGIN_NAME_I18N), "/channelmap.conf");
  cmfile.open(path);

  if (cmfile.fail())
  {
    esyslog("tvscraper, '%s' does not exist, external EPG disabled (see README.md). Message: '%s'", path.c_str() , strerror(errno));
    return 0;
  }

  esyslog("tvscraper, loading '%s'", path.c_str() );

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
      esyslog("tvscraper: ERROR parsing '%s' at line %d!", path.c_str() , line);
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
      esyslog("tvscraper: ERROR: Syntax error in '%s' at line %d!", path.c_str() , line);

      free(right);
      free(source);
      free(extid);
      status = 0;
      break;
    }

    // read channels separated by commas
    sChannelMapEpg channelMapEpg;
    channelMapEpg.extid = extid;   // channel ID of external EPG provider
    channelMapEpg.source = source; // id of external EPG provider
    channelMapEpg.merge = merge;
    channelMapEpg.vps = vps;

    for (index = 0, pc = strtok(right, ","); pc; pc = strtok(0, ","), index++)
    {
      channelMapEpg.channelID = tChannelID::FromString(pc);  // VDR channel ID
      if (!channelMapEpg.channelID.Valid() ) {
        esyslog("tvscraper: ERROR parsing '%s' at line %d, channel %s not valid", path.c_str() , line, pc);
      } else {
        channelMap.push_back(channelMapEpg);
        count++;
      }
    }

    free(right);
    free(source);
    free(extid);
  }

  cmfile.close();

  std::sort(channelMap.begin(), channelMap.end());
  esyslog("tvscraper: %d channel mappings read.", count);

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

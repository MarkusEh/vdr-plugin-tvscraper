/*
 * channel helper functions (for vdr cChannel)
 *
*/
#ifndef __CHANNELHELPERS_H
#define __CHANNELHELPERS_H

#include <string>
#include <string_view>
#include <string.h>
#include <vdr/channels.h>

// =========================================================
// some performance improvemnt, to get string presentation for channel
// you can also use channelID.ToString()
// =========================================================

void sourceToBuf(char *buffer, int Code) {
//char buffer[16];
  int st_Mask = 0xFF000000;
  char *q = buffer;
  *q++ = (Code & st_Mask) >> 24;
  if (int n = cSource::Position(Code)) {
     q += snprintf(q, 14, "%u.%u", abs(n) / 10, abs(n) % 10); // can't simply use "%g" here since the silly 'locale' messes up the decimal point
     *q++ = (n < 0) ? 'W' : 'E';
     }
  *q = 0;
}
void channelToBuf(char *buffer, const tChannelID &channelID) {
// char buffer[256];
  char buffer_src[16];
  sourceToBuf(buffer_src, channelID.Source());
  snprintf(buffer, 256, channelID.Rid() ? "%s-%d-%d-%d-%d" : "%s-%d-%d-%d",
     buffer_src, channelID.Nid(), channelID.Tid(), channelID.Sid(), channelID.Rid() );
}
std::string channelToString(const tChannelID &channelID) {
  char buffer[256];
  channelToBuf(buffer, channelID);
  return buffer;
}

inline void stringAppend(std::string &str, const tChannelID &channelID) {
  char buffer[256];
  channelToBuf(buffer, channelID);
  str.append(buffer);
}

std::string objToString(const tChannelID &i) { return channelToString(i); }

template<> tChannelID stringToObj<tChannelID>(const char *s, size_t len) {
  char buf[len+1];
  buf[len] = 0;
  memcpy(buf, s, len);
  return tChannelID::FromString(buf);
}
#endif // __CHANNELHELPERS_H

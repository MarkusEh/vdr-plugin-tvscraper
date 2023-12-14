#ifndef __CURLFUNCS_H_20020513__
#define __CURLFUNCS_H_20020513__
#include <string>
#include <curl/curl.h>
#include <curl/easy.h>
#include "stringhelpers.h"

struct curl_slist *curl_slistAppend(struct curl_slist *slist) { return slist; }
struct curl_slist *curl_slistAppend(struct curl_slist *slist, const char *string);
template<typename... Args>
struct curl_slist *curl_slistAppend(struct curl_slist *slist, const char *string1, const char *string2, const Args... args) {
  slist = curl_slistAppend(slist, string1);
  return curl_slistAppend(slist, string2, args...);
}

template<class T>
bool CurlGetUrl(const char *url, T &sOutput, struct curl_slist *headers = NULL);
template<class T, typename... Args>
bool CurlGetUrl(const char *url, T &sOutput, const Args... args) {
  struct curl_slist *headers = NULL;
  headers = curl_slistAppend(headers, args...);
  bool result = CurlGetUrl(url, sOutput, headers);
  if (headers) curl_slist_free_all(headers);
  return result;
}

int CurlGetUrlFile(const char *url, const char *filename);
bool CurlGetUrlFile2(const char *url, const char *filename, int &err_code, std::string &error);
void FreeCurlLibrary(void);
int CurlSetCookieFile(char *filename);
bool CurlPostUrl(const char *url, cStr sPost, std::string &sOutput, struct curl_slist *headers = NULL);

void stringAppendCurlEscape(std::string &str, cSv url);
inline void InitCurlLibraryIfNeeded();

class cToSvUrlEscape: public cToSv {
  public:
    cToSvUrlEscape(cSv url);
    operator cSv() const { return cSv(m_escaped_url); }
    const char *c_str() const { return m_escaped_url?m_escaped_url:""; }
    ~cToSvUrlEscape() {
      curl_free(m_escaped_url);
    }
  private:
    char *m_escaped_url = nullptr;
};
#endif // __CURLFUNCS_H_20020513__

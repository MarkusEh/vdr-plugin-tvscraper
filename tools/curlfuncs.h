#ifndef __CURLFUNCS_H_20020513__
#define __CURLFUNCS_H_20020513__
#include <string>
#include <curl/curl.h>
#include <curl/easy.h>

struct curl_slist *curl_slistAppend(struct curl_slist *slist) { return slist; }
struct curl_slist *curl_slistAppend1(struct curl_slist *slist, const char *string);
template<typename... Args>
struct curl_slist *curl_slistAppend(struct curl_slist *slist, const char *string, const Args... args) {
  slist = curl_slistAppend1(slist, string);
  return curl_slistAppend(slist, args...);
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
bool CurlPostUrl(const char *url, const std::string &sPost, std::string &sOutput, struct curl_slist *headers = NULL);
std::string CurlEscape(const char *url);
std::string CurlEscape(std::string_view url);

inline void InitCurlLibraryIfNeeded();
#define CURLESCAPE(url_e, url) \
  InitCurlLibraryIfNeeded(); \
  char *output_##url_e = curl_easy_escape(curlfuncs::curl, url, strlen(url)); \
  char url_e[strlen(output_##url_e) + 1]; \
  strcpy(url_e, output_##url_e); \
  curl_free(output_##url_e);

#endif

#include "curlfuncs.h"
#include <string>
#include <cstring>
#include <cstdio>
#include <curl/curl.h>
#include <curl/easy.h>
#include <unistd.h>
#include <iostream>

namespace curlfuncs {
  bool bInitialized = false;
  CURL *curl = NULL;
}

static size_t curl_collect_data_string(void *data, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  std::string *out = (std::string *) userp;
  out->append((char *)data, realsize);
  return realsize;
}

static size_t curl_collect_data_large_string(void *data, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  cLargeString *out = (cLargeString *) userp;
  out->append((char *)data, realsize);
  return realsize;
}

inline void InitCurlLibraryIfNeeded() 
{
  if (!curlfuncs::bInitialized) {
    curl_global_init(CURL_GLOBAL_ALL);
    curlfuncs::curl = curl_easy_init();
    if (!curlfuncs::curl)
      throw string("Could not create new curl instance");
    curl_easy_setopt(curlfuncs::curl, CURLOPT_NOPROGRESS, 1);       // Do not show progress
    curl_easy_setopt(curlfuncs::curl, CURLOPT_FOLLOWLOCATION, 1);
//  curl_easy_setopt(curlfuncs::curl, CURLOPT_USERAGENT, "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.0; Mayukh's libcurl wrapper http://www.mayukhbose.com/)");
    curl_easy_setopt(curlfuncs::curl, CURLOPT_USERAGENT, "User-Agent: 4.2 (Nexus 10; Android 6.0.1; de_DE)");
    curlfuncs::bInitialized = true;
  }
}

bool CurlGetUrl_int(const char *url, void *sOutput, struct curl_slist *headers, size_t (*func)(void *data, size_t size, size_t nmemb, void *userp) )
{
  InitCurlLibraryIfNeeded();

  curl_easy_setopt(curlfuncs::curl, CURLOPT_URL, url);            // Set the URL to get
  curl_easy_setopt(curlfuncs::curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curlfuncs::curl, CURLOPT_HTTPGET, 1);
  curl_easy_setopt(curlfuncs::curl, CURLOPT_WRITEFUNCTION, func);
  curl_easy_setopt(curlfuncs::curl, CURLOPT_WRITEDATA, sOutput);       // Set option to write to string
  curl_easy_setopt(curlfuncs::curl, CURLOPT_ACCEPT_ENCODING, "");
  return curl_easy_perform(curlfuncs::curl) == 0;
}
bool CurlGetUrl_int(const char *url, string &sOutput, struct curl_slist *headers) {
  return CurlGetUrl_int(url, &sOutput, headers, curl_collect_data_string);
}
bool CurlGetUrl_int(const char *url, cLargeString &sOutput, struct curl_slist *headers) {
  return CurlGetUrl_int(url, &sOutput, headers, curl_collect_data_large_string);
}

template<class T>
bool CurlGetUrl(const char *url, T &sOutput, struct curl_slist *headers) {
  bool ret = false;
  int i;
  for(i=0; i < 20; i++) {
    sOutput.clear();
    ret = CurlGetUrl_int(url, sOutput, headers);
    if (ret && sOutput.length() > 10) {
      if(sOutput[0] == '{') return true; // json file, OK
      if(sOutput[0] == '[') return true; // json file, OK
      if(strncmp(sOutput.data(), "<html>", 6) != 0 && sOutput[0] == '<') return true; // xml  file, OK
//    if("<html>"sv.compare(0, 6, sOutput) != 0 && sOutput[0] == '<') return true; // xml  file, OK
    }
    sleep(2 + 3*i);
//  if (config.enableDebug) esyslog("tvscraper: rate limit calling \"%s\", i = %i output \"%s\"", url, i, sOutput.substr(0, 20).c_str() );
//  cout << "output from curl: " << sOutput.substr(0, 20) << std::endl;
  }
  esyslog("tvscraper: CurlGetUrl ERROR calling \"%s\", tried %i times, output \"%s\"", url, i, sOutput.erase(20).c_str() );
  return false;
}
// template bool CurlGetUrl<std::string> (const char *url,  std::string &sOutput, struct curl_slist *headers);
template bool CurlGetUrl<cLargeString>(const char *url, cLargeString &sOutput, struct curl_slist *headers);

int CurlGetUrlFile(const char *url, const char *filename)
{
  int nRet = 0;
  InitCurlLibraryIfNeeded();
  
  // Point the output to a file
  FILE *fp;
  if ((fp = fopen(filename, "w")) == NULL) return 0;

  curl_easy_setopt(curlfuncs::curl, CURLOPT_HTTPHEADER, NULL);
  curl_easy_setopt(curlfuncs::curl, CURLOPT_WRITEFUNCTION, NULL);
  curl_easy_setopt(curlfuncs::curl, CURLOPT_WRITEDATA, fp);       // Set option to write to file
  curl_easy_setopt(curlfuncs::curl, CURLOPT_URL, url);            // Set the URL to get
  curl_easy_setopt(curlfuncs::curl, CURLOPT_HTTPGET, 1);
  if (curl_easy_perform(curlfuncs::curl) == 0)
    nRet = 1;
  else
    nRet = 0;

  fclose(fp);
  return nRet;
}

bool CurlGetUrlFile2(const char *url, const char *filename, int &err_code, string &error)
{
  CURLcode res;
  err_code = 0;
  error = "";
  InitCurlLibraryIfNeeded();

  // Point the output to a file
  FILE *fp;
  if ((fp = fopen(filename, "w")) == NULL) {
    err_code = -2;
    error = "Error opening file for write";
    return false;
  }

  curl_easy_setopt(curlfuncs::curl, CURLOPT_HTTPHEADER, NULL);
  curl_easy_setopt(curlfuncs::curl, CURLOPT_WRITEFUNCTION, NULL);
  curl_easy_setopt(curlfuncs::curl, CURLOPT_WRITEDATA, fp);       // Set option to write to file
  curl_easy_setopt(curlfuncs::curl, CURLOPT_URL, url);            // Set the URL to get
  curl_easy_setopt(curlfuncs::curl, CURLOPT_HTTPGET, 1);
  res =  curl_easy_perform(curlfuncs::curl);
  fclose(fp);
  if(res == CURLE_OK) return true;
  error = curl_easy_strerror(res);
  err_code = res;
  return false;
}

bool CurlPostUrl(const char *url, const string &sPost, string &sOutput, struct curl_slist *headers) {
  InitCurlLibraryIfNeeded();
  curl_easy_setopt(curlfuncs::curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curlfuncs::curl, CURLOPT_URL, url);
  curl_easy_setopt(curlfuncs::curl, CURLOPT_POST, 1);
  curl_easy_setopt(curlfuncs::curl, CURLOPT_POSTFIELDS, sPost.c_str() );

  curl_easy_setopt(curlfuncs::curl, CURLOPT_WRITEFUNCTION, curl_collect_data_string);
  curl_easy_setopt(curlfuncs::curl, CURLOPT_WRITEDATA, &sOutput);
  if (curl_easy_perform(curlfuncs::curl) == CURLE_OK) return true;
  return false;
}

void FreeCurlLibrary(void)
{
  if (curlfuncs::curl)
    curl_easy_cleanup(curlfuncs::curl);
  curl_global_cleanup();
  curlfuncs::bInitialized = false;
}

int CurlSetCookieFile(char *filename)
{
  InitCurlLibraryIfNeeded();
  if (curl_easy_setopt(curlfuncs::curl, CURLOPT_COOKIEFILE, filename) != 0)
    return 0;
  if (curl_easy_setopt(curlfuncs::curl, CURLOPT_COOKIEJAR, filename) != 0)
    return 0;
  return 1;
}
std::string CurlEscape(const char *url) {
  InitCurlLibraryIfNeeded();
  char *output = curl_easy_escape(curlfuncs::curl, url, strlen(url));
  std::string result(output);
  curl_free(output);
  return result;
}
std::string CurlEscape(string_view url) {
  InitCurlLibraryIfNeeded();
  char *output = curl_easy_escape(curlfuncs::curl, url.data(), url.length());
  std::string result(output);
  curl_free(output);
  return result;
}

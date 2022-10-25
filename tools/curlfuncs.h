#ifndef __CURLFUNCS_H_20020513__
#define __CURLFUNCS_H_20020513__
#include <string>
using namespace std;

template<class T>
bool CurlGetUrl(const char *url, T &sOutput, struct curl_slist *headers = NULL);
int CurlGetUrlFile(const char *url, const char *filename);
bool CurlGetUrlFile2(const char *url, const char *filename, int &err_code, string &error);
void FreeCurlLibrary(void);
int CurlSetCookieFile(char *filename);
bool CurlPostUrl(const char *url, const string &sPost, string &sOutput, struct curl_slist *headers = NULL);
std::string CurlEscape(const char *url);
std::string CurlEscape(std::string_view url);
#endif

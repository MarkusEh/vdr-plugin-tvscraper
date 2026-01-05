#ifndef __CURLFUNCS_H
#define __CURLFUNCS_H
#include <string>
#include <curl/curl.h>
#include <curl/easy.h>
#include "stringhelpers.h"

inline static size_t curl_collect_data_string(void *data, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  std::string *out = (std::string *) userp;
  out->append((char *)data, realsize);
  return realsize;
}

inline struct curl_slist *curl_slistAppend(struct curl_slist *slist) { return slist; }
inline struct curl_slist *curl_slistAppend(struct curl_slist *slist, const char *string) {
  if (!string) return slist;
  struct curl_slist *temp = curl_slist_append(slist, string);
  if (temp == nullptr) {
    if (slist) curl_slist_free_all(slist);
    esyslog("tvscraper, ERROR in curl_slistAppend: cannot append %s", string?string:"NULL");
  }
  return temp;
}
template<typename... Args>
inline struct curl_slist *curl_slistAppend(struct curl_slist *slist, const char *string1, const char *string2, const Args... args) {
  slist = curl_slistAppend(slist, string1);
  return curl_slistAppend(slist, string2, args...);
}

class cCurl {
  public:
    cCurl &operator= (const cCurl &) = delete;
    cCurl (const cCurl &curl) = delete;

    cCurl() {
// note: we call curl_global_init in cPluginTvscraper::Initialize()
      m_curl = curl_easy_init();
      if (!m_curl) throw std::string("ERROR calling curl_easy_init");
      curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 1L);     // Do not show progress
      curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 60L);       // Timeout 60 Secs. This is required. Otherwise, the thread might hang forever, if router is reset (once a day ...)
      curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
//    curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "User-Agent: 4.2 (Nexus 10; Android 6.0.1; de_DE)");
//    curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:138.0) Gecko/20100101 Firefox/138.0");
      curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/137.0.0.0 Safari/537.36");
    }
    ~cCurl() {
      curl_easy_cleanup(m_curl);
// note: we call curl_global_cleanup in cPluginTvscraper::~cPluginTvscraper()
    }
    bool GetUrl(const cStr url, std::string &sOutput, struct curl_slist *headers) {
// input: url && headers
// return false in case of an error
// output: sOutput (whatever was returned by the server)
// Note: output must be json or xml file. Otherwise, we wait and retry.
      CURLcode ret;
      int i;
      for (i=0; i < 20; i++) {
        sOutput.clear();
        ret = GetUrl_int(url, sOutput, headers);
        if (ret == CURLE_OK && sOutput.length() > 1) {  // [] is OK
          if (sOutput[0] == '{') return true; // json file, OK
          if (sOutput[0] == '[') return true; // json file, OK
          if (sOutput[0] == '<' && sOutput.length() > 6 && strncmp(sOutput.data(), "<html>", 6) != 0) return true; // xml  file, OK
        }
        sleep(2 + 3*i);
//  if (config.enableDebug) esyslog("tvscraper: rate limit calling \"%s\", i = %i output \"%s\"", url, i, sOutput.substr(0, 20).c_str() );
//  cout << "output from curl: " << sOutput.substr(0, 20) << std::endl;
      }
      if (ret == CURLE_OK) {
        esyslog("tvscraper: cCurl::GetUrl ERROR calling \"%s\", tried %i times, http return code %ld, output \"%s\"", url.c_str(), i, getLastHttpResponseCode(), sOutput.substr(0, 30).c_str() );
      } else {
        esyslog("tvscraper: cCurl::GetUrl ERROR calling \"%s\", tried %i times, return code from curl_easy_perform %lld", url.c_str(), i, (long long) ret);
      }
      return false;
    }
template<typename... Args>
    bool GetUrl(const cStr url, std::string &sOutput, const Args... args) {
// provide headers in a list of char *
// otherwise, identical to GetUrl
      struct curl_slist *headers = nullptr;
      headers = curl_slistAppend(headers, args...);
      bool result = GetUrl(url, sOutput, headers);
      if (headers) curl_slist_free_all(headers);
      return result;
    }
    int GetUrlFile(const cStr url, const cStr filename, std::string *error = nullptr) {
// download to a file
// return 0 on success
// otherwise, return error code (and error message in error if requested):
//  -2: Error opening file for write
// other error codes as returned by curl_easy_perform

      FILE *fp;
      if ((fp = fopen(filename.c_str(), "w")) == nullptr) {
        if (error) *error = "Error opening file for write";
        return -2;
      }

      curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, NULL);
      curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, NULL);
      curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, fp);       // Set option to write to file
      curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str() );   // Set the URL to get
      curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1L);
      CURLcode res = curl_easy_perform(m_curl);
      fclose(fp);
      if (res == CURLE_OK) return 0;
      if (error) *error = curl_easy_strerror(res);
      return res;
    }
    int PostUrl(const cStr url, const cStr sPost, std::string &sOutput, struct curl_slist *headers = nullptr) {
// return 0 on success
// otherwise, return error code as returned by curl_easy_perform
      curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str() );
      curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
      curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, sPost.c_str() );

      curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, curl_collect_data_string);
      curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &sOutput);
      return curl_easy_perform(m_curl);
    }
    long getLastHttpResponseCode() {
// call only if last curl_easy_perform() returned 0 (GetUrl() returned true)
      long response_code;
      CURLcode res = curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &response_code);
      if (res == CURLE_OK) return response_code;
      esyslog("tvscraper, ERROR %lld in getLastHttpResponseCode", (long long) res);
      return 0;
    }
template <size_t N>
    cToSvConcat<N>& appendCurlEscape(cToSvConcat<N>& target, cSv url) {
      char *output = curl_easy_escape(m_curl, url.data(), url.length());
      target.append(output);
      curl_free(output);
      return target;
    }
  private:
    CURLcode GetUrl_int(const cStr url, void *sOutput, struct curl_slist *headers, size_t (*func)(void *data, size_t size, size_t nmemb, void *userp) ) {
      curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str() );   // Set the URL to get
      curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1L);
      curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, func);
      curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, sOutput);  // Set option to write to string
      curl_easy_setopt(m_curl, CURLOPT_ACCEPT_ENCODING, "");
      return curl_easy_perform(m_curl);
    }
    CURLcode GetUrl_int(const cStr url, std::string &sOutput, struct curl_slist *headers) {
      return GetUrl_int(url, &sOutput, headers, curl_collect_data_string);
    }
    CURL *m_curl;
};

#endif // __CURLFUNCS_H

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std;

bool CreateDirectory(string dir) {
    mkdir(dir.c_str(), 0775);
    //check if successfull
    DIR *pDir;
    bool exists = false;
    pDir = opendir(dir.c_str());
    if (pDir != NULL) {
        exists = true;    
        closedir(pDir);
    }
    return exists;
}

bool FileExists(string filename) {
    ifstream ifile(filename.c_str());
    if (ifile) {
        //a valid image should be larger then 500 bytes
// test: series;  smallest picture: 2521 bytes. Wrong files: 0 bytes or 243 bytes
// test: moviedb; smallest picture: 1103 bytes. Wrong files: 0 bytes or 150 bytes
        ifile.seekg (0, ifile.end);
        int length = ifile.tellg();
        if (length > 500)
            return true;
    }
    return false;
}

bool CheckDirExists(const char* dirName) {
    struct statfs statfsbuf;
    if (statfs(dirName,&statfsbuf)==-1) return false;
    if ((statfsbuf.f_type!=0x01021994) && (statfsbuf.f_type!=0x28cd3d45)) return false;
    if (access(dirName,R_OK|W_OK)==-1) return false;
    return true;

}

void DeleteFile(string filename) {
    remove(filename.c_str());
}

void DeleteAll(const string &dirname) {
  fs::path pDir(dirname);
  std::error_code ec;
  std::uintmax_t n = fs::remove_all(pDir, ec);
  if (ec.value() != 0) esyslog("tvscraper: ERROR \"%s\", code %i  deleted  \"%s\", %ju files", ec.message().c_str(), ec.value(), dirname.c_str(), n);
  else if (config.enableDebug) esyslog("tvscraper: deleted  \"%s\", %ju files", dirname.c_str(), n);
}

bool Download(const std::string &url, const std::string &localPath) {
  if (FileExists(localPath)) return true;
  string error;
  int err_code;
  if (config.enableDebug) esyslog("tvscraper: download file, url: \"%s\" local path: \"%s\"", url.c_str(), localPath.c_str() );
  for(int i=0; i < 11; i++) {
    if (i !=  0) sleep(i);
    if (i == 10) sleep(i); // extra long sleep before last try
    if (CurlGetUrlFile2(url.c_str(), localPath.c_str(), err_code, error) && FileExists(localPath) ) return true;
  }
  esyslog("tvscraper: ERROR download file, url: \"%s\" local path: \"%s\", error: \"%s\", err_code: %i", url.c_str(), localPath.c_str(), error.c_str(), err_code );
  DeleteFile(localPath);
  return false;
}

bool CopyFile(const std::string &from, const std::string &to) {
  if (!FileExists(from)) return false;
  if ( FileExists(to)) return true;
  fs::path pFrom(from);
  fs::path pTo(to);
  std::error_code ec;
  bool result = fs::copy_file(pFrom, pTo, ec);
  if (ec.value() != 0) esyslog("tvscraper: ERROR \"%s\", code %i  tried to copy \"%s\" to \"%s\"", ec.message().c_str(), ec.value(), from.c_str(), to.c_str() );

  return result;
}

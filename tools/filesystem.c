#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std;

bool CreateDirectory(const char *dir) {
  mkdir(dir, 0775);
  //check if successfull
  DIR *pDir;
  bool exists = false;
  pDir = opendir(dir);
  if (pDir != NULL) {
    exists = true;
    closedir(pDir);
  }
  return exists;
}
bool CreateDirectory(const string &dir) {
  return CreateDirectory(dir.c_str() );
}

bool FileExists(const char *filename) {
// return true if filename exists AND is an image (make some guess checks to test for an image, exclude typical error pages)
  if (!filename) return false;
  struct stat buffer;
  if (stat (filename, &buffer) != 0) return false;
// test: series;  smallest picture: 2521 bytes. Wrong files: 0 bytes or 243 bytes
// test: moviedb; smallest picture: 1103 bytes. Wrong files: 0 bytes or 150 bytes
// note: wrong file with size 2361 found :( . Additional test ...
  if (buffer.st_size > 2500) return true;   // error pages are smaller than 2500
  if (buffer.st_size <  500) return false;  // images are larger than 500

// open and read file to make final decision
  FILE *f = fopen(filename, "rb");
  if (!f) {
    esyslog("tvscraper, FileExists, ERROR: stat OK, fopen fails, filename %s", filename);
    return false;
  }
  char m_s[7];
  size_t num_read = fread (m_s, 1, 6, f);
  fclose(f);
  if (num_read != 6) {
    esyslog("tvscraper, FileExists, ERROR: num_read %zu != 6, filename %s", num_read, filename);
    return false;
  }
  return strncmp(m_s, "<html>", 6) != 0;
}
bool FileExists(const std::string &filename) {
  return FileExists(filename.c_str());
}

bool DirExists(const char *dirname) {
// Return true if dirname exists and is a directory
  if (!dirname) return false;
  struct stat buffer;
  if (stat (dirname, &buffer) != 0) return false;
  return S_ISDIR(buffer.st_mode);
}
bool CheckDirExistsRam(const char* dirName) {
// true if dir exists and is in RAM (CRAMFS or TMPFS)
    struct statfs statfsbuf;
    if (statfs(dirName,&statfsbuf)==-1) return false;
    if ((statfsbuf.f_type!=0x01021994) && (statfsbuf.f_type!=0x28cd3d45)) return false;
// TMPFS_MAGIC  0x01021994
// CRAMFS_MAGIC 0x28cd3d45
    if (access(dirName,R_OK|W_OK)==-1) return false;
    return true;
}

void DeleteFile(const string &filename) {
    remove(filename.c_str());
}

void DeleteAll(const string &dirname) {
  fs::path pDir(dirname);
  std::error_code ec;
  std::uintmax_t n = fs::remove_all(pDir, ec);
  if (ec.value() != 0) esyslog("tvscraper: ERROR \"%s\", code %i  deleted  \"%s\", %ju files", ec.message().c_str(), ec.value(), dirname.c_str(), n);
  else if (config.enableDebug) esyslog("tvscraper: deleted  \"%s\", %ju files", dirname.c_str(), n);
}

bool CheckDownloadAccessDenied(const cLargeString &df) {
// return true if df contains an XML file, indication AccessDenied
// return false, otherwise
  if (df.length() < 50) return false;
// the server responded something. Try to parse the response
  cSv df_Error = partInXmlTag(df, "Error");
  if (df_Error.empty() ) return false;
  cSv df_Code = partInXmlTag(df_Error, "Code");
  if (df_Code != "AccessDenied") return false;
  return true;
}
bool Download(const char *url, const char *localPath) {
  if (FileExists(localPath)) return true;
  std::string error;
  int err_code;
  for(int i=0; i < 11; i++) {
    if (i !=  0) sleep(i);
    if (i == 10) sleep(i); // extra long sleep before last try
    if (CurlGetUrlFile2(url, localPath, err_code, error) ) {
      if (FileExists(localPath) ) {
        if (config.enableDebug) esyslog("tvscraper: successfully downloaded file, url: \"%s\" local path: \"%s\"", url, localPath);
        return true;
      }
      cLargeString df("Downloaded File", localPath);
      if (CheckDownloadAccessDenied(df)) {
        if (config.enableDebug) {
          esyslog("tvscraper: INFO download file failed, url: \"%s\" local path: \"%s\", AccessDenied, content \"%s\"", url, localPath, df.substr(0, 50).c_str() );
        } else
          esyslog("tvscraper: Download file failed, url: \"%s\" local path: \"%s\", AccessDenied", url, localPath);
        remove(localPath);
        return false;
      }
    }
  }
  if (err_code == 0) {
    cLargeString df("Downloaded File", localPath);
    esyslog("tvscraper: ERROR download file, url: \"%s\" local path: \"%s\", content \"%s\"", url, localPath, df.substr(0, 100).c_str() );
  } else esyslog("tvscraper: ERROR download file, url: \"%s\" local path: \"%s\", error: \"%s\", err_code: %i", url, localPath, error.c_str(), err_code );
  remove(localPath);
  return false;
}
bool Download(const std::string &url, const std::string &localPath) {
  return Download(url.c_str(), localPath.c_str() ); }
bool Download(const std::string &url, const char *localPath) {
  return Download(url.c_str(), localPath); }
bool Download(const char *url, const std::string &localPath) {
  return Download(url, localPath.c_str() ); }

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

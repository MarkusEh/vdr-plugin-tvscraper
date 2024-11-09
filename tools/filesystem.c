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

bool CreateDirectory(cStr dir) {
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

bool FileExistsImg(cStr filename) {
// return true if filename exists AND is an image (make some guess checks to test for an image, exclude typical error pages)
  struct stat buffer;
  if (stat (filename, &buffer) != 0) return false;
// test: series;  smallest picture: 2521 bytes. Wrong files: 0 bytes or 243 bytes
// test: moviedb; smallest picture: 1103 bytes. Wrong files: 0 bytes or 150 bytes
// note: wrong file with size 2361 found :( . Additional test ...
  if (buffer.st_size > 2500) return true;   // error pages are smaller than 2500
  if (buffer.st_size <  500) return false;  // images are larger than 500

// open and read file to make final decision
  cToSvFileN<6> fileStart(filename);
  if (!fileStart.exists() ) {
    esyslog("tvscraper, FileExistsImg, ERROR: stat OK, open fails, filename %s", filename.c_str() );
    return false;
  }
  if (cSv(fileStart).length() != 6) {
    esyslog("tvscraper, FileExistsImg, ERROR: num_read %i != 6, filename %s", (int)cSv(fileStart).length(), filename.c_str() );
    return false;
  }
  return strncmp(fileStart.c_str(), "<html>", 6) != 0;
}

bool DirExists(cStr dirname) {
// Return true if dirname exists and is a directory
  struct stat buffer;
  if (stat (dirname, &buffer) != 0) return false;
  return S_ISDIR(buffer.st_mode);
}
bool CheckDirExistsRam(cStr dirName) {
// true if dir exists and is in RAM (CRAMFS or TMPFS)
    struct statfs statfsbuf;
    if (statfs(dirName,&statfsbuf)==-1) return false;
    if ((statfsbuf.f_type!=0x01021994) && (statfsbuf.f_type!=0x28cd3d45)) return false;
// TMPFS_MAGIC  0x01021994
// CRAMFS_MAGIC 0x28cd3d45
    if (access(dirName,R_OK|W_OK)==-1) return false;
    return true;
}

void DeleteFile(cStr filename) {
    remove(filename.c_str());
}

void DeleteAll(cStr dirname) {
  std::error_code ec;
  std::uintmax_t n = fs::remove_all(dirname.c_str(), ec);
  if (ec.value() != 0) esyslog("tvscraper: ERROR \"%s\", code %i  deleted  \"%.*s\", %ju files", ec.message().c_str(), ec.value(), (int)dirname.length(), dirname.data(), n);
  else if (config.enableDebug) esyslog("tvscraper: deleted  \"%.*s\", %ju files", (int)dirname.length(), dirname.data(), n);
}

bool CheckDownloadAccessDenied(cSv df) {
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
bool DownloadImg(cCurl *curl, cStr url, cStr localPath) {
  if (FileExistsImg(localPath)) return true;
  std::string error;
  int err_code;
  for(int i=0; i < 11; i++) {
    if (i !=  0) sleep(i);
    if (i == 10) sleep(i); // extra long sleep before last try
    err_code = curl->GetUrlFile(url, localPath, &error);
    if (err_code == 0) {
      if (FileExistsImg(localPath) ) {
        if (config.enableDebug) esyslog("tvscraper: successfully downloaded file, url: \"%s\" local path: \"%s\"", url.c_str(), localPath.c_str() );
        return true;
      }
      cToSvFile df(localPath);
      if (CheckDownloadAccessDenied(df)) {
        esyslog("tvscraper: download file failed, url: \"%s\" local path: \"%s\", AccessDenied, content \"%.*s\"", url.c_str(), localPath.c_str() , std::min(50, (int)cSv(df).length()), df.data() );
        remove(localPath);
        return false;
      }
    }
  }
  if (err_code == 0) {
    cToSvFileN<50> df(localPath);  // read max 50 bytes
    esyslog("tvscraper: ERROR download file, url: \"%s\" local path: \"%s\", content \"%s\"", url.c_str(), localPath.c_str() , df.c_str() );
  } else esyslog("tvscraper: ERROR download file, url: \"%s\" local path: \"%s\", error: \"%s\", err_code: %i", url.c_str(), localPath.c_str() , error.c_str(), err_code );
  remove(localPath);
  return false;
}

bool CopyFile(cStr from, cStr to) {
// true if the file was copied, false otherwise.
  std::error_code ec;
  bool result = fs::copy_file(from.c_str(), to.c_str(), ec);
  if (ec.value() != 0) esyslog("tvscraper: ERROR \"%s\", code %i  tried to copy \"%.*s\" to \"%.*s\"", ec.message().c_str(), ec.value(), (int)from.length(), from.data(), (int)from.length(), to.data() );

  return result;
}
void RenameFile(cStr from, cStr to) {
  std::error_code ec;
  fs::rename(from.c_str(), to.c_str(), ec);
  if (ec.value() != 0) esyslog("tvscraper: ERROR \"%s\", code %i  tried to rename \"%.*s\" to \"%.*s\"", ec.message().c_str(), ec.value(), (int)from.length(), from.data(), (int)to.length(), to.data() );
}

bool CopyFileImg(cStr from, cStr to) {
// include checks for images, which may be incomplete
// true if the file was copied, false otherwise.
  if (!FileExistsImg(from)) return false;
  if ( FileExistsImg(to)) return true;
  return CopyFile(from, to);
}

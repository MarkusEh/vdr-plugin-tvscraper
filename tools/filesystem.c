#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <filesystem>
#include <time.h>

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

//   if (buffer.st_size > 2500) return true;   // error pages are smaller than 2500 unfortunately not, found an 539408 error page ...
  if (buffer.st_size <  500) return false;  // images are larger than 500

// open and read file to make final decision
  cToSvFileN<15> fileStart(filename);
  if (!fileStart.exists() ) {
    esyslog("tvscraper, FileExistsImg, ERROR: stat OK, open fails, filename %s", filename.c_str() );
    return false;
  }
  if (cSv(fileStart).length() != 15) {
    esyslog("tvscraper, FileExistsImg, ERROR: num_read %i != 15, filename %s", (int)cSv(fileStart).length(), filename.c_str() );
    return false;
  }
  return strncmp(fileStart.c_str(), "<html>", 6) != 0 && strncmp(fileStart.c_str(), "<!DOCTYPE html>", 15) != 0;
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
  else dsyslog("tvscraper: deleted  \"%.*s\", %ju files", (int)dirname.length(), dirname.data(), n);
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
// return true on success (localPath exists & is an image file)
  if (FileExistsImg(localPath)) return true;
  std::string error;
  int err_code;
  for(int i=0; i < 11; i++) {
    if (i !=  0) sleep(2*i);
    if (i == 10) sleep(2*i); // extra long sleep before last try
    err_code = curl->GetUrlFile(url, localPath, &error);
    if (err_code == 0) {
      if (FileExistsImg(localPath) ) {
//      dsyslog("tvscraper: DEBUG successfully downloaded file, url: \"%s\" local path: \"%s\"", url.c_str(), localPath.c_str() );
        return true;
      }
      cToSvFile df(localPath);
      if (CheckDownloadAccessDenied(df)) {
        isyslog("tvscraper: download file failed, url: \"%s\" local path: \"%s\", AccessDenied, content \"%.*s\"", url.c_str(), localPath.c_str() , std::min(50, (int)cSv(df).length()), df.data() );
        remove(localPath);
        return false;
      }
    }
  }
  if (err_code == 0) {
    struct stat buffer;
    if (stat(localPath.c_str(), &buffer) != 0) buffer.st_size = 0;
    cToSvFileN<50> df(localPath);  // read max 50 bytes
    isyslog("tvscraper: ERROR download file, url: \"%s\" local path: \"%s\", file size: %zu, content \"%s\"", url.c_str(), localPath.c_str(), (size_t)buffer.st_size, df.c_str() );
  } else isyslog("tvscraper: ERROR download file, url: \"%s\" local path: \"%s\", error: \"%s\", err_code: %i", url.c_str(), localPath.c_str() , error.c_str(), err_code );
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

timespec modification_time(cSv filePath) {
// in: VDR rec. path (*.rec)
// out: modification time of last *.ts (or 00*.vdr) file

//     struct timespec {
//         time_t     tv_sec;   /* Seconds */
//         /* ... */  tv_nsec;  /* Nanoseconds [0, 999'999'999] */
//     };
  size_t folder_length = filePath.length();
  cToSvConcat file_path(filePath, "/00001.ts");
  struct stat buffer;
  struct stat l_stat;
  uint32_t num_ts_files;
  for (num_ts_files = 1; num_ts_files < 100000u; ++num_ts_files) {
    file_path.erase(folder_length+1);
    file_path.appendInt<5>(num_ts_files).append(".ts");
    if (stat(file_path.c_str(), &buffer) != 0) break;
    l_stat = buffer;
  }
  if (num_ts_files == 1) {
// no ts file found, check 00*.vdr files
    for (num_ts_files = 1; num_ts_files < 1000u; ++num_ts_files) {
      file_path.erase(folder_length+1);
      file_path.appendInt<3>(num_ts_files).append(".vdr");
      if (stat(file_path.c_str(), &buffer) != 0) break;
      l_stat = buffer;
    }
  }
  if (num_ts_files == 1) {
// nothing found. return 0
    l_stat.st_mtim.tv_sec = 0;
    l_stat.st_mtim.tv_nsec = 0;
  }
  return l_stat.st_mtim;
}
#include <fcntl.h>
statx_timestamp creation_time(cSv filePath) {
// in: VDR rec. path (*.rec)
// out: creation time of first *.ts (or 00*.vdr) file

//     struct statx_timestamp {
//         __s64 tv_sec;    /* Seconds since the Epoch (UNIX time) */
//         __u32 tv_nsec;   /* Nanoseconds since tv_sec */
//     };

  struct statx l_stat;
  if (statx(0, cToSvConcat(filePath, "/00001.ts").c_str(), 0, STATX_BTIME, &l_stat) == 0) {
// statx: On success, zero is returned.  On error, -1 is returned, and errno is set to indicate the error.
    return l_stat.stx_btime;
  }
// no ts file found, check 00*.vdr files
  if (statx(0, cToSvConcat(filePath, "/001.vdr").c_str(), 0, STATX_BTIME, &l_stat) == 0)
    return l_stat.stx_btime;
// nothing found. return 0
  l_stat.stx_btime.tv_sec = 0;
  l_stat.stx_btime.tv_nsec = 0;
  return l_stat.stx_btime;
}

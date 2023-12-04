#include <iostream>
#include <fstream>
#include <filesystem>
#include "../rapidjson/document.h"
#include "../rapidjson/prettywriter.h"
#include "../rapidjson/stringbuffer.h"
#include "../rapidjson/ostreamwrapper.h"
#include "../rapidjson/error/en.h"

#include <string>
#include "curlfuncs.h"
#include "stringhelpers.h"
#include "largeString.h"

namespace fs = std::filesystem;

inline
bool checkIsObject(const rapidjson::Value &json, const char *tag, const char *context, cLargeString *doc) {
  if (json.IsObject() ) return true;
  if (!context) return false;
  esyslog("tvscraper: ERROR, checkIsObject, not an object, tag %s, context %s, doc %s", tag?tag:"no tag", context, doc?doc->substr(0, 100).c_str():"no doc");
  return false;
}

inline
rapidjson::Value::ConstMemberIterator getTag(const rapidjson::Value &json, const char *tag, const char *context, cLargeString *doc) {
// like Value.FindMember: But: create syslog ERROR entry, if tag was not found && context != NULL
// context is used only for error message. Should be filename (if possible)
  rapidjson::Value::ConstMemberIterator res = json.FindMember(tag);
  if (context && res == json.MemberEnd() ) {
    esyslog("tvscraper: ERROR, getTag, tag %s not found, context %s, doc %s", tag, context, doc?doc->substr(0, 100).c_str():"no doc");
  }
  return res;
}

// getValue  ======================================================================
// simple form, overloads for "all" basic data types
// check if json has requested data type (data type of provided value variable)
// if yes, set value = json.Get...() and return true
// if no, don't change value, and return false
// getValue  ======================================================================

bool getValue(const rapidjson::Value &json, bool &value) {
  if (!json.IsBool() ) return false;
  value = json.GetBool();
  return true;
}

// int ===================================================
bool getValue(const rapidjson::Value &json, int &value) {
  if (!json.IsInt() ) return false;
  value = json.GetInt();
  return true;
}
bool getValue(const rapidjson::Value &json, long int &value) {
  if (!json.IsInt64() ) return false;
  value = json.GetInt64();
  return true;
}
bool getValue(const rapidjson::Value &json, long long int &value) {
  if (!json.IsInt64() ) return false;
  value = json.GetInt64();
  return true;
}

// unsigned int ==========================================
bool getValue(const rapidjson::Value &json, unsigned int &value) {
  if (!json.IsUint() ) return false;
  value = json.GetUint();
  return true;
}
bool getValue(const rapidjson::Value &json, unsigned long int &value) {
  if (!json.IsUint64() ) return false;
  value = json.GetUint64();
  return true;
}
bool getValue(const rapidjson::Value &json, unsigned long long int &value) {
  if (!json.IsUint64() ) return false;
  value = json.GetUint64();
  return true;
}

bool getValue(const rapidjson::Value &json, double &value) {
  if (!json.IsDouble() ) return false;
  value = json.GetDouble();
  return true;
}

bool getValue(const rapidjson::Value &json, const char *&value) {
  if (!json.IsString() ) return false;
  value = json.GetString();
  return true;
}

// getValue: ======================================================================
// return true if value is available, otherwise false. In this case, value will not change
// If false && context: write message to syslog
// getValue: ======================================================================

template<typename T>
bool getValue(const rapidjson::Value &json, const char *tag, T &value) {
// return false if tag does not exist, or is wrong data type
  if (!json.IsObject() ) return false;
  rapidjson::Value::ConstMemberIterator res = json.FindMember(tag);
  if (res == json.MemberEnd() ) return false;
  return getValue(res->value, value);
}
template<typename T>
bool getValue(const rapidjson::Value &json, const char *tag, T &value, const char *context, cLargeString *doc = nullptr) {
// return false if tag does not exist, or is wrong data type
  if (!checkIsObject(json, tag, context, doc)) return false;
  rapidjson::Value::ConstMemberIterator res = getTag(json, tag, context, doc);
  if (res == json.MemberEnd() ) return false; // error message was written by getTag, if required
  if (getValue(res->value, value) ) return true;
  esyslog("tvscraper: ERROR, getValue, tag %s wrong data type, context %s, doc %s", tag, context, doc?doc->substr(0, 100).c_str():"no doc");
  return false;
}

// special getBool, return -1 if tag does not exist, or is not a bool. =============
int getBool(const rapidjson::Value &json, const char *tag, bool *b=NULL, const char *context = NULL, cLargeString *doc = nullptr) {
// return -1 if tag does not exist, or is not a bool.
// 1 true
// 0 false
  bool result;
  if (!getValue(json, tag, result, context, doc)) return -1;
  if (b) *b = result;
  return result?1:0;
}

// getValue for char *  ==========================================================
const char *getValueCharS(const rapidjson::Value &json, const char *tag, const char *context = NULL, cLargeString *doc = nullptr) {
// return NULL if tag does not exist, or is not a string. If context is provided, create syslog ERROR entry
  if (!checkIsObject(json, tag, context, doc)) return NULL;
  rapidjson::Value::ConstMemberIterator res = getTag(json, tag, context, doc);
  if (res == json.MemberEnd() ) return NULL;
  if (res->value.IsString() ) return res->value.GetString();
  if (context) {
    esyslog("tvscraper: ERROR, getValueCharS, tag %s not a string, context %s, doc %s", tag, context, doc?doc->substr(0, 100).c_str():"no doc");
  }
  return NULL;
}

const char *getValueCharS2(const rapidjson::Value &json, const char *tag1, const char *tag2, const char *context = NULL, cLargeString *doc = nullptr) {
// return NULL if tag does not exist, or is not a string. If context is provided, create syslog ERROR entry
  if (!checkIsObject(json, tag1, context, doc)) return NULL;
  rapidjson::Value::ConstMemberIterator res = getTag(json, tag1, context, doc);
  if (res == json.MemberEnd() ) return NULL;
  if (res->value.IsObject() ) return getValueCharS(res->value, tag2, context, doc);
  if (context) {
    esyslog("tvscraper: ERROR, getValueCharS2, tag1: %s, tag2 %s not an object, context %s, doc %s", tag1, tag2, context, doc?doc->substr(0, 100).c_str():"no doc");
  }
  return NULL;
}

// getValue for String  ==========================================================
std::string getValueString(const rapidjson::Value &json, const char *tag, const char *returnOnError = NULL, const char *context = NULL, cLargeString *doc = nullptr) {
// return NULL if tag does not exist, or is not a string. If context is provided, create syslog ERROR entry
  if (!checkIsObject(json, tag, context, doc)) return charPointerToString(returnOnError);
  rapidjson::Value::ConstMemberIterator res = getTag(json, tag, context, doc);
  if (res == json.MemberEnd() ) return charPointerToString(returnOnError);
  if (res->value.IsString() ) return charPointerToString(res->value.GetString());
  if (context) {
    esyslog("tvscraper: ERROR, getValueString, tag %s not a string, context %s, doc %s", tag, context, doc?doc->substr(0, 100).c_str():"no doc");
  }
  return charPointerToString(returnOnError);
}

// getValue for int     ==========================================================
int getValueInt(const rapidjson::Value &json, const char *tag, int returnOnError = 0, const char *context = NULL, cLargeString *doc = nullptr) {
// return returnOnError if tag does not exist, or is not int. If context is provided, create syslog ERROR entry
  if (!checkIsObject(json, tag, context, doc)) return returnOnError;
  rapidjson::Value::ConstMemberIterator res = getTag(json, tag, context, doc);
  if (res == json.MemberEnd() ) return returnOnError;
  if (res->value.IsInt() ) return res->value.GetInt();
  if (context) {
    esyslog("tvscraper: ERROR, getValueInt, tag %s not int, context %s, doc %s", tag, context, doc?doc->substr(0, 100).c_str():"no doc");
  }
  return returnOnError;
}

// getValue for bool    ==========================================================
bool getValueBool(const rapidjson::Value &json, const char *tag, bool returnOnError = false, const char *context = NULL, cLargeString *doc = nullptr) {
// return returnOnError if tag does not exist, or is not bool. If context is provided, create syslog ERROR entry
  if (!checkIsObject(json, tag, context, doc)) return returnOnError;
  rapidjson::Value::ConstMemberIterator res = getTag(json, tag, context, doc);
  if (res == json.MemberEnd() ) return returnOnError;
  if (res->value.IsBool() ) return res->value.GetBool();
  if (context) {
    esyslog("tvscraper: ERROR, getValueBool, tag %s not bool, context %s, doc %s", tag, context, doc?doc->substr(0, 100).c_str():"no doc");
  }
  return returnOnError;
}

// getValue for double  ==========================================================
double getValueDouble(const rapidjson::Value &json, const char *tag, double returnOnError = 0., const char *context = NULL, cLargeString *doc = nullptr) {
// return returnOnError if tag does not exist, or is not double. If context is provided, create syslog ERROR entry
  if (!checkIsObject(json, tag, context, doc)) return returnOnError;
  rapidjson::Value::ConstMemberIterator res = getTag(json, tag, context, doc);
  if (res == json.MemberEnd() ) return returnOnError;
  if (res->value.IsDouble() ) return res->value.GetDouble();
  if (context) {
    esyslog("tvscraper: ERROR, getValueInt, tag %s not double, context %s, doc %s", tag, context, doc?doc->substr(0, 100).c_str():"no doc");
  }
  return returnOnError;
}

class cJsonArrayIterator {
  public:
    cJsonArrayIterator(const rapidjson::Value &json, const char *tag) {
      init(json, tag);
    }
    rapidjson::Value::ConstValueIterator begin() { return m_array?m_array->Begin():&m_dummy; }
    rapidjson::Value::ConstValueIterator end() { return m_array?m_array->End():&m_dummy; }
  private:
    void init(const rapidjson::Value &json, const char *tag) {
      m_array = nullptr;
      if (!json.IsObject() ) return;
      rapidjson::Value::ConstMemberIterator res = json.FindMember(tag);
      if (res == json.MemberEnd() || !res->value.IsArray() ) return;
      m_array = &res->value;
    }
    const rapidjson::Value* m_array;
    const rapidjson::Value m_dummy;
};

std::string getValueArrayConcatenated(const rapidjson::Value &json, const char *arrayName, const char *attributeName) {
// concatenate all values of attribute attributeName in array arrayName
  cContainer result;
  for (const rapidjson::Value &elem: cJsonArrayIterator(json, arrayName)) {
    result.insert(getValueCharS(elem, attributeName));
  }
  return result.moveBuffer();
}
std::string getArrayConcatenated(const rapidjson::Value &json, const char *arrayName) {
// concatenate all strings in array arrayName (use this if there is an array with strings ...)
  cContainer result;
  for (const rapidjson::Value &elem: cJsonArrayIterator(json, arrayName)) {
    if (elem.IsString()) result.insert(elem.GetString() );
  }
  return result.moveBuffer();
}
bool jsonCallRest(rapidjson::Document &document, cLargeString &buffer, const char *url, bool debug, struct curl_slist *headers) {
// true on success
// download json file
  buffer.clear();
  headers = curl_slistAppend(headers, "Accept: application/json");
  if (debug) esyslog("tvscraper: calling %s, buffer %.*s", url, buffer.nameLen(), buffer.nameData() );
  if (!CurlGetUrl(url, buffer, headers) ) {
    esyslog("tvscraper: ERROR jsonCallRest, download %s failed, buffer %.*s", url, buffer.nameLen(), buffer.nameData() );
    return false; // no data
  }
  document.ParseInsitu(buffer.data() );
  if (document.HasParseError() ) {
    esyslog("tvscraper: ERROR jsonCallRest, url %s, parse %s failed, parse error %s buffer %.*s", url, buffer.substr(0, 100).c_str(), rapidjson::GetParseError_En(document.GetParseError()), buffer.nameLen(), buffer.nameData() );
    return false; // no data
  }
  return true;
}
template<typename... Args>
bool jsonCallRest(rapidjson::Document &document, cLargeString &buffer, const char *url, bool debug, const Args... args) {
  struct curl_slist *headers = NULL;
  headers = curl_slistAppend(headers, args...);
  bool result = jsonCallRest(document, buffer, url, debug, headers);
  if (headers) curl_slist_free_all(headers);
  return result;
}

class cJsonDocumentFromFile: public rapidjson::Document {
// if file does not exist: an empty (valid!) document
// if file does     exist: read the file and return document with parsed content
// if file does     exist, but is not valid json: error in syslog (you can check with document.HasParseError()
// if file does     exist, but is not valid json and backup_invalid_file == true: rename the invalid file to .bak, and return an empty (valid) document. Write Error message to syslog
  public:
    cJsonDocumentFromFile(const char *filename, bool backup_invalid_file = false): m_jfile(filename) {
      if (cSv(m_jfile).empty() ) {
        SetObject();
        return;
      }
      ParseInsitu(m_jfile.data() );
      if (HasParseError() ) {
        esyslog("tvscraper: ERROR cJsonDocumentFromFile, file %s size %zu parse error %s, doc %s", filename, cSv(m_jfile).length(), rapidjson::GetParseError_En(GetParseError()), zeroToPercent(cSv(m_jfile).substr(0, 100)).c_str() );
        if (backup_invalid_file) {
          std::error_code ec;
          fs::rename(filename, concat(filename, ".bak"), ec);
          if (ec.value() != 0) esyslog("tvscraper: ERROR \"%s\", code %i  tried to rename \"%s\" to \"%s\"", ec.message().c_str(), ec.value(), filename, concat(filename, ".bak").c_str());

          SetObject();
        }
      }
    }
  private:
    cToSvFile m_jfile;
};

int jsonWriteFile(rapidjson::Document &document, const char *filename) {
// return 1, if it is not possible to write file
// return 0, in case of no error
  std::ofstream ofs(filename);
  if (!ofs.is_open() ) {
    esyslog("tvscraper: ERROR jsonWriteFile, could not open file %s for writing", filename);
    return 1;
  }

  rapidjson::OStreamWrapper osw(ofs);
  rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
  writer.SetIndent(' ', 2);
  document.Accept(writer);
  return 0;
}

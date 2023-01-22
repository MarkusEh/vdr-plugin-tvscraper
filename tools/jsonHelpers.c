#include <iostream>
#include <fstream>
#include <filesystem>
#include "../rapidjson/document.h"
//#include "../rapidjson/istreamwrapper.h"
#include "../rapidjson/prettywriter.h"
#include "../rapidjson/stringbuffer.h"
#include "../rapidjson/ostreamwrapper.h"
#include "../rapidjson/error/en.h"

#include <jansson.h>
#include <string>

rapidjson::Value::ConstMemberIterator assertTagExists(const rapidjson::Value &json, const char *tag, const char *context) {
// like Value.FindMember: But: create syslog entry, if tag was not found
// context is used only for error message. Should be filename (if possible)
  rapidjson::Value::ConstMemberIterator res = json.FindMember(tag);
  if (res == json.MemberEnd() )
    esyslog("tvscraper: ERROR, assertTagExists, tag %s not found, context %s", tag, context?context:"no context available");
  return res;
}

bool assertGetValue(const rapidjson::Value &json, const char *tag, int64_t &value, const char *context) {
// return false if tag does not exist, or is not a int64_t
// context is used only for error message. Should be filename (if possible)
  rapidjson::Value::ConstMemberIterator res = assertTagExists(json, tag, context);
  if (res == json.MemberEnd() ) return false;
  if (res->value.IsInt64() ) {
    value = res->value.GetInt64();
    return true;
  }
  esyslog("tvscraper: ERROR, assertGetValue, tag %s not int64_t, context %s", tag, context?context:"no context available");
  return false;
}

bool assertGetValue(const rapidjson::Value &json, const char *tag, bool &value, const char *context) {
// return false if tag does not exist, or is not a bool
// context is used only for error message. Should be filename (if possible)
  rapidjson::Value::ConstMemberIterator res = assertTagExists(json, tag, context);
  if (res == json.MemberEnd() ) return false;
  if (res->value.IsBool() ) {
    value = res->value.GetBool();
    return true;
  }
  esyslog("tvscraper: ERROR, assertGetValue, tag %s not bool, context %s", tag, context?context:"no context available");
  return false;
}

bool getValue(const rapidjson::Value &json, const char *tag, bool &value) {
// return false if tag does not exist, or is not a bool
  rapidjson::Value::ConstMemberIterator res = json.FindMember(tag);
  if (res == json.MemberEnd() || !res->value.IsBool() ) return false;
  value = res->value.GetBool();
  return true;
}

bool getValue(const rapidjson::Value &json, const char *tag, int &value) {
// return false if tag does not exist, or is not an int
  rapidjson::Value::ConstMemberIterator res = json.FindMember(tag);
  if (res == json.MemberEnd() || !res->value.IsInt() ) return false;
  value = res->value.GetInt();
  return true;
}

bool getValue(const rapidjson::Value &json, const char *tag, int64_t &value) {
// return false if tag does not exist, or is not an int64
  rapidjson::Value::ConstMemberIterator res = json.FindMember(tag);
  if (res == json.MemberEnd() || !res->value.IsInt64() ) return false;
  value = res->value.GetInt64();
  return true;
}

bool getValue(const rapidjson::Value &json, const char *tag, const char *&value) {
// return false if tag does not exist, or is not a string
  rapidjson::Value::ConstMemberIterator res = json.FindMember(tag);
  if (res == json.MemberEnd() || !res->value.IsString() ) return false;
  value = res->value.GetString();
  return true;
}

int getBool(const rapidjson::Value &json, const char *tag, bool *b=NULL) {
// return -1 if tag does not exist, or is not a bool. No errors in syslog
// 1 true
// 0 false
  rapidjson::Value::ConstMemberIterator res = json.FindMember(tag);
  if (res == json.MemberEnd() ) return -1;
  if (!res->value.IsBool() ) return -1;
  if (b) { *b = res->value.GetBool(); return *b?1:0; }
  return res->value.GetBool()?1:0;
}

cLargeString jsonReadFile(rapidjson::Document &document, const char *filename) {
// if file does not exist: return 0, and an empty (valid!) document
// if file does     exist: return 0, read the file and return document with parsed content
// if file does     exist, but is not valid json: return 1 (error).

  cLargeString jfile(filename);
  if (jfile.empty() ) {
    document.SetObject();
    return jfile;
  }
  document.ParseInsitu(jfile.data() );
  if (document.HasParseError() ) {
    esyslog("tvscraper: ERROR jsonReadFile, file %s size %zu parse error %s", filename, jfile.length(), rapidjson::GetParseError_En(document.GetParseError()) );
  }
  return jfile;
}

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

json_t *json_object_validated(json_t *j, const char *attributeName, const char *error = NULL){
  json_t *jAttribute = json_object_get(j, attributeName);
  if (json_is_object(jAttribute)) return jAttribute;
  if (error && *error) esyslog("tvscraper: ERROR, %s is not an object, context %s", attributeName, error);
  return NULL;
}

json_t *json_array_validated(json_t *j, const char *attributeName, const char *error = NULL){
  json_t *jAttribute = json_object_get(j, attributeName);
  if (json_is_array(jAttribute)) return jAttribute;
  if (error && *error) esyslog("tvscraper: ERROR, %s is not an array, context %s", attributeName, error);
  return NULL;
}

const char *json_string_value_validated_c(const json_t *j, const char *attributeName) {
  json_t *jAttribute = json_object_get(j, attributeName);
  if(json_is_string(jAttribute)) return json_string_value(jAttribute);
  return NULL;
}

std::string json_string_value_validated(const json_t *j, const char *attributeName){
  json_t *jAttribute = json_object_get(j, attributeName);
  if(json_is_string(jAttribute)) return json_string_value(jAttribute);
  return "";
}

int json_integer_value_validated(json_t *j, const char *attributeName){
  json_t *jAttribute = json_object_get(j, attributeName);
  if(json_is_integer(jAttribute)) return json_integer_value(jAttribute);
  return 0;
}

float json_number_value_validated(json_t *j, const char *attributeName){
  json_t *jAttribute = json_object_get(j, attributeName);
  if(json_is_number(jAttribute)) return json_number_value(jAttribute);
  return 0.0;
}

std::string json_concatenate_array(json_t *j, const char *arrayName, const char *attributeName){
  std::string result;
  json_t *jArray = json_object_get(j, arrayName);
  if(json_is_array(jArray)) {
    size_t numElements = json_array_size(jArray);
    if (numElements == 0) return "";
    result = "|";
    for (size_t i = 0; i < numElements; i++) {
      json_t *jElement = json_array_get(jArray, i);
      json_t *jElementName = json_object_get(jElement, attributeName);
      if(json_is_string(jElementName)) {
        result.append(json_string_value(jElementName));
        result.append("|");
      }
    }
  }
  return result;
}

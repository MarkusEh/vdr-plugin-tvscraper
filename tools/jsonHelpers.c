#include <jansson.h>
#include <string>

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

string json_string_value_validated(const json_t *j, const char *attributeName){
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

string json_concatenate_array(json_t *j, const char *arrayName, const char *attributeName){
  string result;
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

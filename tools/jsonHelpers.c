#include <jansson.h>
#include <string>

string json_string_value_validated(json_t *j, const char *attributeName){
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
      for (size_t i = 0; i < numElements; i++) {
        json_t *jElement = json_array_get(jArray, i);
        json_t *jElementName = json_object_get(jElement, attributeName);
        if(json_is_string(jElementName)) {
          if(result.empty() ) result = json_string_value(jElementName);
            else { result.append("; "); result.append(json_string_value(jElementName)); }
          }
      }
    }
  return result;
}

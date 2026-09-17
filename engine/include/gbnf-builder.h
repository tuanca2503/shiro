#pragma once

#include <string>
#include <vector>

std::string buildGbnfBoolean();
std::string buildGbnfPlainEnum(const std::vector<std::string> &values);
std::string buildGbnfJsonSchema(const std::string &schema_json);
std::string buildGbnfToolCall(const std::string &arguments_schema, const std::string &trigger);


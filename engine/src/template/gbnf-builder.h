#pragma once

#include <string>
#include <vector>

namespace shiro
{
    std::string buildGbnfBoolean();
    std::string buildGbnfPlainEnum(const std::vector<std::string> &values);
    std::string buildGbnfJsonSchema(const std::string &json);
    std::string buildGbnfToolCall(const std::string &json, const std::string &trigger);
}
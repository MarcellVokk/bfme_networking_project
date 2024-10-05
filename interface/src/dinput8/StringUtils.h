#pragma once

#include <string>

class StringUtils
{
public:
    static std::string ReplaceAll(std::string str, const std::string& from, const std::string& to);
};
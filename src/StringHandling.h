#pragma once
#include <utility>
#include <string>
#include <vector>
namespace StringHandling
{

std::pair<std::string, std::string> SplitTwoParts(const std::string& message, const std::string& delimiter=" ");

std::vector<std::pair<std::string, std::string>> ReadParams(const std::string& message);

std::vector<std::string> SplitBySpace(const std::string& message);

} // namespace StringHandling

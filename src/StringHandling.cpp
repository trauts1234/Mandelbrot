#include "StringHandling.h"

#include "threadsafe_io.h"

#include <sstream>
#include <cassert>

std::pair<std::string, std::string> StringHandling::SplitTwoParts(const std::string& message, const std::string& delimiter)
{
    size_t index = message.find(delimiter);

    if(index == std::string::npos){
        return {message, ""};
    }

    std::string first_part = message.substr(0, index);
    std::string second_part = message.substr(index + delimiter.length());

    return {first_part, second_part};
}

std::vector<std::pair<std::string, std::string>> StringHandling::ReadParams(const std::string& message)
{
    std::vector<std::string> parts = SplitBySpace(message);
    assert(parts.size() % 2 == 0);
    std::vector<std::pair<std::string, std::string>> ans;

    for(size_t i=0;i<parts.size();i+=2){
        ans.push_back({parts[i], parts[i+1]});
    }
    
    return ans;
}

std::vector<std::string> StringHandling::SplitBySpace(const std::string& message)
{
    std::string item;

    std::vector<std::string> row;
    std::stringstream ss(message);

    while (std::getline(ss, item, ' ')) {
        row.push_back(item);
    }

    return row;
}

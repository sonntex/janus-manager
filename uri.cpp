#include "uri.hpp"

#include <regex>

uri make_uri(const std::string& value)
{
    std::regex expr("(/?[^ #?]*)\\x3f?([^ ?#]*)\\x23?([^ ?#]*)");
    std::smatch what;
    if (!std::regex_match(value, what, expr))
        throw std::out_of_range("bad uri");
    return {{what[1].first, what[1].second},
            {what[2].first, what[2].second},
            {what[3].first, what[3].second}};
}

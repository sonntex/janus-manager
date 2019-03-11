#include "uri.hpp"

#include <regex>

uri make_uri(const std::string& s)
{
    uri res;
    std::regex expr("(/?[^ #?]*)\\x3f?([^ #]*)\\x23?([^ ]*)");
    auto pos = std::sregex_iterator(s.begin(), s.end(), expr);
    auto end = std::sregex_iterator();
    if (pos != end) {
        const auto& match = *pos;
        res.path = match[1];
        res.query = match[2];
        res.fragment = match[3];
    }
    return res;
}

uri_path make_path(const std::string& s)
{
    return boost::filesystem::path(s);
}

uri_query make_query(const std::string& s)
{
    uri_query res;
    std::regex expr("([^=]*)=([^&]*)&?");
    auto pos = std::sregex_iterator(s.begin(), s.end(), expr);
    auto end = std::sregex_iterator();
    for ( ; pos != end; ++pos) {
        const auto& match = *pos;
        res.insert(std::make_pair(match[1], match[2]));
    }
    return res;
}

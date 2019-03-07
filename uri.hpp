#pragma once

#include <boost/filesystem/path.hpp>
#include <regex>

struct uri
{
    uri(boost::filesystem::path path, std::string query, std::string fragment);
    boost::filesystem::path path;
    std::string query;
    std::string fragment;
};

template <typename Iterator>
uri make_uri(Iterator first, Iterator last)
{
    std::regex expr("(/?[^ #?]*)\\x3f?([^ ?#]*)\\x23?([^ ?#]*)");
    std::match_results<Iterator> what;
    if (!std::regex_match(first, last, what, expr))
        throw std::out_of_range("bad uri");
    return uri(
        {what[1].first, what[1].second},
        {what[2].first, what[2].second},
        {what[3].first, what[3].second});
}

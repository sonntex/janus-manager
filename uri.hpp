#pragma once

#include <boost/filesystem.hpp>
#include <map>

struct uri
{
    std::string path;
    std::string query;
    std::string fragment;
};

using uri_path = boost::filesystem::path;
using uri_query = std::map<std::string, std::string>;

uri make_uri(const std::string& s);
uri_path make_path(const std::string& s);
uri_query make_query(const std::string& s);

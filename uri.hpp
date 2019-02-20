#pragma once

#include <boost/filesystem/path.hpp>

struct uri
{
    boost::filesystem::path path;
    std::string query;
    std::string fragment;
};

uri make_uri(const std::string& value);

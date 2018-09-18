#pragma once

#include "definitions.hpp"

#include <map>

struct stream_info
{
    std::uint64_t id = 0;
    std::uint16_t port = 0;
    std::chrono::system_clock::time_point expires_at;
};

using stream_info_map = std::map<std::uint64_t, stream_info>;

stream_info make_stream(const stream_info_map& streams,
    std::uint16_t min_port, std::uint16_t max_port, std::uint16_t& port);

void keep_alive(stream_info& stream);

bool expired(const stream_info& stream, std::chrono::system_clock::time_point now);

stream_info_map expired(stream_info_map& streams);

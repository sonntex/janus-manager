#include "stream.hpp"

static bool has_id(const stream_info_map& streams, std::uint32_t id)
{
    return streams.count(id);
}

static std::uint32_t gen_id(const stream_info_map& streams)
{
    for (std::uint32_t res = std::rand(); ; res = std::rand()) {
        res |= 0x80000000;
        if (res && !has_id(streams, res))
            return res;
    }
    throw std::out_of_range("no more identificators!");
}

static bool has_port(const stream_info_map& streams, std::uint16_t port)
{
    for (auto it = streams.begin(); it != streams.end(); ++it) {
        auto& stream = it->second;
        if (stream.port == port)
            return true;
    }
    return false;
}

static std::uint16_t gen_port(const stream_info_map& streams,
    std::uint16_t min_port, std::uint16_t max_port, std::uint16_t& port)
{
    if ((max_port + 1 - min_port) % 4 ||
        (max_port + 1 - port) % 4)
        throw std::out_of_range("invalid range of ports");
    for (std::uint32_t res = port; res < max_port; res += 4) {
        if (res && !has_port(streams, res)) {
            port += 4;
            if (port > max_port)
                port = min_port;
            return res;
        }
    }
    for (std::uint32_t res = min_port; res < port; res += 4) {
        if (res && !has_port(streams, res)) {
            port += 4;
            if (port > max_port)
                port = min_port;
            return res;
        }
    }
    throw std::out_of_range("no more ports!");
}

stream_info make_stream(const stream_info_map& streams,
    std::uint16_t min_port, std::uint16_t max_port, std::uint16_t& port)
{
    stream_info res;
    res.id = gen_id(streams);
    res.port = gen_port(streams, min_port, max_port, port);
    return res;
}

void keep_alive(stream_info& stream, std::chrono::system_clock::time_point expires_at)
{
    stream.expires_at = expires_at;
}

stream_info_map expired(stream_info_map& streams)
{
    auto now = std::chrono::system_clock::now();
    stream_info_map expires;
    for (auto it = streams.begin(); it != streams.end(); ) {
        auto& stream = it->second;
        if (stream.expires_at <= now) {
            expires[stream.id] = std::move(stream);
            it = streams.erase(it);
        } else ++it;
    }
    return expires;
}

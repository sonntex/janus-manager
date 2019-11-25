#include "hash.hpp"

#include <boost/uuid/detail/md5.hpp>

std::string gen_uid()
{
    // FIXME:
    return std::to_string(std::rand());
}

std::string md5()
{
    return md5(gen_uid());
}

std::string md5(const std::string& buf)
{
    static const char map[] = "0123456789abcdef";
    boost::uuids::detail::md5 hash;
    boost::uuids::detail::md5::digest_type digest;
    hash.process_bytes(buf.c_str(), buf.size());
    hash.get_digest(digest);
    std::string res;
    for (std::size_t i = 0; i < 4; ++i) {
        res.push_back(map[(digest[i] >> 28) & 0x0f]);
        res.push_back(map[(digest[i] >> 24) & 0x0f]);
        res.push_back(map[(digest[i] >> 20) & 0x0f]);
        res.push_back(map[(digest[i] >> 16) & 0x0f]);
        res.push_back(map[(digest[i] >> 12) & 0x0f]);
        res.push_back(map[(digest[i] >> 8) & 0x0f]);
        res.push_back(map[(digest[i] >> 4) & 0x0f]);
        res.push_back(map[(digest[i]) & 0x0f]);
    }
    return res;
}

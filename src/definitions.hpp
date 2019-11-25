#pragma once

#include <boost/log/trivial.hpp>
#include <chrono>
#include <functional>

static const auto default_timeout = std::chrono::seconds(1);
static const auto timeout_keepalive = std::chrono::seconds(30);
static const auto timeout_loop = std::chrono::seconds(1);
static const auto timeout_retries = std::chrono::milliseconds(100);

static const std::size_t retries = 256;

class guard
{
public:
    using handler_type = std::function<void()>;
    guard() {}
    guard(handler_type h) { h_ = h; }
    guard(const guard& other) = delete;
    guard(guard&& other) { h_ = other.h_; other.h_ = nullptr; }
   ~guard() { if (h_) h_(); }
    guard& operator=(handler_type h) { h_ = h; return *this; }
    guard& operator=(const guard& other) = delete;
    guard& operator=(guard&& other) { h_ = other.h_; other.h_ = nullptr; return *this; }
private:
    handler_type h_;
};

template <typename F> guard make_guard(F h) { return guard(h); }
template <typename F> std::unique_ptr<guard> make_unique_guard(F h) { return std::make_unique<guard>(h); }
template <typename F> std::shared_ptr<guard> make_shared_guard(F h) { return std::make_shared<guard>(h); }

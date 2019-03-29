#pragma once

#include <boost/log/trivial.hpp>
#include <chrono>

static const auto default_timeout = std::chrono::seconds(1);
static const auto timeout_keepalive = std::chrono::seconds(30);
static const auto timeout_loop = std::chrono::seconds(1);
static const auto timeout_retries = std::chrono::milliseconds(100);

static const std::size_t retries = 256;

template <typename F>
class guard
{
public:
    guard(F f) : f(f) {}
    guard(const guard&) = delete;
    guard& operator=(const guard&) = delete;
    guard(guard&& other) : f(std::move(other.f)) {}
    guard& operator=(guard&& other) { f = std::move(other.f); }
   ~guard() { f(); }
private:
    F f;
};

template <typename F> guard<F> make_guard(F f)
{ return guard<F>(f); }

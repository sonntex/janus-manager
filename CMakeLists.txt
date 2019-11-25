cmake_minimum_required(VERSION 2.8.11)

project(janus-manager)

set(FLAGS_RELEASE "-O3")
set(FLAGS_DEBUG "-O0 -g -ggdb")
set(FLAGS "-pthread")
set(CMAKE_C_FLAGS_RELEASE "${FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_DEBUG "${FLAGS_DEBUG}")
set(CMAKE_C_FLAGS "${FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_DEBUG "${FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS "${FLAGS} -std=c++14 -DBOOST_ALL_DYN_LINK")

include_directories(contrib/json/single_include)

add_executable(janus-manager
    src/definitions.hpp
    src/hash.hpp
    src/hash.cpp
    src/http.hpp
    src/http.cpp
    src/stream.hpp
    src/stream.cpp
    src/uri.hpp
    src/uri.cpp
    src/main.cpp)

target_link_libraries(janus-manager boost_system boost_thread boost_filesystem boost_log boost_log_setup)

install(TARGETS janus-manager DESTINATION /usr/bin)
install(FILES share/janus-manager.service DESTINATION /usr/lib/systemd/system)
install(FILES share/janus-manager.env DESTINATION /etc/sysconfig)

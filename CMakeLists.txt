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

add_executable(janus-manager
    json.hpp
    definitions.hpp
    hash.hpp
    hash.cpp
    http.hpp
    http.cpp
    stream.hpp
    stream.cpp
    uri.hpp
    uri.cpp
    main.cpp)

target_link_libraries(janus-manager boost_system boost_filesystem boost_log)

install(TARGETS janus-manager DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(FILES janus-manager.service DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)

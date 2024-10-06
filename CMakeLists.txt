cmake_minimum_required(VERSION 3.16)

project(uvhttp_sample_cpp 
    LANGUAGES CXX)

add_executable(http_srv)
target_sources(http_srv PRIVATE  main.cpp)

set_property(TARGET http_srv PROPERTY CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libuv REQUIRED libuv)

target_link_libraries(http_srv PRIVATE ${libuv_LIBRARIES} http_parser)


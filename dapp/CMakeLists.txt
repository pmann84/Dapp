cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 17)

add_library(dapp INTERFACE)

target_include_directories(dapp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_sources(dapp INTERFACE "dapp.hpp")
target_link_libraries(dapp INTERFACE CONAN_PKG::sqlite3)
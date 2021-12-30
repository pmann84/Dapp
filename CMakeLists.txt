#### cmake -G "Visual Studio 16 2019" -A x64 ..
cmake_minimum_required(VERSION 3.19)

# Project settings
project(
        dapp
        VERSION 0.0.1
        DESCRIPTION ""
        LANGUAGES CXX
)

# CMake C++ standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Who doesnt like to know the compilation commands...
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Disable the ZERO_CHECK project generation
set(CMAKE_SUPPRESS_REGENERATION true)

# CMake scripts
include(cmake/conan.cmake)

# Set up Conan dependencies
set(CONAN_EXTRA_OPTIONS "")
set(CONAN_EXTRA_REQUIRES ${CONAN_EXTRA_REQUIRES}
    sqlite3/3.36.0
)

run_conan()

# Since googletest uses these flags we should too.
#SET(CMAKE_CXX_FLAGS_DEBUG "/MTd")
#SET(CMAKE_CXX_FLAGS_RELEASE "/MT")

add_subdirectory(dapp)
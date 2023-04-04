cmake_minimum_required(VERSION 3.10)

project(HighNetwork VERSION 1.0.0)

option(BUILD_EXAMPLE "build-examples" ON)

option(BUILD_UNITTEST "build-unittest" ON)

# 设置C++ 20
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)


if(BUILD_EXAMPLE)
    add_subdirectory(example)
endif()


if(BUILD_UNITTEST)
    add_subdirectory(unittest)
endif()


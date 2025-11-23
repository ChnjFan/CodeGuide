cmake_minimum_required(VERSION 4.0)
project(source)

set(CMAKE_CXX_STANDARD 20)

add_executable(source main.cpp
        file.h
        thread.h
)

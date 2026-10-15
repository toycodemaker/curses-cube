cmake_minimum_required(VERSION 3.21)

project(curses_cube LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED on)

add_executable(r ./src/main.cpp)
target_include_directories(r PRIVATE ./include)

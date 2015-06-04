cmake_minimum_required(VERSION 3.2)
project(dtw)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp)
set(CHECKER check.h check.cpp)
add_executable(dtw ${SOURCE_FILES} ${CHECKER} dtw.cpp dtw.h Vector.cpp Vector.h Point.cpp Point.h)
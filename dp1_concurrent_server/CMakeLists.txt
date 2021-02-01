cmake_minimum_required(VERSION 3.13)
project(DP1serverconcorrentedef C)

set(CMAKE_C_STANDARD 99)

add_executable(DP1serverconcorrentedef main.c cmake-build-debug/protocol.h cmake-build-debug/protocol.c protocol.h protocol.c)
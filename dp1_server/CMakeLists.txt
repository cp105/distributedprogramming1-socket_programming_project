cmake_minimum_required(VERSION 3.13)
project(DP1serverdef C)

set(CMAKE_C_STANDARD 99)

add_executable(DP1serverdef main.c protocol.c protocol.h)
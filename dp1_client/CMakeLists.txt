cmake_minimum_required(VERSION 3.13)
project(DP1clientdef C)

set(CMAKE_C_STANDARD 99)

add_executable(DP1clientdef main.c protocol.c protocol.h)
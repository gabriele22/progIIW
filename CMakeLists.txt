cmake_minimum_required(VERSION 3.6)
project(progettoIIW)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES server.c utils.c utils.h basic.h http_image.c http_image.h)
add_executable(progettoIIW ${SOURCE_FILES})
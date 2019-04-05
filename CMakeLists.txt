cmake_minimum_required(VERSION 3.10)
project(electrum)

include(CTest)

set(CMAKE_CXX_STANDARD 17)

include_directories(src/lib)
include_directories(src)
include_directories(src/include)

add_subdirectory(src)
add_subdirectory(test)

#add_executable(electrum main.cpp src/lexer/LexerDefs.h)
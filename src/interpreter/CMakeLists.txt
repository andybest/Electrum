project(${CMAKE_PROJECT_NAME}_interpreter)

set(CMAKE_CXX_STANDARD 17)

set(HEADER_FILES
        Interpreter.h)

set(SOURCE_FILES
        Interpreter.cpp
        InterpreterExceptions.h)

add_library(${CMAKE_PROJECT_NAME}_interpreter
        STATIC
        ${HEADER_FILES}
        ${SOURCE_FILES})

target_include_directories(${CMAKE_PROJECT_NAME}_interpreter
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${CMAKE_PROJECT_NAME}_interpreter ${CMAKE_PROJECT_NAME}_runtime)
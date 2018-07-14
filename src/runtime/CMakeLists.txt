project(${CMAKE_PROJECT_NAME}_runtime)

set(CMAKE_CXX_STANDARD 17)

set(HEADER_FILES
        Runtime.h
        GarbageCollector.h
        stackmap/api.h
        ENamespace.h
        )

set(SOURCE_FILES
        Runtime.cpp
        GarbageCollector.cpp
        generate.c
        hash_table.c
        ENamespace.cpp)

add_library(${CMAKE_PROJECT_NAME}_runtime STATIC
        ${HEADER_FILES}
        ${SOURCE_FILES})

target_include_directories(${CMAKE_PROJECT_NAME}_runtime
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})
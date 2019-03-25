project(${CMAKE_PROJECT_NAME}_runtime)

set(CMAKE_CXX_STANDARD 17)

#set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

set(HEADER_FILES
        Runtime.h
        GarbageCollector.h
        stackmap/api.h
        ENamespace.h
        )

set(SOURCE_FILES
        Runtime.cpp
        apply.cpp
        GarbageCollector.cpp
        generate.c
        hash_table.c
        ENamespace.cpp EHashTable.c EHashTable.h Dwarf_eh.cpp Dwarf_eh.h)

add_library(${CMAKE_PROJECT_NAME}_runtime SHARED
        ${HEADER_FILES}
        ${SOURCE_FILES})

target_include_directories(${CMAKE_PROJECT_NAME}_runtime
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})
project(electrum_repl)

set(CMAKE_CXX_STANDARD 17)

set(HEADER_FILES
        )

set(SOURCE_FILES
        main.cpp)

add_executable(electrum_repl ${HEADER_FILES} ${SOURCE_FILES})

target_include_directories(electrum_repl PUBLIC ../lib/linenoise-ng/include)
target_link_libraries(electrum_repl linenoise)

target_link_libraries(electrum_repl electrumc_lib)
target_link_libraries(electrum_repl electrum_runtime)

# Add location of Homebrew'd LLVM if on MacOS
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/llvm)
endif()

# LLVM
find_package(LLVM 8 REQUIRED CONFIG )

include_directories(electrum_repl PUBLIC ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

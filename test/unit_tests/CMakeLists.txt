include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

# TODO: Fix compiler lib so LLVM headers aren't required
# LLVM
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")


include_directories(${LLVM_INCLUDE_DIRS})

include(GoogleTest)

add_executable(Unit_Tests_run
        LexerHelpers.h
        test_lexer.cpp
        test_parser.cpp
        test_analyzer.cpp
        test_interpreter.cpp
        test_compiler.cpp
        test_gc.cpp)


target_link_libraries(Unit_Tests_run gtest gtest_main)
target_link_libraries(Unit_Tests_run ${CMAKE_PROJECT_NAME}_lexer)
target_link_libraries(Unit_Tests_run ${CMAKE_PROJECT_NAME}c_lib)
target_link_libraries(Unit_Tests_run ${CMAKE_PROJECT_NAME}_runtime)
target_link_libraries(Unit_Tests_run ${CMAKE_PROJECT_NAME}_interpreter)

gtest_discover_tests(Unit_Tests_run)

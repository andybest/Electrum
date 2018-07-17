include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(Unit_Tests_run
        LexerHelpers.h
        test_lexer.cpp
        test_parser.cpp
        test_analyzer.cpp
        test_interpreter.cpp
        test_gc.cpp)


target_link_libraries(Unit_Tests_run gtest gtest_main)
target_link_libraries(Unit_Tests_run ${CMAKE_PROJECT_NAME}_lexer)
target_link_libraries(Unit_Tests_run ${CMAKE_PROJECT_NAME}c_lib)
target_link_libraries(Unit_Tests_run ${CMAKE_PROJECT_NAME}_runtime)
target_link_libraries(Unit_Tests_run ${CMAKE_PROJECT_NAME}_interpreter)

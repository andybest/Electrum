project(${CMAKE_PROJECT_NAME}_lexer)

set(CMAKE_CXX_STANDARD 17)

# Generate the lexer files
add_custom_command(
        OUTPUT lex.yy.cpp lex.yy.h
        COMMAND reflex -F ${CMAKE_CURRENT_SOURCE_DIR}/electrum_lexer.l
        DEPENDS electrum_lexer.l
        COMMENT "Generating lexer."
)

set(LEXER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.h)
set(LEXER_SRC ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp)

set(HEADER_FILES
        ${LEXER_HEADER}
        LexerDefs.h)

set(SOURCE_FILES
        ${LEXER_SRC}
        ../interpreter/Interpreter.cpp
        ../interpreter/Interpreter.h)

add_library(${CMAKE_PROJECT_NAME}_lexer STATIC
        ${HEADER_FILES}
        ${SOURCE_FILES})

target_include_directories(${CMAKE_PROJECT_NAME}_lexer
        PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(${CMAKE_PROJECT_NAME}_lexer
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(${CMAKE_PROJECT_NAME}_lexer
        PUBLIC
        /usr/local/include)

# Link with the reflex static library
target_link_libraries(${CMAKE_PROJECT_NAME}_lexer /usr/local/lib/libreflex.a)

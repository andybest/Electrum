project(${CMAKE_PROJECT_NAME}c)

set(CMAKE_CXX_STANDARD 17)

set(HEADER_FILES
        CompilerContext.h
        Compiler.h
        Parser.h
        Analyzer.h
        ElectrumJit.h
        CompilerExceptions.h
        JitMemoryManager.h Namespace.h)

set(SOURCE_FILES
        main.cpp
        CompilerContext.cpp
        Compiler.cpp
        Parser.cpp
        Analyzer.cpp
        ElectrumJit.cpp
        JitMemoryManager.cpp NamespaceManager.cpp NamespaceManager.h)

add_library(${CMAKE_PROJECT_NAME}c_lib STATIC
        ${HEADER_FILES}
        ${SOURCE_FILES})


# Add location of Homebrew'd LLVM if on MacOS
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/llvm)
endif()

# LLVM
find_package(LLVM 8 REQUIRED CONFIG )

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${CMAKE_PROJECT_NAME}c_lib PUBLIC ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs
        executionengine
        Analysis
        Core
        CodeGen
        ExecutionEngine
        InstCombine
        IRReader
        Object
        OrcJIT
        RuntimeDyld
        ScalarOpts
        Support
        native)
target_link_libraries(${CMAKE_PROJECT_NAME}c_lib ${llvm_libs})

# Boost

find_package(Boost 1.59.0 REQUIRED system filesystem)
include_directories(${Boost_INCLUDE_DIR})

target_link_libraries(${CMAKE_PROJECT_NAME}c_lib ${Boost_LIBRARIES})

target_link_libraries(${CMAKE_PROJECT_NAME}c_lib ${CMAKE_PROJECT_NAME}_lexer)
target_link_libraries(${CMAKE_PROJECT_NAME}c_lib ${CMAKE_PROJECT_NAME}_runtime)

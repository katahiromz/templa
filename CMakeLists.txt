# CMakeLists.txt --- CMake project settings
#    ex) cmake -G "Visual Studio 9 2008" .
#    ex) cmake -DCMAKE_BUILD_TYPE=Release -G "MSYS Makefiles" .
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.0)

# https://stackoverflow.com/questions/10851247/how-do-i-activate-c-11-in-cmake
if (POLICY CMP0025)
    cmake_policy(SET CMP0025 NEW)
endif()

# enable testing
enable_testing()

# project name and languages
project(templa CXX)

# C++11
set(CMAKE_CXX_STANDARD 11)

# Unicode
add_definitions(-DUNICODE -D_UNICODE)

# statically link
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # using Clang
    set(CMAKE_CXX_FLAGS "-static")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # using GCC
    set(CMAKE_CXX_FLAGS "-static")
elseif (MSVC)
    # replace "/MD" with "/MT" (building without runtime DLLs)
    set(CompilerFlags
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    foreach(CompilerFlags ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlags} "${${CompilerFlags}}")
    endforeach()
endif()

##############################################################################

# libtempla.a
add_library(libtempla STATIC templa.cpp)
set_target_properties(libtempla PROPERTIES PREFIX "")
target_link_libraries(libtempla shlwapi)

# templa.exe
add_executable(templa main.cpp templa.cpp)
target_link_libraries(templa shlwapi)

##############################################################################

add_subdirectory(tests)

##############################################################################

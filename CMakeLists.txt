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

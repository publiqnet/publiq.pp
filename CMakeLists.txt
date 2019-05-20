cmake_minimum_required(VERSION 3.2)

option(BUILD_SHARED_LIBS "dynamic link build" ON)
if (BUILD_SHARED_LIBS)
    message("define export symbols for shared lib build")
    add_definitions(-DBUILD_SHARED_LIBS)
endif()

set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED YES )
set( CMAKE_CXX_EXTENSIONS OFF )
set( CMAKE_CXX_VISIBILITY_PRESET hidden )
set( CMAKE_VISIBILITY_INLINES_HIDDEN 1 )
cmake_policy( SET CMP0063 NEW )

if (WIN32)
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_compile_options(/WX)
    add_compile_options(/permissive-)
    add_compile_options(/wd4127)
else (WIN32)
    add_compile_options(-Wall)
    add_compile_options(-Werror)
endif (WIN32)

project(publiq.pp)

add_subdirectory(src)

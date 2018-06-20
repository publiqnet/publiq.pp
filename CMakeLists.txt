cmake_minimum_required(VERSION 3.2)

option(BUILD_SHARED_LIBS "dynamic link build" ON)
if (BUILD_SHARED_LIBS)
    message("define export symbols for shared lib build")
    add_definitions(-DBUILD_SHARED_LIBS)
endif()

set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED YES )

if (WIN32)
    add_compile_options(/wd4251)
    add_compile_options(/wd4275)
else (WIN32)
    add_compile_options(-Wall)
    add_compile_options(-Werror)
endif (WIN32)

project(publiq.pp)

set(PUBLIQ_PP_LOADED TRUE)

add_subdirectory(src)

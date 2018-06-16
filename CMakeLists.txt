cmake_minimum_required(VERSION 3.2)

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

set(PUBLIQ_PP_LOADED TRUE PARENT_SCOPE)

add_subdirectory(src)

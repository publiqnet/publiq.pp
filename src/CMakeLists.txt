# don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/publiq.pp;${CMAKE_INSTALL_PREFIX}/lib/belt.pp;${CMAKE_INSTALL_PREFIX}/lib/mesh.pp")

set(PUBLIQ_PP_LOADED TRUE)

if(NOT BELT_PP_LOADED)
add_subdirectory(belt.pp)
endif()
if(NOT MESH_PP_LOADED)
add_subdirectory(mesh.pp)
endif()
add_subdirectory(libblockchainsocket)
add_subdirectory(publiq.pp)
add_subdirectory(exe_publiqd)

# following is used for find_package functionality
install(FILES publiq.pp-config.cmake DESTINATION lib/publiq.pp)
install(EXPORT publiq.pp.package DESTINATION lib/publiq.pp)

if( WIN32 )
    SET(PUBLIQPP_INSTALL_DESTINATION_RUNTIME bin)
    SET(PUBLIQPP_INSTALL_DESTINATION_LIBRARY lib)
    SET(PUBLIQPP_INSTALL_DESTINATION_ARCHIVE lib)
    SET(PUBLIQPP_INSTALL_DESTINATION_INCLUDE include)
else( WIN32 )
    SET(PUBLIQPP_INSTALL_DESTINATION_RUNTIME bin/publiq.pp)
    SET(PUBLIQPP_INSTALL_DESTINATION_LIBRARY lib/publiq.pp)
    SET(PUBLIQPP_INSTALL_DESTINATION_ARCHIVE lib/publiq.pp)
    SET(PUBLIQPP_INSTALL_DESTINATION_INCLUDE include/publiq.pp)
endif( WIN32 )

if( NOT WIN32 )
    # don't skip the full RPATH for the build tree
    SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

    # when building, don't use the install RPATH already
    # (but later on when installing)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

    SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/publiq.pp;${CMAKE_INSTALL_PREFIX}/lib/belt.pp;${CMAKE_INSTALL_PREFIX}/lib/mesh.pp")

    # not sure if there is any use from below command
    # add the automatically determined parts of the RPATH
    # which point to directories outside the build tree to the install RPATH
    #SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

if(NOT BELT_PP_LOADED)
add_subdirectory(belt.pp)
endif()
if(NOT MESH_PP_LOADED)
add_subdirectory(mesh.pp)
endif()
add_subdirectory(libblockchain)
add_subdirectory(publiq.pp)
add_subdirectory(exe_publiqd)

# following is used for find_package functionality
install(FILES publiq.pp-config.cmake DESTINATION lib/publiq.pp)
install(EXPORT publiq.pp.package DESTINATION lib/publiq.pp)

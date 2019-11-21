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

include(cmake_utility/utility.cmake)

if(NOT TARGET belt.pp)
add_subdirectory(belt.pp)
endif()
if(NOT TARGET mesh.pp)
add_subdirectory(mesh.pp)
endif()
add_subdirectory(blockchain_client)
add_subdirectory(commander)
add_subdirectory(genesis_creator)
add_subdirectory(idl_php)
add_subdirectory(idl_ts)
add_subdirectory(libblockchain)
add_subdirectory(libstorage_utility)
add_subdirectory(publiq.pp)
add_subdirectory(publiqd)
add_subdirectory(storage_helper)
add_subdirectory(test_files_diff)
add_subdirectory(test_actionlog_diff)
add_subdirectory(test_loader_simulation)
add_subdirectory(test_parser_performance)
add_subdirectory(test_db_backed_container)

# following is used for find_package functionality
install(FILES publiq.pp-config.cmake DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY})
install(EXPORT publiq.pp.package NAMESPACE publiq:: DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY})

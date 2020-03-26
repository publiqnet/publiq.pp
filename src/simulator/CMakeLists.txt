# define the executable
add_executable(simulator
    main.cpp)

# libraries this module links to
target_link_libraries(simulator PRIVATE
    socket
    publiq.pp
    mesh.pp
    systemutility
    belt.pp
    blockchain
    utility
    cryptoutility
    log
    mlog
    Boost::filesystem
    Boost::program_options
    Boost::locale
    )

if(NOT WIN32 AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(simulator PRIVATE Threads::Threads)
endif()

# what to do on make install
install(TARGETS simulator
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

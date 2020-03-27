# define the executable
add_executable(publiqd
    main.cpp)

# libraries this module links to
target_link_libraries(publiqd PRIVATE
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
    target_link_libraries(publiqd PRIVATE Threads::Threads)
endif()

# what to do on make install
install(TARGETS publiqd
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

# define the executable
add_executable(storage_helper
    main.cpp)

# libraries this module links to
target_link_libraries(storage_helper PRIVATE
    storageutility
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

# what to do on make install
install(TARGETS storage_helper
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

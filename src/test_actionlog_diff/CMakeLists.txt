# define the executable
add_executable(test_actionlog_diff
    main.cpp)

# libraries this module links to
target_link_libraries(test_actionlog_diff PRIVATE
    mesh.pp
    belt.pp
    socket
    processor
    packet
    utility
    )

# what to do on make install
install(TARGETS test_actionlog_diff
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

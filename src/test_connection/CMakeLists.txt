
# define the executable
add_executable(test_connection
    main.cpp)

# what to do on make install
install(TARGETS test_connection
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

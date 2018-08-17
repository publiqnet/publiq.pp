# define the executable
add_executable(test_files_diff
    main.cpp)

# libraries this module links to
target_link_libraries(test_files_diff PRIVATE
    mesh.pp
    belt.pp
    )

# what to do on make install
install(TARGETS test_files_diff
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

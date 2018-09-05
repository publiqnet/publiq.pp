# define the executable
add_executable(test_parser_performance
    main.cpp)

# libraries this module links to
target_link_libraries(test_parser_performance PRIVATE
    packet
    mesh.pp # adds an implicit dependency on boost and cryptopp
    belt.pp
    utility
    systemutility)

add_dependencies(test_parser_performance blockchain)

# what to do on make install
install(TARGETS test_parser_performance
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

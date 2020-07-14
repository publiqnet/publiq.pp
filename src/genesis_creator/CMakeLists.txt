# define the executable
add_executable(genesis_creator
    main.cpp)

# libraries this module links to
target_link_libraries(genesis_creator PRIVATE
    packet
    mesh.pp
    belt.pp
    utility
    blockchain
    cryptoutility)

# what to do on make install
install(TARGETS genesis_creator
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

# define the executable
add_executable(blockchain_client
    main.cpp)

# libraries this module links to
target_link_libraries(blockchain_client PRIVATE
    socket
    packet
    mesh.pp
    belt.pp
    utility
    Boost::filesystem)

add_dependencies(blockchain_client blockchain)

# what to do on make install
install(TARGETS blockchain_client
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

# generate message.gen.hpp from message.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
  MAIN_DEPENDENCY message.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/message.idl ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
)

# define the executable
add_executable(commander
    main.cpp
    http.hpp
    message.hpp
    message.gen.hpp
    program_options.cpp
    program_options.hpp
    rpc.cpp
    rpc.hpp
    daemon_rpc.cpp
    daemon_rpc.hpp)

# libraries this module links to
target_link_libraries(commander PRIVATE
    socket
    packet
    mesh.pp
    belt.pp
    utility
    systemutility
    cryptoutility
    blockchain
    Boost::filesystem
    Boost::program_options
    Boost::locale)

# what to do on make install
install(TARGETS commander
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

# generate commander_message.gen.hpp from commander_message.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/commander_message.gen.hpp
  MAIN_DEPENDENCY commander_message.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/commander_message.idl ${CMAKE_CURRENT_SOURCE_DIR}/commander_message.gen.hpp
)

# define the executable
add_executable(commander
    commander_message.hpp
    commander_message.gen.hpp
    main.cpp
    http.hpp
    program_options.cpp
    program_options.hpp
    rpc.cpp
    rpc.hpp
    daemon_rpc.cpp
    daemon_rpc.hpp
    utility.cpp
    utility.hpp)

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

# generate storage_manager_message.gen.hpp from storage_manager_message.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/storage_manager_message.gen.hpp
  MAIN_DEPENDENCY storage_manager_message.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/storage_manager_message.idl ${CMAKE_CURRENT_SOURCE_DIR}/storage_manager_message.gen
)

# define the executable
add_executable(storage_manager
    storage_manager_message.hpp
    storage_manager_message.gen.hpp
    main.cpp
    http.hpp
    program_options.cpp
    program_options.hpp
    manager.cpp
    manager.hpp
    daemon_manager.cpp
    daemon_manager.hpp
    utility.cpp
    utility.hpp)

# libraries this module links to
target_link_libraries(storage_manager PRIVATE
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
install(TARGETS storage_manager
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

# generate pid.gen.hpp from pid.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/pid.gen.hpp
  MAIN_DEPENDENCY pid.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/pid.idl ${CMAKE_CURRENT_SOURCE_DIR}/pid.gen.hpp
)

# define the executable
add_executable(exe_publiqd
    main.cpp
    pid.hpp
    pid.gen.hpp
    settings.cpp
    settings.hpp)

# libraries this module links to
target_link_libraries(exe_publiqd PRIVATE
    socket
    publiq.pp
    mesh.pp # adds an implicit dependency on boost and cryptopp
    systemutility
    belt.pp
    blockchain
    utility
    cryptoutility
    log
    )

# what to do on make install
install(TARGETS exe_publiqd
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

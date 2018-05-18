# generate message.gen.hpp from message.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
  MAIN_DEPENDENCY message.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/message.idl ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
)
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/pid.gen.hpp
  MAIN_DEPENDENCY pid.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/pid.idl ${CMAKE_CURRENT_SOURCE_DIR}/pid.gen.hpp
)

# define the executable
add_executable(exe_publiqd
    main.cpp
    message.hpp
    message.gen.hpp
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
    blockchainsocket
    utility
    log
    )

# what to do on make install
install(TARGETS exe_publiqd
        EXPORT publiq.pp.package
        RUNTIME DESTINATION bin/publiq.pp)

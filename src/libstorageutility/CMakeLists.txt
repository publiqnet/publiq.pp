# generate types.gen.hpp from types.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/types.gen.hpp
  MAIN_DEPENDENCY types.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/types.idl ${CMAKE_CURRENT_SOURCE_DIR}/types.gen
)

add_definitions(-DSTORAGEUTILITY_LIBRARY)

# define the library
add_library(storageutility
    global.hpp
    exception.hpp
    exception.cpp
    http.hpp
    node.cpp
    node.hpp
    node_internals.hpp
    types.hpp
    types.gen.hpp)

# libraries this module links to
target_link_libraries(storageutility
    PUBLIC
        mesh.pp
        belt.pp
        publiq.pp
    PRIVATE
        packet
        utility
        p2psocket
        socket
        systemutility
        cryptoutility
        sessionutility
        log)

# what to do on make install
install(TARGETS storageutility
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

install(FILES
    global.hpp
    node.hpp
    DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_INCLUDE}/libstorageutility)

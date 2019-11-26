# generate types.gen.hpp from types.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/types.gen.hpp
  MAIN_DEPENDENCY types.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/types.idl ${CMAKE_CURRENT_SOURCE_DIR}/types.gen
)

add_definitions(-DSTORAGEUTILITY_LIBRARY)

# define the library
add_library(storage_utility
    global.hpp
    exception.hpp
    exception.cpp
    http.hpp
    rpc.cpp
    rpc.hpp
    rpc_internals.hpp
    types.hpp
    types.gen.hpp
    utility.cpp
    utility.hpp)

# libraries this module links to
target_link_libraries(storage_utility
    PUBLIC
        mesh.pp
        belt.pp
        publiq.pp
    PRIVATE
        packet
        utility
        socket
        systemutility
        cryptoutility
        sessionutility
        log)

# what to do on make install
install(TARGETS storage_utility
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

install(FILES
    global.hpp
    rpc.hpp
    DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_INCLUDE}/libstorage_utility)

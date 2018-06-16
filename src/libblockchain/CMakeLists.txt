# generate message.gen.hpp from message.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
  MAIN_DEPENDENCY message.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/message.idl ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
)

# generate data.gen.hpp from data.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/data.gen.hpp
  MAIN_DEPENDENCY data.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/data.idl ${CMAKE_CURRENT_SOURCE_DIR}/data.gen.hpp
)

add_definitions(-DBLOCKCHAIN_LIBRARY)

# define the shared library
add_library(blockchain SHARED
    global.hpp
    blockchain.cpp
    blockchain.hpp
    state.cpp
    state.hpp
    data.hpp
    data.gen.hpp
    message.hpp
    message.gen.hpp
    node.cpp
    node.hpp)

# libraries this module links to
target_link_libraries(blockchain PUBLIC
    mesh.pp
    belt.pp
    packet
    PRIVATE
    utility
    p2psocket
    socket
    systemutility)

# what to do on make install
install(TARGETS blockchain
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

install(FILES
    global.hpp
    node.hpp
    DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_INCLUDE}/libblockchain)

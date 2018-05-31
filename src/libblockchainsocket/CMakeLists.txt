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

add_definitions(-DBLOCKCHAINSOCKET_LIBRARY)

# define the shared library
add_library(blockchainsocket SHARED
    global.hpp
    blockchainsocket.cpp
    blockchainsocket.hpp
    blockchain.cpp
    blockchain.hpp
    blockchainstate.cpp
    blockchainstate.hpp
    data.hpp
    data.gen.hpp
    message.hpp
    message.gen.hpp)

# libraries this module links to
target_link_libraries(blockchainsocket PUBLIC
    mesh.pp
    belt.pp
    packet
    PRIVATE
    utility
    p2psocket
    socket)

# what to do on make install
install(TARGETS blockchainsocket
        EXPORT publiq.pp.package
        LIBRARY DESTINATION lib/publiq.pp
        ARCHIVE DESTINATION lib/publiq.pp/archive)

install(FILES
    global.hpp
    blockchainsocket.hpp
    DESTINATION include/publiq.pp/libblockchainsocket)

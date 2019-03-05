# generate message.gen.hpp from message.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
  MAIN_DEPENDENCY message.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/message.idl ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
)

add_definitions(-DBLOCKCHAIN_LIBRARY)

# define the library
add_library(blockchain
    global.hpp
    action_log.cpp
    action_log.hpp
    blockchain.cpp
    blockchain.hpp
    communication_rpc.cpp
    communication_rpc.hpp
    communication_p2p.cpp
    communication_p2p.hpp
    coin.cpp
    coin.hpp
    common.hpp
    documents.cpp
    documents.hpp
    exception.hpp
    exception.cpp
    http.hpp
    message.hpp
    message.gen.hpp
    node.cpp
    node.hpp
    node_internals.hpp
    nodeid_service.cpp
    nodeid_service.hpp
    open_container_packet.hpp
    sessions.cpp
    sessions.hpp
    state.cpp
    state.hpp
    storage.cpp
    storage.hpp
    storage_node.cpp
    storage_node.hpp
    storage_node_internals.hpp
    transaction_content.cpp
    transaction_content.hpp
    transaction_contentinfo.cpp
    transaction_contentinfo.hpp
    transaction_contentunit.cpp
    transaction_contentunit.hpp
    transaction_file.cpp
    transaction_file.hpp
    transaction_handler.cpp
    transaction_handler.hpp
    transaction_pool.cpp
    transaction_pool.hpp
    transaction_role.cpp
    transaction_role.hpp
    transaction_statinfo.cpp
    transaction_statinfo.hpp
    transaction_transfer.cpp
    transaction_transfer.hpp)

# libraries this module links to
target_link_libraries(blockchain
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
install(TARGETS blockchain
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

install(FILES
    coin.hpp
    global.hpp
    node.hpp
    message.hpp
    message.gen.hpp
    storage_node.hpp
    DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_INCLUDE}/libblockchain)

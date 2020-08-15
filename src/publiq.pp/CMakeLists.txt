# interface library for headers only module
add_library(publiq.pp INTERFACE)

# modules linking to this library will include following
# directories. two options are provided for installed package
# and for in-tree source build
target_include_directories(publiq.pp INTERFACE
    $<INSTALL_INTERFACE:include/publiq.pp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)

# libraries this module links to
target_link_libraries(publiq.pp)

# what to do on make install
install(TARGETS publiq.pp
        EXPORT publiq.pp.package
        DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY})

set(SRC_FILES
    coin.hpp
    global.hpp
    message.hpp
    message.tmpl.hpp
    node.hpp
    storage_node.hpp
    storage_utility_rpc.hpp
    network.hpp)

install(FILES
    ${SRC_FILES}
    DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_INCLUDE}/publiq.pp)

#
# add custom target simply for IDE to show the files
#
add_custom_target(publiq.pp.include SOURCES
    ${SRC_FILES})

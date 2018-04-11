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
        DESTINATION lib/publiq.pp)

set(SRC_FILES
    global.hpp)

install(FILES
    ${SRC_FILES}
    DESTINATION include/publiq.pp/publiq.pp)

#
# add custom target simply for IDE to show the files
#
add_custom_target(publiq.pp.include SOURCES
    ${SRC_FILES})

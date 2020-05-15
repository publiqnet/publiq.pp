
# define the executable
add_executable(test_broadcast
        main.cpp)

# libraries this module links to
target_link_libraries(test_broadcast
    PRIVATE
        publiq.pp
        mesh.pp
        systemutility
        belt.pp
        blockchain
        utility
        cryptoutility
        log
        mlog
        Boost::filesystem
        Boost::program_options
        Boost::locale
        simulator
)

add_dependencies(test_broadcast simulator)

# what to do on make install
install(TARGETS test_broadcast
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

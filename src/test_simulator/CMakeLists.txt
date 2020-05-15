
# define the executable
add_executable(test_simulator
        main.cpp)

# libraries this module links to
target_link_libraries(test_simulator
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

add_dependencies(test_simulator simulator)

# what to do on make install
install(TARGETS test_simulator
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

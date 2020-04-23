
# define the executable
add_executable(test_simulator
        main.cpp)

# libraries this module links to
target_link_libraries(test_simulator
    PUBLIC
        publiq.pp
    PRIVATE
        simulator
)

# what to do on make install
install(TARGETS test_simulator
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

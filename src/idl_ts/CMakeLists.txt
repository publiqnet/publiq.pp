# define the executable
add_executable(idl_ts
    generator.cpp
    generator.hpp
    main.cpp)

# libraries this module links to
target_link_libraries(idl_ts
    PRIVATE
        belt.pp
        mesh.pp
        Boost::filesystem)

# what to do on make install
install(TARGETS idl_ts
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

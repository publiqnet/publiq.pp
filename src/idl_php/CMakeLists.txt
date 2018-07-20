# define the executable
add_executable(idl_php
    generator.cpp
    generator.hpp
    main.cpp)

# libraries this module links to
target_link_libraries(idl_php PUBLIC belt.pp)

# what to do on make install
install(TARGETS idl_php
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

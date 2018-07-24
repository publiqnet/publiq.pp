# define the executable
add_executable(idl_php
    generator.cpp
    generator.hpp
    main.cpp)


find_package(Boost 1.58 REQUIRED
    COMPONENTS
               filesystem )

MESSAGE(STATUS "boost info")
MESSAGE(STATUS "${Boost_INCLUDE_DIR}")
MESSAGE(STATUS "${Boost_LIBRARIES}")

target_include_directories(idl_php PUBLIC
               ${Boost_INCLUDE_DIR})


# libraries this module links to
target_link_libraries(idl_php PUBLIC belt.pp
                        ${Boost_LIBRARIES})

# what to do on make install
install(TARGETS idl_php
        EXPORT publiq.pp.package
        RUNTIME DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${PUBLIQPP_INSTALL_DESTINATION_ARCHIVE})

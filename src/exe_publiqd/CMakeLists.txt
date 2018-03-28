# define the executable
add_executable(exe_publiqd
    main.cpp)

# libraries this module links to
target_link_libraries(exe_publiqd PUBLIC
    publiq.pp
    mesh.pp
    belt.pp
    p2psocket
    )

# what to do on make install
install(TARGETS exe_publiqd
        EXPORT publiq.pp.package
        RUNTIME DESTINATION bin/publiq.pp)

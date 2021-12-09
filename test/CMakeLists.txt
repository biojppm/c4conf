c4_setup_testing(DOCTEST)

function(c4conf_test name)
    set(name c4conf-test-${name})
    c4_add_executable(${name}
        SOURCES ${ARGN}
        LIBS c4conf doctest
        FOLDER test)
    c4_add_test(${name})
endfunction()

c4conf_test(basic test_basic.cpp)
c4conf_test(opts test_opts.cpp)

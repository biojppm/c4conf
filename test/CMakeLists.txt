c4_setup_testing(DOCTEST)

function(c4conf_test name)
    c4_add_executable(c4conf-test-${name}
        SOURCES ${ARGN}
        LIBS c4conf doctest
        FOLDER test)
    c4_add_test(c4conf-test-${name})
endfunction()

c4conf_test(basic test_basic.cpp)
c4conf_test(opts test_opts.cpp)
if(NOT CMAKE_CROSSCOMPILING)
    include(./test_quickstart.cmake)
endif()

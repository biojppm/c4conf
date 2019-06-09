c4conf_setup_testing()

function(c4conf_test name)
    set(name c4conf-test-${name})
    c4conf_add_executable(${name}
        SOURCES ${ARGN}
        LIBS c4conf gtest_main gtest
        FOLDER c4conf-test)
    c4conf_add_test(${name})
endfunction()

c4conf_test(basic basic.cpp)

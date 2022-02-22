# quickstart examples:
find_package(Python REQUIRED COMPONENTS Interpreter)
function(c4conf_add_quickstart_case name expected_yaml)
    set(pydriver ${CMAKE_CURRENT_BINARY_DIR}/quickstart_test_driver.py)
    if(NOT TARGET c4conf-test-quickstart)
        c4_add_executable(c4conf-test-quickstart
            SOURCES ../samples/quickstart.cpp
            LIBS c4conf
            FOLDER test)
        add_dependencies(c4conf-test-build c4conf-test-quickstart)
        add_custom_target(c4conf-test-quickstart-run
            COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> -R "c4conf-test-quickstart-.*" --output-on-failure
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        file(WRITE ${pydriver} "
import os
import sys
import subprocess

def log(*args, **kwargs):
    kwargs['flush'] = True
    print(*args, **kwargs)

def fixwhitespace(s: str):
    return s.replace('\\r', \"\").strip()

log('argv=', sys.argv)
exe = sys.argv[1]
expected_file = sys.argv[2]
args = [exe] + sys.argv[3:]

log('opening file:', expected_file)
with open(expected_file) as f:
    expected_yaml = fixwhitespace(f.read())

log('executing:', *args)
sp = subprocess.run(args, capture_output=True, check=True)
actual_yaml = fixwhitespace(sp.stdout.decode('utf8'))

if actual_yaml != expected_yaml:
    log(f'''ERROR: yaml is different:
========
expected
========
~~~{expected_yaml}~~~
======
actual
======
~~~{actual_yaml}~~~
''')
    exit(1)
")
    endif()
    set(expected_file "${CMAKE_CURRENT_BINARY_DIR}/${name}.yml")
    file(WRITE "${expected_file}" "${expected_yaml}")
    add_test(NAME c4conf-test-quickstart-${name}
        COMMAND "${Python_EXECUTABLE}" "${pydriver}" "$<TARGET_FILE:c4conf-test-quickstart>" "${expected_file}" ${ARGN})
endfunction()

c4conf_add_quickstart_case(seq_node_values "
foo:
  - foo0
  - 1.234e9
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely" -cn foo[1]=1.234e9)


c4conf_add_quickstart_case(map_node_values "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: newvalue
baz: definitely"
-cn bar.bar2=newvalue)


c4conf_add_quickstart_case(change_values_repeatedly "
foo:
  - foo0
  - 2.468e9
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely"
-cn foo[1]=0 -cn foo[1]=1.234e9 -cn foo[1]=2.468e9
    )


c4conf_add_quickstart_case(append_elements_to_a_seq "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
  - these
  - will
  - be
  - appended
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely"
-cn foo=[these,will,be,appended]
    )


c4conf_add_quickstart_case(append_elements_to_a_map "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
  these: will
  be: appended
baz: definitely"
-cn "bar='{these: will, be: appended}'"
    )


c4conf_add_quickstart_case(replace_all_elements_in_a_seq "
foo:
  - all
  - new
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely"
-cn foo=~ -cn foo=[all,new]
    )


c4conf_add_quickstart_case(replace_all_elements_in_a_map "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  all: new
baz: definitely"
-cn bar=~ -cn "bar='{all: new}'"
    )


c4conf_add_quickstart_case(replace_a_seq_with_a_different_type "
foo: newfoo
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely"
-cn foo=newfoo
    )


c4conf_add_quickstart_case(replace_a_map_with_a_different_type "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar: newbar
baz: definitely"
-cn bar=newbar
    )


c4conf_add_quickstart_case(add_new_nodes_seq "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely
coffee:
  - morning
  - lunch
  - afternoon"
-cn coffee=[morning,lunch,afternoon]
    )


c4conf_add_quickstart_case(add_new_nodes_map "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely
wine:
  green: summer
  red: winter
  champagne: 'year-round'"
-cn "wine='{green: summer, red: winter, champagne: year-round}'"
    )

c4conf_add_quickstart_case(add_new_nested_nodes_seq "
foo:
  - foo0
  - foo1
  - foo2
  - - a
    - b
    - c
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely"
-cn foo[3]=[a,b,c]
    )

c4conf_add_quickstart_case(add_new_nested_nodes_map "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
  possibly:
    - d
    - e
    - f
baz: definitely"
-cn bar.possibly=[d,e,f]
    )

c4conf_add_quickstart_case(noncontiguous_seq "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
  - 
  - - d
    - e
    - f
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely"
-cn foo[5]=[d,e,f]
    )


c4conf_add_quickstart_case(erase_all "eraseall" -cn eraseall)


c4conf_add_quickstart_case(call_setfoo "
foo:
  - foo0
  - foo1
  - 'foo2, actually footoo'
  - - foo3elm0
    - foo3elm1
    - foo3elm2
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely"
-sf
    )


c4conf_add_quickstart_case(call_setfoo3 "
foo:
  - foo0
  - foo1
  - foo2
  - 123
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely"
-sf3 123
    )

c4conf_add_quickstart_case(call_setfoo3_equiv "
foo:
  - foo0
  - foo1
  - foo2
  - 123
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: indeed2
baz: definitely"
-cn foo[3]=123
    )


c4conf_add_quickstart_case(call_setbar2 "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: the value
baz: definitely"
-sb2 "the value"
    )

c4conf_add_quickstart_case(call_setbar2_equiv "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: the value
baz: definitely"
-cn "'bar.bar2=the value'"
    )


c4conf_add_quickstart_case(call_setbar2_omit "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: 
baz: definitely"
-sb2
    )

c4conf_add_quickstart_case(call_setbar2_omit_equiv "
foo:
  - foo0
  - foo1
  - foo2
  - foo3
bar:
  bar0: indeed0
  bar1: indeed1
  bar2: ~
baz: definitely"
-cn bar.bar2=~
    )

add_test(NAME c4conf-test-quickstart-help
    COMMAND "$<TARGET_FILE:c4conf-test-quickstart>" --help)

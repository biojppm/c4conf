cmake_minimum_required(VERSION 3.2)
project(c4conf)

include(./ext/c4core/cmake/c4Project.cmake)

c4_declare_project(c4conf
    DESC "YAML configuration loading and merging from multiple files"
    AUTHOR "Joao Paulo Magalhaes <dev@jpmag.me>"
    MAJOR 0 MINOR 0 RELEASE 1)

set(C4CONF_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
set(C4CONF_EXT_DIR ${CMAKE_CURRENT_LIST_DIR}/ext)

c4conf_require_module(c4core
    SUBDIRECTORY ${C4CONF_EXT_DIR}/c4core)

c4conf_require_module(c4fs
    SUBDIRECTORY ${C4CONF_EXT_DIR}/c4fs)

c4_override(RYML_STANDALONE OFF)
c4conf_require_module(ryml
    SUBDIRECTORY ${C4CONF_EXT_DIR}/rapidyaml)

set(C4CONF_SRC
    ${C4CONF_SRC_DIR}/c4/conf/conf.hpp
    ${C4CONF_SRC_DIR}/c4/conf/conf.cpp
    )

c4conf_add_library(c4conf
    SOURCES ${C4CONF_SRC}
    LIBS c4fs ryml c4core
    INC_DIRS ${C4CONF_SRC_DIR}
)

if(C4CONF_BUILD_TESTS)
    enable_testing() # this must be done here (and not inside the test dir)
                     # so that the test targets are available at the top level
    add_subdirectory(test)
endif()

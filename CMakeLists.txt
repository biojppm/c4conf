cmake_minimum_required(VERSION 3.13)
include(./ext/c4core/cmake/c4Project.cmake)
project(c4conf
    DESCRIPTION "YAML configuration loading and merging from multiple files"
    LANGUAGES CXX)
c4_project(VERSION 0.0.1
    CXX_STANDARD 17
    AUTHOR "Joao Paulo Magalhaes <dev@jpmag.me>")

c4_require_subproject(c4core SUBDIRECTORY ${C4CONF_EXT_DIR}/c4core)
c4_require_subproject(c4fs   SUBDIRECTORY ${C4CONF_EXT_DIR}/c4fs)
c4_require_subproject(ryml   SUBDIRECTORY ${C4CONF_EXT_DIR}/rapidyaml)

set(C4CONF_SRC
    ${C4CONF_SRC_DIR}/c4/conf/conf.hpp
    ${C4CONF_SRC_DIR}/c4/conf/conf.cpp
    )

c4_add_library(c4conf
    SOURCES ${C4CONF_SRC}
    LIBS c4fs ryml c4core
    INC_DIRS
        $<BUILD_INTERFACE:${C4CONF_SRC_DIR}> $<INSTALL_INTERFACE:include>
)

c4_install_target(c4conf)
c4_install_exports()

c4_add_dev_targets()


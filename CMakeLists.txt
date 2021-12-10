cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
include(./ext/c4core/cmake/c4Project.cmake)
project(c4conf
    DESCRIPTION "YAML configuration loading and merging from multiple files"
    HOMEPAGE_URL "https://github.com/biojppm/c4conf"
    LANGUAGES CXX)
c4_project(VERSION 0.0.1
    AUTHOR "Joao Paulo Magalhaes <dev@jpmag.me>")

c4_require_subproject(c4core SUBDIRECTORY ${C4CONF_EXT_DIR}/c4core)
c4_require_subproject(c4fs   SUBDIRECTORY ${C4CONF_EXT_DIR}/c4fs)
c4_require_subproject(ryml   SUBDIRECTORY ${C4CONF_EXT_DIR}/rapidyaml)

c4_add_library(c4conf
    SOURCES c4/conf/conf.hpp c4/conf/conf.cpp c4/conf/export.hpp
    SOURCE_ROOT ${C4CONF_SRC_DIR}
    LIBS c4fs ryml c4core
    INC_DIRS
        $<BUILD_INTERFACE:${C4CONF_SRC_DIR}> $<INSTALL_INTERFACE:include>
)

c4_install_target(c4conf)
c4_install_exports()

c4_add_dev_targets()


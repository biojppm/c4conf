cmake_minimum_required(VERSION 3.12)
project(c4conf-quickstart LANGUAGES CXX)

find_package(c4conf REQUIRED)

add_executable(c4conf-quickstart ../quickstart.cpp)
target_link_libraries(c4conf-quickstart c4conf::c4conf)  # note the namespace!

add_custom_target(run c4conf-quickstart
    COMMAND $<TARGET_FILE:c4conf-quickstart>
    DEPENDS c4conf-quickstart
    COMMENT "running: $<TARGET_FILE:c4conf-quickstart>")

cmake_minimum_required(VERSION 3.24.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

add_subdirectory(lua-util)

project(luac)

add_executable(luac test/luac.c)
target_link_libraries(luac PRIVATE lua-core)

add_custom_target(luac-copy
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:luac>"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/$<TARGET_FILE_NAME:luac>"
    DEPENDS luac
    COMMENT "copying executable to dist directory"
)

project(lua-util-test)

add_executable(lua-util-test test/main.cpp)
target_link_libraries(lua-util-test PRIVATE lua-util)

cmake_minimum_required(VERSION 3.24.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

project(lua-core)

# library
set(LUA_LIB_CORE_SRC
  lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c
  lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c
  lparser.c lstate.c lstring.c ltable.c ltm.c
  lundump.c lvm.c lzio.c lauxlib.c lbaselib.c
  lcorolib.c ldblib.c liolib.c lmathlib.c loadlib.c
  loslib.c lstrlib.c ltablib.c lutf8lib.c linit.c
)
list(TRANSFORM LUA_LIB_CORE_SRC PREPEND "../lua/")

# interpreter
set(LUA_INTERPRETER ../lua/lua.c)

add_library(lua-core STATIC
  ${LUA_LIB_CORE_SRC}
  ${LUA_INTERPRETER}
)
target_include_directories(lua-core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../lua/>
)

project(lua-util)

set(LUA_UTIL_SRC
  lua_util.cpp
  lua_util.hpp
  lua_util_chunk.h
  lua_util_chunk.cpp
  lua_util_args.hpp
)

include_directories(./)
add_library(lua-util ${LUA_UTIL_SRC})
target_include_directories(lua-util PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/./>
)

target_link_libraries(${PROJECT_NAME} PUBLIC lua-core)

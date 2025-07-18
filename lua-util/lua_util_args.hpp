#pragma once

#include <lua.hpp>

#include <tuple>
#include <string>
#include <utility>
#include <sstream>

namespace lua_util {

template<typename T>
struct arg {
  template<typename> struct always_false : std::false_type {};
  static T get(lua_State* L, int idx) { static_assert(always_false<T>::value, "unsupport type"); }
  static inline void push(lua_State* L, const T& value) { static_assert(always_false<T>::value, "unsupport type"); }
};

template<>
struct arg<double> {
  static double get(lua_State* L, int idx) {
    if (lua_type(L, idx) != LUA_TNUMBER)
      luaL_error(L, "arg #%d must be a number", idx);
    return lua_tonumber(L, idx);
  }

  static inline void push(lua_State* L, const double& value) { lua_pushnumber(L, value); }
};

template<>
struct arg<float> {
  static double get(lua_State* L, int idx) {
    if (lua_type(L, idx) != LUA_TNUMBER)
      luaL_error(L, "arg #%d must be a number", idx);
    return lua_tonumber(L, idx);
  }

  static inline void push(lua_State* L, const float& value) { lua_pushnumber(L, value); }
};

template<>
struct arg<int32_t> {
  static double get(lua_State* L, int idx) {
    if (lua_type(L, idx) != LUA_TNUMBER)
      luaL_error(L, "arg #%d must be a number", idx);
    return lua_tointeger(L, idx);
  }

  static inline void push(lua_State* L, const int32_t& value) { lua_pushinteger(L, value); }
};

template<>
struct arg<int64_t> {
  static double get(lua_State* L, int idx) {
    if (lua_type(L, idx) != LUA_TNUMBER)
      luaL_error(L, "arg #%d must be a number", idx);
    return lua_tointeger(L, idx);
  }

  static inline void push(lua_State* L, const int64_t& value) { lua_pushinteger(L, value); }
};

template<>
struct arg<uint32_t> {
  static double get(lua_State* L, int idx) {
    if (lua_type(L, idx) != LUA_TNUMBER)
      luaL_error(L, "arg #%d must be a number", idx);
    return lua_tointeger(L, idx);
  }

  static inline void push(lua_State* L, const uint32_t& value) { lua_pushinteger(L, value); }
};

template<>
struct arg<uint64_t> {
  static double get(lua_State* L, int idx) {
    if (lua_type(L, idx) != LUA_TNUMBER)
      luaL_error(L, "arg #%d must be a number", idx);
    return lua_tointeger(L, idx);
  }

  static inline void push(lua_State* L, const uint64_t& value) { lua_pushinteger(L, value); }
};

template<>
struct arg<bool> {
  static bool get(lua_State* L, int idx) {
    if (lua_type(L, idx) != LUA_TBOOLEAN)
      luaL_error(L, "arg #%d must be a boolean", idx);
    return lua_toboolean(L, idx) != 0;
  }

  static inline void push(lua_State* L, const bool& value) { lua_pushboolean(L, value ? 1 : 0); }
};

template<>
struct arg<std::string> {
  static std::string get(lua_State* L, int idx) {
    if (lua_type(L, idx) != LUA_TSTRING)
      luaL_error(L, "arg #%d must be a string", idx);
    size_t len;
    const char* str = lua_tolstring(L, idx, &len);
    return std::string(str, len);
  }

  static bool try_get(lua_State* L, int idx, std::string& o_str) {
    const auto type = lua_type(L, idx);
    if (type != LUA_TSTRING && type != LUA_TNUMBER && type != LUA_TBOOLEAN && type != LUA_TNIL) return false;
    size_t len;
    const char* str = lua_tolstring(L, idx, &len);
    o_str = std::string(str, len);
    return true;
  }

  static bool try_get(lua_State* L, int idx, std::stringstream& o_str) {
    const auto type = lua_type(L, idx);
    if (type != LUA_TSTRING && type != LUA_TNUMBER && type != LUA_TBOOLEAN && type != LUA_TNIL) return false;
    o_str << lua_tostring(L, idx);
    return true;
  }

  static inline void push(lua_State* L, const std::string& value) { lua_pushlstring(L, value.data(), value.size()); }
};

template<>
struct arg<const char*> {
  static const char* get(lua_State* L, int idx) {
    if (lua_type(L, idx) != LUA_TSTRING)
      luaL_error(L, "arg #%d must be a string", idx);
    size_t len;
    const char* str = lua_tolstring(L, idx, &len);
    return str;
  }

  static inline void push(lua_State* L, const char* value) { lua_pushstring(L, value); }
};

template<size_t s>
struct arg<char[s]> {
  static const char* get(lua_State* L, int idx) {
    if (lua_type(L, idx) != LUA_TSTRING)
      luaL_error(L, "arg #%d must be a string", idx);
    size_t len;
    const char* str = lua_tolstring(L, idx, &len);
    return str;
  }

  static inline void push(lua_State* L, const char* value) { lua_pushstring(L, value); }
};

template<typename... Args, size_t... Is>
void lua_util_extract_args(lua_State* L, std::tuple<Args...>& tuple, std::index_sequence<Is...>) {
  ((std::get<Is>(tuple) = arg<std::decay_t<Args>>::get(L, Is + 1)), ...);
}

}

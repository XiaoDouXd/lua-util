
#include <array>
#include <fstream>
#include <iostream>

#include <lua_util.hpp>

int print(lua_State* L) {
  using namespace lua_util;
  auto nargs = lua_param_cnt(L);
  auto ss = std::stringstream();

  for (auto i = -1; i >= -nargs; i--) {
    if (!arg<std::string>::try_get(L, i, ss)) {
      ss << lua_param_typename(L, i) << '\t';
    }
  }
  std::cout << ss.str() << std::endl;
  return 0;
}

int Ext_FuncA(lua_State* L) {
  using namespace lua_util;
  auto nargs = lua_param_cnt(L);
  if (nargs != 2) return luaL_error(L, "wrong param cnt: %d", nargs);

  const auto param2 = arg<std::double_t>::get(L, -1);
  const auto param1 = arg<std::string>::get(L, -2);
  std::cout << "[FuncA] " << "param1: " << param1 << ", param2: " << param2 << std::endl;
  std::cout << "[FuncA] return param2 + 10 = " << param2 + 10 << std::endl;
  arg<std::double_t>::push(L, param2 + 10);
  return 1;
}

int Ext_FuncB(lua_State* L) {
  using namespace lua_util;
  auto nargs = lua_param_cnt(L);
  if (nargs != 2) return luaL_error(L, "wrong param cnt: %d", nargs);

  const auto param2 = arg<std::double_t>::get(L, -1);
  const auto param1 = arg<std::double_t>::get(L, -2);
  std::cout << "[FuncB] " << "param1: " << param1 << ", param2: " << param2 << std::endl;
  std::cout << "[FuncB] return param2 + param1 = " << param1 + param2 << std::endl;
  arg<std::double_t>::push(L, param2 + param1);
  return 1;
}

const std::array<lua_util::lua_bind_data, 2> Ext = {{
  { "FuncA", Ext_FuncA },
  { "FuncB", Ext_FuncB },
}};

std::string Ext_Str_FuncC(const double& param1, const std::string& param2, const bool param3, const char* param4) {
  auto ss = std::stringstream();
  ss << "[Ext_Str_FuncC] " << std::endl
    << "    param1: " << param1 << std::endl
    << "    param2: " << param2 << std::endl
    << "    param3: " << param3 << std::endl
    << "    param4: " << param4 << std::endl;
  auto r = ss.str();
  std::cout << r;
  return r;
}

int main() {
  // create lua env state
  auto env = lua_util::lua_env();

  // load lua script
  auto f = std::fstream("../test/main.lua.bytes", std::ios::in | std::ios::binary);
  if (!f.is_open()) {
    std::cerr << "fail to open lua bytes";
    return -1;
  }
  f.seekg(0, std::ios::end);
  const auto size = f.tellg();
  f.seekg(0, std::ios::beg);
  auto buffer = std::vector<uint8_t>(size);
  f.read((char*)buffer.data(), size);

  // load
  // env.load("../test/main.lua");
  env.load("main", buffer.data(), size);

  // bind
  env.bind("Ext", Ext);
  env.bind("Ext.Str", "FuncC", Ext_Str_FuncC);
  env.bind("print", print);

  // call
  env.call();

  // push val
  env.push(79837);

  // ref and call lua func
  const auto luaFunc = env.ref_global("LuaFunc");
  env.call(luaFunc, 0.125, "\"luaFunc strValue\"");
  return 0;
}

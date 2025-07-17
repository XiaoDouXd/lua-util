#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "lua_util.hpp"

using namespace lua_util;

#define CHECK_LUA if (!_env) throw std::runtime_error("invalid lua state")

lua_env::~lua_env() {
  if (!_env) return;
  lua_close(_env);
  _env = nullptr;

  for (auto _ref : _refs) delete _ref;
  _refs.clear();
}

lua_env::lua_env(): _env(luaL_newstate()) {
  CHECK_LUA;
  luaL_openlibs(_env);
}

lua_env::lua_env(lua_env &&other) {
  _env = other._env;
  other._env = nullptr;
}

lua_env& lua_env::operator=(lua_env &&other) {
  _env = other._env;
  other._env = nullptr;
  return *this;
}

const char* lua_env::load(const char* filename) {
  CHECK_LUA;

  int ret = luaL_loadfile(_env, filename);
  if (ret) return "lua_env: failed to load file";
  return nullptr;
}

const char* lua_env::load(const char* name, const uint8_t* buffer, size_t size) {
  CHECK_LUA;

  int ret = luaL_loadbuffer(_env, (const char*)buffer, size, name);
  if (ret) return "lua_env: failed to load buffer";
  return nullptr;
}

const char* lua_env::call() {
  CHECK_LUA;

  int ret = lua_pcall(_env, 0, 0, 0);
  if (ret) {
    auto r = lua_tostring(_env, -1);
    lua_pop(_env, 1);
    return r;
  }
  return nullptr;
}

lua_ref lua_env::ref_global(const std::string_view &name) {
  CHECK_LUA;

  lua_getglobal(_env, name.data());
  if (lua_isnil(_env, -1)) {
    lua_pop(_env, 1);
    return nullptr;
  }

  auto *_ref = new lua_ref_inner;
  _ref->typ = (enum e_lua_type)lua_type(_env, -1);
  _ref->ref = luaL_ref(_env, LUA_REGISTRYINDEX);
  if (_ref->ref == LUA_REFNIL) {
    delete _ref;
    return nullptr;
  }

  _refs.insert(_ref);
  return (lua_ref)_ref;
}

lua_ref lua_env::ref(const int32_t &idx) {
  CHECK_LUA;

  lua_pushvalue(_env, idx);
  if (lua_isnil(_env, -1)) {
    lua_pop(_env, 1);
    return nullptr;
  }

  auto *_ref = new lua_ref_inner;
  _ref->typ = (enum e_lua_type)lua_type(_env, -1);
  _ref->ref = luaL_ref(_env, LUA_REGISTRYINDEX);
  if (_ref->ref == LUA_REFNIL) {
    delete _ref;
    return nullptr;
  }

  _refs.insert(_ref);
  return (lua_ref)_ref;
}

bool lua_env::push(const lua_ref &ref) {
  CHECK_LUA;

  const auto _ref = (lua_ref_inner*)ref;
  const auto it = _refs.find(_ref);
  if (it == _refs.end()) return false;

  lua_rawgeti(_env, LUA_REGISTRYINDEX, _ref->ref);
  return true;
}

void lua_env::unref(const lua_ref &ref) {
  CHECK_LUA;

  if (!ref) return;
  const auto _ref = (lua_ref_inner*)ref;
  const auto it = _refs.find(_ref);
  if (it == _refs.end()) return;

  luaL_unref(_env, LUA_REGISTRYINDEX, _ref->ref);
  _refs.erase(it);
  delete _ref;
}

std::string lua_env::stack_dump(int nPreStack) {
  CHECK_LUA;
  return lua_util::stack_dump(_env, nPreStack);
}

std::string lua_util::stack_dump(lua_State* L, int nPreStack) {
  auto top = lua_gettop(L);
  auto ss = std::stringstream();
  ss << "---- stack dump (top=" << top << ") ----\n";

  for (auto i = top; i > nPreStack; i--) {
    auto t = lua_type(L, i);
    ss << "    >> " << i << "(" << i - top - 1 << "): [" << lua_typename(L, t) << "] ";
    switch(t) {
      case LUA_TNUMBER:
        if (lua_isinteger(L, i))
          ss << (long long)lua_tointeger(L, i);
        else
          ss << lua_tonumber(L, i);
        break;
      case LUA_TSTRING:
        ss << "\"" << lua_tostring(L, i) << "\"";
        break;
      case LUA_TBOOLEAN:
        ss << (lua_toboolean(L, i) ? "true" : "false");
        break;
      case LUA_TNIL:
        ss << "nil";
        break;
      case LUA_TFUNCTION:
        if (lua_iscfunction(L, i)) 
          ss << "c-function@" << (void*)lua_tocfunction(L, i);
        else
          ss << "lua-function@" << lua_topointer(L, i);
        break;
      case LUA_TUSERDATA:
        ss << "userdata@" << lua_touserdata(L, i);
        break;
      case LUA_TTHREAD:
        ss << "thread@" << lua_tothread(L, i);
        break;
      case LUA_TTABLE:
        ss << "table@" << lua_topointer(L, i);
        break;
      default:
        ss << lua_topointer(L, i);
    }
    ss << "\n";
  }
  return ss.str();
}

int lua_util::create_table(lua_State *L, const std::string_view &tablePath) {
  // 解析表路径（支持嵌套表如 "EXT.sub"）
  size_t start = 0;
  size_t end = tablePath.find('.');
  std::string current = std::string(tablePath.substr(0, end));

  // 获取根表
  lua_getglobal(L, current.c_str());
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1); // 移除 nil
    lua_createtable(L, 0, 5); // 创建新表
    lua_pushvalue(L, -1); // 复制表引用
    lua_setglobal(L, current.c_str()); // 设置为全局变量
  }

  // 处理嵌套表
  while (end != std::string_view::npos) {
    start = end + 1;
    end = tablePath.find('.', start);
    current = std::string(tablePath.substr(start, end - start));

    // 获取子表
    lua_getfield(L, -1, current.data());
    if (!lua_istable(L, -1)) {
      lua_pop(L, 1); // 移除 nil
      lua_createtable(L, 0, 5); // 创建新子表
      lua_pushvalue(L, -1); // 复制子表引用
      lua_setfield(L, -3, current.c_str()); // 设置到父表
    }

    // 移除父表，保留子表
    lua_remove(L, -2);
  }
  return -1; // 返回新表的索引
}

int lua_util::get_global(lua_State *L, const std::string_view &tablePath, const std::string_view &field) {
  // 解析表路径（支持嵌套表如 "EXT.sub"）
  size_t start = 0;
  size_t end = tablePath.find('.');
  std::string current = std::string(tablePath.substr(0, end));

  // 获取根表
  lua_getglobal(L, current.c_str());
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1); // 移除
    return 0;
  }

  // 处理嵌套表
  while (end != std::string_view::npos) {
    start = end + 1;
    end = tablePath.find('.', start);
    current = std::string(tablePath.substr(start, end - start));

    // 获取子表
    lua_getfield(L, -1, current.data());
    if (!lua_istable(L, -1)) {
      lua_pop(L, 1); // 移除
      return 0;
    }

    // 移除父表，保留子表
    lua_remove(L, -2);
  }

  lua_getfield(L, -1, field.data());
  lua_remove(L, -2);
  return -1; // 返回新表的索引
}

int lua_util::get_field(lua_State* L, const int32_t& idx, const std::string_view& path, const std::string_view& field) {
  lua_pushvalue(L, idx);
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1); // 移除
    return 0;
  }

  // 解析表路径（支持嵌套表如 "EXT.sub"）
  size_t start = 0;
  size_t end = path.find('.');
  std::string current = std::string(path.substr(0, end));

  // 获取根表
  lua_getfield(L, -1, current.c_str());
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1); // 移除
    lua_pop(L, 1); // 移除父表
    return 0;
  }
  lua_remove(L, -2); // 移除父表

  // 处理嵌套表
  while (end != std::string_view::npos) {
    start = end + 1;
    end = path.find('.', start);
    current = std::string(path.substr(start, end - start));

    // 获取子表
    lua_getfield(L, -1, current.data());
    if (!lua_istable(L, -1)) {
      lua_pop(L, 1); // 移除
      return 0;
    }

    // 移除父表，保留子表
    lua_remove(L, -2);
  }

  lua_getfield(L, -1, field.data());
  lua_remove(L, -2);
  return -1; // 返回新表的索引
}

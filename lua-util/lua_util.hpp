#pragma once

#include <lua.hpp>

#include <tuple>
#include <string>
#include <utility>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

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

template<typename... Args, size_t... Is>
void lua_util_extract_args(lua_State* L, std::tuple<Args...>& tuple, std::index_sequence<Is...>) {
  ((std::get<Is>(tuple) = arg<std::decay_t<Args>>::get(L, Is + 1)), ...);
}

template<typename R, typename... Args>
struct lua_func_param_wrapper {
  static int dispatch(lua_State* L) {
    // 检查参数数量
    if (param_cnt() != lua_param_cnt(L)) {
      return luaL_error(L, "wrong number of arguments: expected %d, got %d", sizeof...(Args), lua_gettop(L));
    }

    // 从 upvalue 获取函数指针
    using FuncPtr = R(*)(Args...);
    FuncPtr func = reinterpret_cast<FuncPtr>(lua_touserdata(L, lua_upvalueindex(1)));

    // 提取参数
    auto args = extract_args(L);

    // 调用函数并处理结果
    if constexpr (std::is_same_v<R, void>) {
      std::apply(func, args);
      return 0;
    } else {
      R result = std::apply(func, args);
      arg<R>::push(L, std::forward<R>(result));
      return 1;
    }
  }

  static inline int lua_param_cnt(lua_State* L) { return lua_gettop(L); }
  static inline int param_cnt() { return static_cast<int>(sizeof...(Args)); }
  static inline std::tuple<std::decay_t<Args>...> extract_args(lua_State* L) {
    std::tuple<std::decay_t<Args>...> args;
    lua_util_extract_args(L, args, std::index_sequence_for<Args...>{});
    return args;
  }
};

inline int lua_param_cnt(lua_State* L) { return lua_gettop(L); }
inline int lua_param_type(lua_State* L, int idx) { return lua_type(L, idx); }
inline const char* lua_param_typename(lua_State* L, int idx) { return lua_typename(L, lua_type(L, idx)); }

std::string stack_dump(lua_State* L, int nPreStack);

int create_table(lua_State* L, const std::string_view& tablePath);
int get_global(lua_State* L, const std::string_view& tablePath, const std::string_view& field);
int get_field(lua_State* L, const int32_t& idx, const std::string_view& path, const std::string_view& field);

template<typename R, typename... Args>
void bind(lua_State* L, const std::string_view& name, R(*func)(Args...)) {
  void* ptr = reinterpret_cast<void*>(func);
  lua_pushlightuserdata(L, ptr);
  lua_pushcclosure(L, &lua_func_param_wrapper<R, Args...>::dispatch, 1);
  lua_setglobal(L, name.data());
}

template<typename R, typename... Args>
void bind(lua_State* L, const std::string_view& tablePath, const std::string_view& funcName, R(*func)(Args...)) {
  // 获取目标表（如果不存在则创建）
  int top = lua_gettop(L);
  create_table(L, tablePath);

  // 现在栈顶是目标表
  // 创建函数闭包
  void* ptr = reinterpret_cast<void*>(func);
  lua_pushlightuserdata(L, ptr);
  lua_pushcclosure(L, &lua_func_param_wrapper<R, Args...>::dispatch, 1);
  lua_setfield(L, -2, funcName.data());

  // 清理栈
  lua_settop(L, top);
}

enum class e_lua_type: uint8_t {
  nil = LUA_TNIL,
  boolean = LUA_TBOOLEAN,
  lightUserdata = LUA_TLIGHTUSERDATA,
  number = LUA_TNUMBER,
  string = LUA_TSTRING,
  table = LUA_TTABLE,
  function = LUA_TFUNCTION,
  userdata = LUA_TUSERDATA,
  thread = LUA_TTHREAD,

  lua_type_cnt = LUA_NUMTYPES,
};

typedef void* lua_ref;

struct lua_bind_data {
  std::string_view name;
  int(*func)(lua_State*);
};

class lua_env {
public:
  lua_env();
  ~lua_env();

public:
  lua_env(const lua_env&) = delete;
  lua_env& operator=(const lua_env&) = delete;
  lua_env(lua_env &&other);
  lua_env& operator=(lua_env &&other);

public:
  const char* load(const char* filename);
  const char* load(const char* name, const uint8_t* buffer, size_t size);
  inline void push() { lua_pushvalue(_env, -1); }

public:
  void unref(const lua_ref &ref);
  lua_ref ref(const int32_t &idx);
  lua_ref ref_global(const std::string_view &name);
  bool push(const lua_ref &ref);

  template<typename T>
  T get(const lua_ref& ref) {
    if (!_env) throw std::runtime_error("invalid lua state");
    if (!push(ref)) throw std::runtime_error("invalid ref");
    auto r = arg<T>::get(_env, -1);
    lua_pop(_env, 1);
    return r;
  }

  template<typename T>
  void push(const T& value) { arg<T>::push(_env, value); }

public:
  const char* call();

  template<typename... Args>
  const char* call(const lua_ref func, const Args&... args) {
    if (!_env) throw std::runtime_error("invalid lua state");

    const auto _ref = (lua_ref_inner*)func;
    const auto it = _refs.find(_ref);
    if (it == _refs.end()) return "lua_env: invalid ref";
    if (_ref->typ != e_lua_type::function) return "lua_env: ref is not a function";

    lua_rawgeti(_env, LUA_REGISTRYINDEX, _ref->ref);
    ((arg<Args>::push(_env, args)), ...);

    int cnt = sizeof...(Args);
    int ret = lua_pcall(_env, cnt, 0, 0);
    if (ret) {
      auto r = lua_tostring(_env, -1);;
      lua_pop(_env, 1);
      return r;
    }
    return nullptr;
  }

  template<typename R, typename... Args>
  const char* call(const lua_ref func, R& r, const Args&... args) {
    if (!_env) throw std::runtime_error("invalid lua state");

    const auto _ref = (lua_ref_inner*)func;
    const auto it = _refs.find(_ref);
    if (it == _refs.end()) return "lua_env: invalid ref";
    if (_ref->typ != e_lua_type::function) return "lua_env: ref is not a function";

    lua_rawgeti(_env, LUA_REGISTRYINDEX, _ref->ref);
    ((arg<Args>::push(_env, args)), ...);

    int ret = lua_pcall(_env, sizeof...(Args), 1, 0);
    if (ret) {
      auto r = lua_tostring(_env, -1);
      lua_pop(_env, 1);
      return r;
    }
    r = arg<R>::get(_env, -1);
    lua_pop(_env, 1);
    return nullptr;
  }

  template<typename... Args>
  const char* call(const lua_ref func, lua_ref& r, Args&&... args) {
    if (!_env) throw std::runtime_error("invalid lua state");

    const auto _ref = (lua_ref_inner*)func;
    const auto it = _refs.find(_ref);
    if (it == _refs.end()) return "lua_env: invalid ref";
    if (_ref->typ != e_lua_type::function) return "lua_env: ref is not a function";

    lua_rawgeti(_env, LUA_REGISTRYINDEX, _ref->ref);
    (arg<Args...>::push(_env, std::forward<Args>(args)...));

    int ret = lua_pcall(_env, sizeof...(Args), 1, 0);
    if (ret) {
      auto r = lua_tostring(_env, -1);
      lua_pop(_env, 1);
      return r;
    }
    r = ref(-1);
    lua_pop(_env, 1);
    return nullptr;
  }

  inline void bind(const std::string_view &name, int(*func)(lua_State*)) {
    if (!_env) throw std::runtime_error("invalid lua state");
    lua_pushcfunction(_env, func);
    lua_setglobal(_env, name.data());
  }

  template<typename Arr>
  void bind(const Arr& arr)
  requires std::is_same_v<typename Arr::value_type, lua_bind_data> &&
  std::is_base_of<std::forward_iterator_tag, typename Arr::iterator::iterator_category>::value
  {
    if (!_env) throw std::runtime_error("invalid lua state");
    for (auto it = arr.begin(); it != arr.end(); ++it) {
      const auto &f = *it;
      bind(f.name, f.func);
    }
  }

  template<typename Arr>
  void bind(const std::string_view& tablePath, const Arr& arr)
  requires std::is_same_v<typename Arr::value_type, lua_bind_data> &&
  std::is_base_of<std::forward_iterator_tag, typename Arr::iterator::iterator_category>::value
  {
    if (!_env) throw std::runtime_error("invalid lua state");

    // 获取目标表（如果不存在则创建）
    int top = lua_gettop(_env);
    create_table(_env, tablePath);

    // 清理栈
    for (auto it = arr.begin(); it != arr.end(); ++it) {
      const auto &f = *it;
      lua_pushcfunction(_env, f.func);
      lua_setfield(_env, -2, f.name.data());
    }
    lua_settop(_env, top);
  }

  template<typename R, typename... Args>
  void bind(const std::string_view &tablePath, const std::string_view &funcName, R(*func)(Args...)) {
    if (!_env) throw std::runtime_error("invalid lua state");
    lua_util::bind(_env, tablePath, funcName, func);
  }

  template<typename R, typename... Args>
  void bind(const std::string_view &name, R(*func)(Args...)) {
    if (!_env) throw std::runtime_error("invalid lua state");
    lua_util::bind(_env, name, func);
  }

public:
  std::string stack_dump(int nPreStack);
  inline lua_State* env() { return _env; }

private:
  struct lua_ref_inner {
    int ref;
    e_lua_type typ;
  };

  lua_State *_env;
  std::unordered_set<lua_ref_inner*> _refs;
};

} // namespace lua_util

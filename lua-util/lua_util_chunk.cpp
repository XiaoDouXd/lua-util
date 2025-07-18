#include <lua.hpp>

#include <fstream>
#include <algorithm>
#include <functional>

#include "lua_util_chunk.h"

constexpr size_t SIGN_BIT = (size_t)1 << (sizeof(size_t)*8 - 1);

std::unique_ptr<lua_util::id_tree> lua_util::id_tree::deserialize(size_t* nodes, size_t node_count) {
  using ParseResult = std::pair<id_tree*, size_t>;
  std::function<ParseResult(size_t*, size_t*)> parse = [&](size_t* begin, size_t* end) -> ParseResult {
    if (begin >= end) return {nullptr, 0};

    size_t id = *begin;
    size_t* ptr = begin + 1;

    size_t child_count = 0;
    size_t data = id_tree::NULL_DATA;

    // 根据最高位确定数据结构
    if (id & SIGN_BIT) {
      if (ptr + 1 >= end) return {nullptr, 1}; // 数据不足
      data = *ptr++;
      child_count = *ptr++;
    } else {
      if (ptr >= end) return {nullptr, 1}; // 数据不足
      child_count = *ptr++;
    }

    auto children = std::vector<id_tree*>(child_count);
    size_t total_consumed = ptr - begin;

    // 递归解析子节点
    for (size_t i = 0; i < child_count; ++i) {
      size_t* new_pos = begin + total_consumed;
      auto [child, consumed] = parse(new_pos, end);
      if (!child) {
        for (size_t j = 0; j < i; ++j) delete children[j];
        return {nullptr, total_consumed + consumed};
      }
      children[i] = child;
      total_consumed += consumed;
    }

    // 创建节点
    id_tree* n = new lua_util::id_tree(id & ~SIGN_BIT, data, std::move(children));

    // 排序节点
    std::sort(n->_children.begin(), n->_children.end(),
      [](id_tree* a, id_tree* b) { return a->id() < b->id(); });
    return {n, total_consumed};
  };

  // 解析节点
  auto [root, consumed] = parse(nodes, nodes + node_count);
  return (consumed == node_count) ? std::unique_ptr<id_tree>(root) : nullptr;
}

void lua_util::id_tree::serialize(const id_tree& node, std::vector<size_t>& output) {
  if (node._data != id_tree::NULL_DATA) {
    output.push_back(node.id() | SIGN_BIT);
    output.push_back(node._data);
  } else output.push_back(node.id());
  output.push_back(node._children.size());

  // 序列化子节点（已排序）
  for (size_t i = 0; i < node._children.size(); ++i)
    serialize(node.get_child(i), output);
}

const int32_t lua_util::id_tree::find(size_t id) const {
  int32_t left = 0, right = _children.size() - 1;
  while (left <= right) {
    int32_t mid = (left + right) / 2;
    auto child = _children[mid];
    if (child->id() == id) return mid;
    if (child->id() < id) left = mid + 1;
    else right = mid - 1;
  }
  return id_tree::NULL_IDX;
}

int32_t lua_util::id_tree::push(id_tree&& other) {
  size_t idx = find(other.id());
  if (idx != NULL_DATA) return idx;

  // 二分查找的有序插入
  int32_t left = 0, right = _children.size() - 1;
  while (left <= right) {
    int32_t mid = (left + right) / 2;
    auto child = _children[mid];
    if (child->id() < other.id()) left = mid + 1;
    else right = mid - 1;
  }
  _children.insert(_children.begin() + left, new id_tree(std::move(other)));
  return left;
}

int32_t lua_util::id_tree::push(size_t id, size_t data) {
  return push(id_tree(id, data));
}

lua_util::id_tree::id_tree(id_tree&& other) {
  _id = other._id;
  _data = other._data;
  _children = std::move(other._children);

  other._children = {};
}

lua_util::id_tree& lua_util::id_tree::operator=(id_tree&& other) {
  _id = other._id;
  _data = other._data;
  _children = std::move(other._children);
  other._children = {};
  return *this;
}

lua_util::id_tree::id_tree(size_t id, size_t data) : _id(id), _data(data) {
  if (id & SIGN_BIT) throw std::logic_error("id must not be negative");
}

lua_util::id_tree::id_tree(size_t id, size_t data, std::vector<id_tree*>&& children)
  : _id(id), _data(data), _children(children) {}

lua_util::id_tree::~id_tree() {
  for (size_t i = 0; i < _children.size(); i++) {
    auto child = _children[i];
    if (child) delete child;
  }
}

void lua_util::path_part_collection::add_part(const std::string_view &path_part,
                                              size_t id) {
  if (path_part.empty())
    throw std::runtime_error("empty path");
  if (path_part == CURR_DIR || path_part == PARENT_DIR)
    throw std::runtime_error("invalid path: current dir or parent dir");
  if (path_part.find('/') != path_part.npos ||
      path_part.find('\\') != path_part.npos)
    throw std::runtime_error("invalid path: contains separator");
  _map[path_part] = id;
}

std::vector<size_t>
lua_util::path_part_collection::to_ids(const std::string_view &path) const {
  std::vector<size_t> result;
  enumerate_path(path, [&](const std::string_view &path_part) {
    if (path_part == CURR_DIR) return;
    if (path_part == PARENT_DIR) {
      if (result.empty())
        throw std::runtime_error("invalid path: parent dir at root");
      result.pop_back();
      return;
    }
    auto it = _map.find(path_part);
    if (it == _map.end())
      throw std::runtime_error("invalid path: path part not found");
    result.push_back(it->second);
  });
  return result;
}

void lua_util::id_tree::for_each_child(
    const std::function<void(const id_tree &children)> &func) const {
  func(*this);
  for (size_t i = 0; i < _children.size(); i++)
    for_each_child(func);
}

lua_util::chunk::chunk(): _buffer(nullptr), _buffer_size(0) {}

lua_util::chunk::chunk(const std::string_view &filename): chunk() {
  std::ifstream file(filename.data(), std::ios::binary | std::ios::ate);
  if (!file.is_open()) throw std::runtime_error("failed to open file");
  _buffer_size = file.tellg();
  file.seekg(0, std::ios::beg);
  if (!_buffer_size) return;

  _buffer = new uint8_t[_buffer_size];

  if (!file.read((char*)_buffer, _buffer_size))
    throw std::runtime_error("failed to read file");

  build_buffer_map();
}

lua_util::chunk::chunk(uint8_t *buffer, size_t buffer_size) {
  _buffer = buffer;
  _buffer_size = buffer_size;
  build_buffer_map();
}

lua_util::chunk::chunk(const std::span<uint8_t> &buffer) {
  _buffer = buffer.data();
  _buffer_size = buffer.size();
  build_buffer_map();
}

lua_util::chunk::chunk(chunk&& other) {
  _buffer = other._buffer;
  _buffer_size = other._buffer_size;
  _data_map = std::move(other._data_map);

  other._buffer = nullptr;
  other._buffer_size = 0;
  other._data_map = {};
}

lua_util::chunk &lua_util::chunk::operator=(chunk &&other) {
  _buffer = other._buffer;
  _buffer_size = other._buffer_size;
  _data_map = std::move(other._data_map);

  other._buffer = nullptr;
  other._buffer_size = 0;
  other._data_map = {};
  return *this;
}

lua_util::chunk::~chunk() {
  if (!_buffer) return;

  _data_map = {};
  _buffer_size = 0;
  if (_buffer) delete[] _buffer;
}

void lua_util::chunk::build_buffer_map() {
  if (!_buffer || !_buffer_size) return;

  // read header
  const auto sp = std::span<uint8_t>(_buffer, _buffer_size);
  auto chunk_count = read_bytes<uint64_t>(sp, 0);
  auto header = std::vector<std::pair<uint64_t, uint64_t>>();
  for (size_t i = 0; i < chunk_count; i++) {
    const auto offset = sizeof(uint64_t) + i * sizeof(uint64_t) * 2;
    const auto id = read_bytes<uint64_t>(sp, offset);
    const auto size = read_bytes<uint64_t>(sp, offset + sizeof(uint64_t));
    header.emplace_back(id, size);
  }

  // build map
  const auto chunk_offset = sizeof(uint64_t) + chunk_count * sizeof(uint64_t) * 2;
  auto offset = chunk_offset;
  for (size_t i = 0; i < chunk_count; i++) {
    const auto [id, size] = header[i];
    _data_map[id] = std::span<uint8_t>(_buffer + offset, size);
    offset += size;
  }
}

std::vector<uint8_t> lua_util::chunk::build_chunk_buffer(
    std::unordered_map<uint64_t, std::span<uint8_t>> &chunks) {
  // write header
  const uint64_t chunk_count = chunks.size();
  auto result = std::vector<uint8_t>();

  write_bytes(result, chunk_count);
  for (auto [id, chunk] : chunks) {
    write_bytes(result, (uint64_t)id);
    write_bytes(result, (uint64_t)chunk.size());
  }

  // write chunks
  for (auto [id, chunk] : chunks)
    result.insert(result.end(), chunk.begin(), chunk.end());
  return result;
}

lua_util::chunk lua_util::chunk::build_chunk(
    std::unordered_map<uint64_t, std::span<uint8_t>> &chunks) {
  auto buffer = build_chunk_buffer(chunks);
  auto copy = new uint8_t[buffer.size()];
  std::copy(buffer.begin(), buffer.end(), copy);
  return chunk(copy, buffer.size());
}

int lua_util::lua_custom_requirer::require(lua_State *L) {
  const char* module_name = luaL_checkstring(L, 1);
  auto ids = lua_src_path_collection.to_ids(module_name);

  auto node_idx = lua_src_tree.find(ids);
  if (node_idx == lua_util::id_tree::NULL_IDX) {
    return luaL_error(L, "module not found: %s, tree_node not found", module_name);
  }

  auto& tree_node = lua_src_tree.get_child(node_idx);
  auto chunk = lua_src_chunk.get(tree_node.id());
  if (chunk.empty()) {
    return luaL_error(L, "module not found: %s, chunk empty", module_name);
  }

  const auto ret = luaL_loadbuffer(L, (const char*)chunk.data(), chunk.size(), module_name);
  if (ret) {
    return luaL_error(L, "module not found: %s, load buffer error: %s", module_name, lua_tostring(L, -1));
  }
  return 1;
}

void lua_util::lua_custom_requirer::register_requirer(lua_State *L, int(*requirer)(lua_State*)) {
  // 1. 获取 package.searchers 表 (Lua 5.1 使用 package.loaders)
  lua_getglobal(L, "package");

  #if LUA_VERSION_NUM >= 502
      lua_getfield(L, -1, "searchers");
  #else
      lua_getfield(L, -1, "loaders");
  #endif

  // 2. 在加载器列表最前面插入自定义加载器
  int searchers_idx = lua_gettop(L);
  int len = lua_rawlen(L, searchers_idx);

  // 将现有加载器后移一位
  for (int i = len; i >= 1; i--) {
      lua_rawgeti(L, searchers_idx, i);
      lua_rawseti(L, searchers_idx, i + 1);
  }

  // 插入自定义加载器为第一个
  lua_pushcfunction(L, requirer);
  lua_rawseti(L, searchers_idx, 1);

  // 3. 清理栈
  lua_pop(L, 2); // 弹出 package.searchers 和 package
}

void lua_util::path_part_collection::enumerate_path(
    const std::string_view &path,
    std::function<void(const std::string_view &)> func) {
  const auto end = path.size();

  // call current dir
  func(CURR_DIR);
  for (size_t i = 0; i < end;) {
    // skip separators
    while (i < end && is_path_separator(path[i])) i++;

    // find the next separator
    size_t j = i;
    while (j < end && !is_path_separator(path[j])) j++;

    // get the part
    const auto part = path.substr(i, j - i);
    i = j;

    // call func
    // if part is current dir, skip it
    if (part != CURR_DIR) {
      if (part == PARENT_DIR) func(PARENT_DIR);
      else func(part);
    }
  }
}

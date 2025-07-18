#include <bit>
#include <span>
#include <memory>
#include <vector>
#include <stdexcept>
#include <functional>
#include <unordered_map>

struct lua_State;

namespace lua_util {

/// write bytes to vector
/// @tparam T: the type of the value to write, must be unsigned
/// @param target: the vector to write to
/// @param value: the value to write
template<typename T>
void write_bytes(std::vector<uint8_t>& target, T value) {
  static_assert(std::is_unsigned_v<T>, "must be unsigned");

  if constexpr (std::endian::native == std::endian::big) {
    for (int i = sizeof(T)-1; i >= 0; i--)
      target.push_back((value >> (i * 8)) & 0xff);
  } else {
    // 小端序反转字节序
    for (int i = 0; i < sizeof(T); i++)
      target.push_back((value >> (i * 8)) & 0xff);
  }
}

/// read bytes from vector
/// @tparam T: the type of the value to read, must be unsigned
/// @param data: the vector to read from
/// @param offset: the offset to read from
/// @return the value read
template<typename T>
T read_bytes(const std::span<uint8_t>& data, int offset = 0) {
  static_assert(std::is_unsigned_v<T>, "must be unsigned");

  T value = 0;
  if constexpr (std::endian::native == std::endian::big) {
    const int end = offset + sizeof(T);
    for (int i = end-1; i >= offset; --i)
      value |= (T)data[i] << (i * 8);
  } else {
    // 小端序反转字节序
    const int end = offset + sizeof(T);
    for (int i = offset; i < end; ++i)
      value |= (T)data[i] << (i * 8);
  }
  return value;
}

/// convert a value to bytes
/// @tparam T: the type of the value to convert
/// @param value: the value to convert
/// @return the bytes of the value
template<typename T>
std::vector<uint8_t> to_bytes(T* value, size_t count) {
  std::vector<uint8_t> result;
  for (size_t i = 0; i < count; i++) write_bytes(result, value[i]);
  return result;
}

/// convert bytes to a value
/// @tparam T: the type of the value to convert
/// @param data: the bytes to convert
/// @param count: the count of the bytes
/// @return the value
template<typename T>
std::vector<T> from_bytes(uint8_t* data, size_t count) {
  std::vector<T> result;
  count = count - count % sizeof(T);

  const auto sp = std::span<uint8_t>(data, count);
  for (size_t i = 0; i < count; i += sizeof(T))
    result.push_back(read_bytes<T>(sp, i));
  return result;
}

/// id tree
/// id_tree is a tree that each node has a id and a data
class id_tree {
public:
  /// id tree deserializer
  /// @param nodes: array of nodes
  ///     as a node:
  ///     1. the first size_t is the id of the node.
  ///     2. if the first bit of id is 1, the second size_t is the data of the node,
  ///        and the third size_t is the count of children of the node.
  ///        otherwise the second size_t is the count of children of the node.
  ///     3. the next size_t * count of children is the id of children...
  /// @param node_count: the count of nodes
  static std::unique_ptr<id_tree> deserialize(size_t* nodes, size_t node_count);

  /// id tree serializer
  /// @param tree: the tree to serialize
  /// @param output: the output vector
  static void serialize(const id_tree& tree, std::vector<size_t>& output);

public:
  /// the null data
  static constexpr size_t NULL_DATA = SIZE_MAX;
  /// the null index
  static constexpr int32_t NULL_IDX = -1;

  /// find the child with the given id
  /// @param id: the id of the child
  /// @return the index of the child in the children array, or id_tree::NULL_DATA if not found
  const int32_t find(size_t id) const;

  /// find the child with the given id
  /// @param ids: the ids of the child
  /// @return the index of the child in the children array, or id_tree::NULL_DATA if not found
  template<typename T>
  const int32_t find(const T &ids) const
  requires std::is_same_v<typename T::value_type, size_t> {
    if (ids.empty()) return NULL_IDX;
    auto idx = find(*ids.begin());
    if (idx == NULL_IDX) return idx;

    auto* child = &get_child(idx);
    for (auto i = ids.begin() + 1; i < ids.end(); i++) {
      idx = child->find(*i);
      if (idx == NULL_IDX) return NULL_IDX;
      if (i + 1 == ids.end()) return idx;
      child = &child->get_child(idx);
    }
    return idx;
  }

  /// check if the index is valid
  /// @param idx: the index of the child
  inline const bool valid_idx(int32_t idx) const { return idx >= 0 && idx < _children.size(); }

  /// get the child with the given index
  /// @param idx: the index of the child
  /// @return the child with the given index
  /// @throw std::out_of_range if the index is invalid
  inline id_tree& get_child(int32_t idx) const {
    if (!valid_idx(idx)) throw std::out_of_range("Invalid index");
    return *_children[idx];
  }

  const size_t id() const { return _id; }
  const size_t data() const { return _data; }

  int32_t push(id_tree&& other);
  int32_t push(size_t id, size_t data = id_tree::NULL_DATA);

  /// for each child
  /// @param func: the function to call for each child
  void for_each_child(
      const std::function<void(const id_tree &children)> &func) const;

public:
  ~id_tree();
  id_tree(): _id(0), _data(NULL_DATA) {}
  id_tree(size_t id, size_t data = id_tree::NULL_DATA);

private:
  id_tree(id_tree&& other);
  id_tree& operator=(id_tree&& other);

  id_tree(id_tree& other) = delete;
  id_tree& operator=(id_tree& other) = delete;
  id_tree(size_t id, size_t data, std::vector<id_tree*>&& children);

  size_t _id = 0;
  size_t _data = NULL_DATA;

  std::vector<id_tree*> _children;
};

/// chunk
/// [header] [chunk1] [chunk2] ...
/// header: [chunk_count(uint64)]
///         [chunk1_id(uint64)] [chunk1_size(uint64)]
///         [chunk2_id]         [chunk2_size]
///         [chunk3_id]         [chunk3_size]
///         ...
/// chunk:  [data]
class chunk {
public:
  /// build a chunk from a buffer map
  /// @param chunks: the chunks to build
  /// @return the chunk
  static chunk
  build_chunk(std::unordered_map<uint64_t, std::span<uint8_t>> &chunks);

  /// build a chunk buffer from a buffer map
  /// @param chunks: the chunks to build
  /// @return the buffer
  static std::vector<uint8_t>
  build_chunk_buffer(std::unordered_map<uint64_t, std::span<uint8_t>> &chunks);

public:
  chunk();
  chunk(const std::string_view &filename);
  chunk(const std::span<uint8_t> &buffer);
  chunk(uint8_t *buffer, size_t buffer_size);
  ~chunk();

  chunk(chunk&& other);
  chunk &operator=(chunk &&other);

  chunk(const chunk&) = delete;
  chunk& operator=(const chunk&) = delete;

  /// get a chunk by id
  /// @param id: the id of the chunk
  /// @return the chunk
  inline const std::span<uint8_t> get(size_t id) const {
    if (!_buffer) return {};
    if (!_data_map.contains(id)) return {};
    return _data_map.at(id);
  }

  /// get the raw buffer
  inline const std::span<uint8_t> get_raw() const { return { _buffer, _buffer_size }; }

private:
  void build_buffer_map(); // only call by constructor

  uint8_t* _buffer;
  size_t _buffer_size;
  std::unordered_map<size_t, std::span<uint8_t>> _data_map;
};

/// path part collection
/// path_part_collection is a collection of path parts and their ids
class path_part_collection {
public:
  constexpr static const char* CURR_DIR = ".";
  constexpr static const char* PARENT_DIR = "..";
  constexpr static bool is_path_separator(char c) { return c == '/' || c == '\\' || c == ':'; }

  /// enumerate a path
  /// @param path: the path to enumerate
  /// @param func: the function to call for each path part
  static void
  enumerate_path(const std::string_view &path, std::function<void(const std::string_view &)> func);

public:
  /// add a path part
  /// @param path_part: the path part to add
  /// @param id: the id of the path part
  void add_part(const std::string_view &path_part, size_t id);

  /// convert a path to ids
  /// @param path: the path to convert
  /// @return the ids of the path
  std::vector<size_t> to_ids(const std::string_view &path) const;

private:
  std::unordered_map<std::string_view, size_t> _map;
};

class lua_custom_requirer {
public:
  static void register_requirer(lua_State *L, int(*requirer)(lua_State*));

public:
  id_tree lua_src_tree;
  chunk lua_src_chunk;
  path_part_collection lua_src_path_collection;

public:
  int require(lua_State *L);
};

}

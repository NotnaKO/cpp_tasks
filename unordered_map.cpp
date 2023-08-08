#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

template <typename T, typename Allocator = std::allocator<T>>
class List {
 private:
  struct BaseNode;

  struct Node;

 public:
  template <bool is_const>
  struct Iterator;

  using value_type = T;
  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  size_t size() const noexcept {
    return size_;
  }

  auto get_allocator() const noexcept {
    return alloc_;
  }

  template <typename... Args>
  void emplace(const_iterator iter, Args&&... args);

  inline void insert(const_iterator iter, const T& value) {
    emplace(iter, value);
  }

  void erase(const_iterator iter) noexcept;

  iterator end() {
    return &fake_node_;
  }

  const_iterator end() const {
    return iterator(fake_node_.next->prev);
  }

  const_iterator cend() const {
    return iterator(fake_node_.next->prev);
  }

  iterator begin() {
    return fake_node_.next;
  }

  const_iterator begin() const {
    return ++end();
  }

  const_iterator cbegin() const {
    return ++end();
  }

  reverse_iterator rend() {
    return begin();
  }

  const_reverse_iterator rend() const {
    return cbegin();
  }

  reverse_iterator rbegin() {
    return reverse_iterator(end());
  }

  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(cend());
  }

  inline void push_back(const T& value) {
    insert(end(), value);
  }

  inline void push_front(const T& value) {
    insert(begin(), value);
  }

  template <typename... Args>
  inline void emplace_back(Args&&... args) {
    emplace(end(), std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void emplace_front(Args&&... args) {
    emplace(begin(), std::forward<Args>(args)...);
  }

  template <typename U, typename... Args>
  U* construct_with_allocator(Args&&... args) const {
    auto new_alloc = typename AllocTraits ::template rebind_alloc<U>(alloc_);
    U* ptr = AllocTraits ::template rebind_traits<U>::allocate(new_alloc, 1);
    AllocTraits ::template rebind_traits<U>::construct(
        new_alloc, ptr, std::forward<Args>(args)...);
    return ptr;
  }

  template <typename U>
  void destroy_with_allocator(U* ptr) const {
    auto new_alloc = typename AllocTraits ::template rebind_alloc<U>(alloc_);
    AllocTraits ::template rebind_traits<U>::destroy(new_alloc, ptr);
    AllocTraits ::template rebind_traits<U>::deallocate(new_alloc, ptr, 1);
  }

  inline void pop_back() noexcept {
    erase(--end());
  }

  inline void pop_front() noexcept {
    erase(begin());
  }

  explicit List(const Allocator& allocator = Allocator()) noexcept
      : alloc_(allocator),
        node_alloc_(alloc_),
        size_(0){};

  List(List&& list)
      : List(AllocTraits::select_on_container_copy_construction(
            list.get_allocator())) {
    for (auto&& item : list) {
      emplace_back(std::move(item));
    }
    list.clear();
  }

  List& operator=(List&& list) {
    List&& copy = std::move(list);
    std::swap(*this, copy);
    return *this;
  }

  void insert(const_iterator iter, Node* new_node) {
    BaseNode* current = iter.node;
    BaseNode* previous = (--iter).node;
    previous->next = new_node;
    current->prev = new_node;
    ++size_;
  }

  List(size_t count, const T& value = T(),
       const Allocator& allocator = Allocator());

  explicit List(size_t count, const Allocator& allocator);

  List(const List& list);

  ~List() {
    clear();
  }

  bool operator==(const List& list) const;

  List& operator=(const List& list);

  bool is_empty() const {
    return size_ == 0;
  }

  void clear() noexcept;

 private:
  using NodeAlloc =
      typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
  using NodeTraits = std::allocator_traits<NodeAlloc>;
  using AllocTraits = std::allocator_traits<Allocator>;

  [[no_unique_address]] Allocator alloc_;
  [[no_unique_address]] NodeAlloc node_alloc_;
  BaseNode fake_node_;
  size_t size_;
};

template <typename T, typename Allocator>
void List<T, Allocator>::clear() noexcept {
  while (!is_empty()) {
    pop_back();
  }
}

template <typename T, typename Allocator>
List<T, Allocator>& List<T, Allocator>::operator=(const List& list) {
  if (this != &list) {
    if constexpr (!AllocTraits::propagate_on_container_copy_assignment::value) {
      Allocator copy =
          AllocTraits ::select_on_container_copy_construction(list.alloc_);
      std::swap(copy, alloc_);
      node_alloc_ = NodeAlloc(alloc_);
    } else {
      alloc_ = list.alloc_;
    }
    size_t old_size = size();
    try {
      for (const auto& item : list) {
        push_back(item);
      }
    } catch (...) {
      while (size_ > old_size) {
        pop_back();
      }
    }
    while (size_ > list.size_) {
      pop_front();
    }
  }
  return *this;
}

template <typename T, typename Allocator>
bool List<T, Allocator>::operator==(const List& list) const {
  if (list.size() != size()) {
    return false;
  }
  const_iterator it_1 = cbegin();
  const_iterator it_2 = list.cbegin();
  for (; it_1 != cend() && it_2 != list.cend(); ++it_1, ++it_1) {
    if (*it_1 != *it_2) {
      return false;
    }
  }
  return true;
}

template <typename T, typename Allocator>
List<T, Allocator>::List(const List& list)
    : List(AllocTraits ::select_on_container_copy_construction(list.alloc_)) {
  try {
    for (const auto& item : list) {
      push_back(item);
    }
  } catch (...) {
    clear();
  }
}

template <typename T, typename Allocator>
List<T, Allocator>::List(size_t count, const Allocator& allocator)
    : List(allocator) {
  for (size_t i = 0; i < count; ++i) {
    emplace(end());
  }
}

template <typename T, typename Allocator>
List<T, Allocator>::List(size_t count, const T& value,
                         const Allocator& allocator)
    : List(allocator) {
  for (size_t i = 0; i < count; ++i) {
    push_back(value);
  }
}

template <typename T, typename Allocator>
void List<T, Allocator>::erase(List::const_iterator iter) noexcept {
  Node& current = static_cast<Node&>(*iter.node);
  BaseNode* next = current.next;
  BaseNode* prev = current.prev;
  NodeTraits::destroy(node_alloc_, &current);
  NodeTraits::deallocate(node_alloc_, &current, 1);
  prev->next = next;
  next->prev = prev;
  --size_;
}

template <typename T, typename Allocator>
template <typename... Args>
void List<T, Allocator>::emplace(List::const_iterator iter, Args&&... args) {
  BaseNode* current = iter.node;
  BaseNode* previous = (--iter).node;
  Node* new_node = NodeTraits::allocate(node_alloc_, 1);
  try {
    NodeTraits::construct(node_alloc_, new_node, previous, current, alloc_,
                          std::forward<Args>(args)...);
  } catch (...) {
    NodeTraits::deallocate(node_alloc_, new_node, 1);
    throw;
  }
  previous->next = new_node;
  current->prev = new_node;
  ++size_;
}

template <typename T, typename Allocator>
struct List<T, Allocator>::BaseNode {
  BaseNode(BaseNode* prev, BaseNode* next) noexcept
      : next(next),
        prev(prev) {
  }

  BaseNode() noexcept
      : BaseNode(this, this) {
  }

  ~BaseNode() = default;

  BaseNode* next;
  BaseNode* prev;
};

template <typename T, typename Allocator>
struct List<T, Allocator>::Node : public BaseNode {
  using value_type = T;
  using pointer = value_type*;

  template <typename... Args>
  Node(BaseNode* prev, BaseNode* next, Allocator allocator_1, Args&&... args)
      : BaseNode(prev, next),
        alloc(allocator_1),
        value(AllocTraits::allocate(alloc, 1)) {
    AllocTraits ::construct(alloc, value, std::forward<Args>(args)...);
  }

  ~Node() {
    AllocTraits ::destroy(alloc, value);
    AllocTraits ::deallocate(alloc, value, 1);
  }

  pointer get_value() {
    return value;
  }

  Allocator alloc;
  pointer value;
};

template <typename T, typename Allocator>
template <bool is_const>
struct List<T, Allocator>::Iterator {
  using value_type = std::conditional_t<is_const, const T, T>;
  using pointer = value_type*;
  using reference = value_type&;
  using difference_type = int;
  using iterator_category = std::bidirectional_iterator_tag;

  using NodeType = BaseNode*;

  NodeType node;

  operator Iterator<true>() const {
    return {node};
  }

  Iterator(NodeType node)
      : node(node) {
  }

  Iterator& operator++() {
    node = node->next;
    return *this;
  }

  Iterator& operator--() {
    node = node->prev;
    return *this;
  }

  Iterator operator++(int) {
    auto copy = *this;
    operator++();
    return copy;
  }

  Iterator operator--(int) {
    auto copy = *this;
    operator--();
    return copy;
  }

  pointer operator->() {
    return static_cast<Node*>(node)->get_value();
  }

  reference operator*() {
    return *static_cast<Node*>(node)->get_value();
  }

  bool operator==(Iterator iterator) const noexcept {
    return node == iterator.node;
  }
};

template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename Equal = std::equal_to<Key>,
          typename Alloc = std::allocator<std::pair<const Key, Value>>>
class UnorderedMap {
 public:
  using NodeType = std::pair<const Key, Value>;

 private:
  // struct Handler;

  template <bool is_const>
  struct Iterator;

 public:
  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;

  UnorderedMap(Alloc alloc = Alloc())
      : max_factor_(default_factor),
        values_(alloc) {
  }

  UnorderedMap(const UnorderedMap&) = default;

  UnorderedMap(UnorderedMap&& map);

  UnorderedMap& operator=(UnorderedMap&& map);

  UnorderedMap& operator=(const UnorderedMap&) = default;

  ~UnorderedMap() = default;

  inline size_t size() const noexcept {
    return values_.size();
  }

  inline auto get_allocator() const noexcept {
    return Alloc(values_.get_allocator());
  }

  inline size_t bucket_count() const noexcept {
    return buckets_.size();
  }

  inline auto load_factor() const noexcept {
    return static_cast<double>(size()) / bucket_count();
  }

  inline auto max_load_factor() const noexcept {
    return max_factor_;
  }

  inline void max_load_factor(double new_factor) noexcept {
    max_factor_ = new_factor;
  }

  inline iterator begin() noexcept {
    return values_.begin();
  }

  inline const_iterator begin() const noexcept {
    return values_.begin();
  }

  inline const_iterator cbegin() const noexcept {
    return values_.begin();
  }

  inline iterator end() noexcept {
    return values_.end();
  }

  inline const_iterator end() const noexcept {
    return values_.end();
  }

  inline const_iterator cend() const noexcept {
    return values_.end();
  }

  Value& at(const Key& key);

  Value at(const Key& key) const {
    const_iterator result = find(key);
    if (result == cend()) {
      throw std::out_of_range("no such element");
    }
    return *result;
  }

  void rehash(size_t count);

  void reserve(size_t count) {
    rehash(std::ceil(count / max_load_factor()));
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args);

  std::pair<iterator, bool> insert(const NodeType& node) {
    return emplace(node.first, node.second);
  }

  std::pair<iterator, bool> insert(NodeType&& node) {
    return emplace(const_cast<Key&&>(std::move(node.first)),
                   std::move(node.second));
  }

  template <typename InputIt>
  void insert(InputIt begin, InputIt end);

  const_iterator find(const Key& key) const;

  iterator find(const Key& key);

  bool empty() const noexcept {
    return size() == 0;
  }

  Value& operator[](const Key& key);

  Value& operator[](Key&& key) {
    return (*emplace(std::move(key), Value()).first).second;
  }

  iterator erase(const_iterator iter);

  iterator erase(const_iterator first, const_iterator second);

 private:
  inline const_iterator get_iter(const Key& key) const {
    return buckets_[get_hash(key)];
  }

  inline iterator get_iter(const Key& key) {
    return buckets_[get_hash(key)];
  }

  inline size_t get_hash(const Key& key) const {
    return hash_(key) % bucket_count();
  }

  inline bool equal(const Key& first, const Key& second) const {
    return equal_(first, second);
  }

  double max_factor_;
  constexpr static double default_factor = 0.75;
  constexpr static double growing_coefficient = 2;
  constexpr static size_t default_start_size = 16;
  [[no_unique_address]] Hash hash_;
  [[no_unique_address]] Equal equal_;
  List<NodeType, Alloc> values_;
  std::vector<typename List<NodeType, Alloc>::iterator> buckets_;
};

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Alloc>
Value& UnorderedMap<Key, Value, Hash, Equal, Alloc>::at(const Key& key) {
  iterator result = find(key);
  if (result == end()) {
    throw std::out_of_range("no such element");
  }
  return result->second;
}

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Alloc>
void UnorderedMap<Key, Value, Hash, Equal, Alloc>::rehash(size_t count) {
  count = std::max(count, static_cast<size_t>(size() / max_load_factor()));
  if (count == bucket_count()) {
    return;
  }
  List<NodeType, Alloc> copy(values_.get_allocator());
  while (!values_.is_empty()) {
    auto& item = *values_.begin();
    copy.emplace_back(const_cast<Key&&>(std::move(item.first)),
                      std::move(item.second));
    values_.pop_front();
  }
  buckets_.assign(count, values_.end());
  while (!copy.is_empty()) {
    auto&& item = *copy.begin();
    insert(std::move(item));
    copy.pop_front();
  }
}

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Alloc>
template <typename InputIt>
void UnorderedMap<Key, Value, Hash, Equal, Alloc>::insert(InputIt begin,
                                                          InputIt end) {
  for (auto it = begin; it != end; ++it) {
    insert(*it);
  }
}

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::const_iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::find(const Key& key) const {
  auto hash = get_hash(key);
  const_iterator begin = get_iter(key);
  const_iterator end = (hash != buckets_.size() - 1)
                           ? const_iterator(buckets_[hash + 1])
                           : cend();
  for (; begin != end; ++begin) {
    if (equal(begin->first, key)) {
      return begin;
    }
  }
  return cend();
}

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Alloc>
template <typename... Args>
std::pair<typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator, bool>
UnorderedMap<Key, Value, Hash, Equal, Alloc>::emplace(Args&&... args) {
  if (size() + 1 > static_cast<size_t>(bucket_count() * max_load_factor())) {
    reserve(std::max(default_start_size, static_cast<size_t>(std::ceil(
                                             growing_coefficient * size()))));
  }
  std::pair<Key, Value>* node =
      values_.template construct_with_allocator<std::pair<Key, Value>>(
          std::forward<Args>(args)...);
  iterator iter = find(node->first);
  if (iter != end()) {
    values_.destroy_with_allocator(node);
    return {iter, false};
  }
  iter = get_iter(node->first);
  auto hash = get_hash(node->first);
  if (iter == end()) {
    values_.emplace_front(std::move(node->first), std::move(node->second));
    buckets_[hash] = values_.begin();
    values_.destroy_with_allocator(node);
    return {begin(), true};
  }
  values_.emplace(iter.node, std::move(node->first), std::move(node->second));
  --buckets_[hash];
  values_.destroy_with_allocator(node);
  return {buckets_[hash], true};
}

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::find(const Key& key) {
  if (empty()) {
    return end();
  }
  auto hash = get_hash(key);
  for (iterator bucket_begin = get_iter(key);
       bucket_begin != values_.end() && get_hash(bucket_begin->first) == hash;
       ++bucket_begin) {
    if (equal(bucket_begin->first, key)) {
      return bucket_begin;
    }
  }
  return this->end();
}

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Alloc>
Value& UnorderedMap<Key, Value, Hash, Equal, Alloc>::operator[](
    const Key& key) {
  iterator result = find(key);
  if (result == end()) {
    emplace(key, Value());
  }
  return *find(key);
}

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::erase(
    UnorderedMap::const_iterator iter) {
  auto hash = get_hash(iter->first);
  iterator result = end();
  if (std::next(iter) != cend()) {
    result = find(std::next(iter)->first);
    assert(result != end());
  }
  if (iter == const_iterator(buckets_[hash])) {
    if (result != end() && get_hash(result->first) == hash) {
      buckets_[hash] = result.node;
    } else {
      buckets_[hash] = values_.end();
    }
  }
  values_.erase(iter.node);
  return result;
}

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Alloc>
typename UnorderedMap<Key, Value, Hash, Equal, Alloc>::iterator
UnorderedMap<Key, Value, Hash, Equal, Alloc>::erase(
    UnorderedMap::const_iterator first, UnorderedMap::const_iterator second) {
  iterator iter = end();
  while (first != second) {
    iter = erase(first);
    first = iter;
  }
  return iter;
}

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Alloc>
UnorderedMap<Key, Value, Hash, Equal, Alloc>&
UnorderedMap<Key, Value, Hash, Equal, Alloc>::operator=(UnorderedMap&& map) {
  values_.clear();
  buckets_.assign(map.buckets_.size(), values_.end());
  map.buckets_.clear();
  while (!map.values_.is_empty()) {
    auto& item = *map.values_.begin();
    emplace(const_cast<Key&&>(std::move(item.first)), std::move(item.second));
    map.values_.pop_front();
  }
  return *this;
}

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Alloc>
UnorderedMap<Key, Value, Hash, Equal, Alloc>::UnorderedMap(UnorderedMap&& map)
    : UnorderedMap(map.get_allocator()) {
  buckets_.assign(map.bucket_count(), values_.end());
  map.buckets_.clear();
  while (!map.values_.is_empty()) {
    auto& item = *map.values_.begin();
    emplace(const_cast<Key&&>(std::move(item.first)), std::move(item.second));
    map.values_.pop_front();
  }
}

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Alloc>
template <bool is_const>
struct UnorderedMap<Key, Value, Hash, Equal, Alloc>::Iterator {
  using value_type = std::conditional_t<is_const, const NodeType, NodeType>;
  using pointer = value_type*;
  using reference = value_type&;
  using difference_type = int;
  using iterator_category = std::forward_iterator_tag;
  using DataType =
      std::conditional_t<is_const,
                         typename List<NodeType, Alloc>::const_iterator,
                         typename List<NodeType, Alloc>::iterator>;
  DataType node;

  operator Iterator<true>() const {
    return {node};
  }

  Iterator(DataType node)
      : node(node) {
  }

  Iterator& operator++() {
    ++node;
    return *this;
  }

  inline bool operator==(Iterator other) const noexcept {
    return node == other.node;
  }

  Iterator operator++(int) {
    auto copy = *this;
    operator++();
    return copy;
  }

  pointer operator->() {
    return node.operator->();
  }

  reference operator*() {
    return *node;
  }
};

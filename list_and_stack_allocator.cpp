#include <array>
#include <iostream>
#include <memory>

template <size_t N>
class StackStorage {
 public:
  StackStorage(const StackStorage&) = delete;
  auto& operator=(const StackStorage&) = delete;

  StackStorage()
      : size_(N){};

  ~StackStorage() {
    std::cout << "";  // dealing with clang bug
  }

  inline size_t used() const noexcept {
    return N - size_;
  }

  template <typename T>
  T* create_with_alignment(size_t count);

 private:
  char storage_[N];
  size_t size_;
};

template <size_t N>
template <typename T>
T* StackStorage<N>::create_with_alignment(size_t count) {
  void* st = storage_ + used();
  size_t size = sizeof(T) * count;
  if (std::align(alignof(T), size, st, size_)) {
    T* ptr = reinterpret_cast<T*>(st);
    size_ -= size;
    return ptr;
  }
  return nullptr;
}

template <typename T, size_t N>
class StackAllocator {
 public:
  using value_type = T;
  using pointer = T*;

  explicit StackAllocator(StackStorage<N>& stack_storage) {
    set_storage(&stack_storage);
  }

  StackAllocator()
      : StackAllocator(StackStorage<N>()) {
  }

  template <class U>
  StackAllocator(const StackAllocator<U, N>& allocator)
      : storage_(allocator.get_storage()) {
  }
  pointer allocate(size_t size) {
    return storage_->template create_with_alignment<value_type>(size);
  }

  void deallocate(pointer, size_t) {
  }

  bool operator==(const StackAllocator& stack_allocator) const {
    return storage_ == stack_allocator.storage_;
  }

  StackAllocator(const StackAllocator& stack_allocator)
      : storage_(stack_allocator.storage_) {
  }

  StackAllocator& operator=(const StackAllocator& stack_allocator) noexcept {
    set_storage(stack_allocator.storage_);
    return *this;
  }

  template <typename U>
  struct rebind {
    using other = StackAllocator<U, N>;
  };

  auto& get_storage() const {
    return storage_;
  }

 private:
  void set_storage(StackStorage<N>* stack_storage) noexcept {
    storage_ = stack_storage;
  }

  StackStorage<N>* storage_;
};

template <typename T, typename Allocator = std::allocator<T>>
class List {
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

  inline void pop_back() noexcept {
    erase(--end());
  }

  inline void pop_front() noexcept {
    erase(begin());
  }

  explicit List(const Allocator& allocator = NodeAlloc())
      : alloc_(allocator),
        size_(0){};

  List(size_t count, const T& value = T(),
       const Allocator& allocator = NodeAlloc());

  List(size_t count, const Allocator& allocator);

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
  struct BaseNode;

  struct Node;

  using NodeAlloc =
      typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
  using NodeTraits = std::allocator_traits<NodeAlloc>;
  using AllocTraits = std::allocator_traits<Allocator>;

  [[no_unique_address]] NodeAlloc alloc_;
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
      NodeAlloc copy =
          NodeTraits::select_on_container_copy_construction(list.alloc_);
      std::ranges::swap(copy, alloc_);
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
    : List(NodeTraits::select_on_container_copy_construction(list.alloc_)) {
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
  NodeTraits::destroy(alloc_, &current);
  NodeTraits::deallocate(alloc_, &current, 1);
  prev->next = next;
  next->prev = prev;
  --size_;
}

template <typename T, typename Allocator>
template <typename... Args>
void List<T, Allocator>::emplace(List::const_iterator iter, Args&&... args) {
  BaseNode* current = iter.node;
  BaseNode* previous = (--iter).node;
  Node* new_node = NodeTraits::allocate(alloc_, 1);
  try {
    NodeTraits::construct(alloc_, new_node, previous, current,
                          std::forward<Args>(args)...);
  } catch (...) {
    NodeTraits::deallocate(alloc_, new_node, 1);
    throw;
  }
  previous->next = new_node;
  current->prev = new_node;
  ++size_;
}

template <typename T, typename Allocator>
struct List<T, Allocator>::BaseNode {
  BaseNode(BaseNode* prev, BaseNode* next)
      : next(next),
        prev(prev) {
  }

  BaseNode()
      : BaseNode(this, this) {
  }

  BaseNode* next;
  BaseNode* prev;
};

template <typename T, typename Allocator>
struct List<T, Allocator>::Node : public BaseNode {
  using value_type = T;
  using reference = value_type&;
  using const_refence = const value_type&;
  using pointer = value_type*;

  template <typename... Args>
  Node(BaseNode* prev, BaseNode* next, Args&&... args)
      : BaseNode(prev, next),
        value(std::forward<Args>(args)...) {
  }

  pointer get_value() {
    return &value;
  }

  value_type value;
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
    return node->get_value();
  }

  reference operator*() {
    return *static_cast<Node*>(node)->get_value();
  }

  bool operator==(const Iterator& iterator) const {
    return node == iterator.node;
  }
};

#include <cassert>
#include <iostream>
#include <vector>

template <typename T>
class Deque {
  template <bool is_const>
  struct Iterator;

 public:
  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;

 private:
  struct Chunk;
  using DataType = std::vector<Chunk>;
  using Coord = std::pair<int64_t, int64_t>;
  static const size_t chunk_size = 32;
  static const size_t init_size = 2;

 public:
  size_t size() const noexcept;

  Deque()
      : end_({0, -1}){};

  Deque(size_t size, const T& value);

  auto& operator=(const Deque& deque);

  Deque(const Deque& deque) = default;

  ~Deque() = default;

  explicit Deque(size_t size)
      : Deque(size, T()) {
  }

  void clear() noexcept {
    data_.clear();
    begin_ = Coord();
    end_ = {0, -1};
  }

  void erase(iterator it);

  void insert(iterator it, const T& value);

  T& operator[](size_t index) {
    return *(begin() + index);
  }

  const T& operator[](size_t index) const {
    return *(begin() + index);
  }

  T& at(size_t index) {
    check_index(index);
    return operator[](index);
  }

  const T& at(size_t index) const {
    check_index(index);
    return operator[](index);
  }

  bool empty() const noexcept {
    return size() == 0;
  }

  void push_back(const T& value);

  void pop_back() noexcept;

  void push_front(const T& value);

  void pop_front() noexcept;

  iterator begin() noexcept {
    return iterator{data_.begin() + begin_.first,
                    (empty()) ? nullptr : data_[begin_.first].begin()};
  }

  iterator end() noexcept {
    return {data_.begin() + end_.first,
            (empty()) ? nullptr : data_[end_.first].end()};
  }

  const_iterator begin() const noexcept {
    return cbegin();
  }

  const_iterator end() const noexcept {
    return cend();
  }

  const_iterator cbegin() const noexcept {
    return const_iterator(data_.begin() + begin_.first,
                          (empty()) ? nullptr : data_[begin_.first].begin());
  }

  const_iterator cend() const noexcept {
    return cbegin() + size();
  }

  reverse_iterator rbegin() noexcept {
    return rend() - size() - 1;
  }

  auto rend() noexcept;

 private:
  template <typename Type>
  static inline Type ceil_divide(Type first, Type second) {
    return (first - 1) / second + 1;
  }

  void resize(size_t size);

  void push_if_empty(const T& value) {
    try {
      init(1);
      fill(value);
      return;
    } catch (...) {
      clear();
      throw;
    }
  }

  void init(size_t size);

  inline void check_index(size_t index) const {
    if (index >= size()) {
      throw std::out_of_range("Index out of range");
    }
  }

  void fill(const T& value);

  DataType data_;
  Coord begin_;
  Coord end_;  // [begin; end]
};

template <typename T>
size_t Deque<T>::size() const noexcept {
  if (end_.first > begin_.first) {
    return (end_.first - begin_.first - 1) * chunk_size + chunk_size -
           begin_.second + end_.second + 1;
  }
  return (end_.second - begin_.second + 1);
}

template <typename T>
Deque<T>::Deque(size_t size, const T& value)
    : Deque() {
  try {
    init(size);
    fill(value);
  } catch (...) {
    clear();
    throw;
  }
}

template <typename T>
auto& Deque<T>::operator=(const Deque& deque) {
  DataType new_data(deque.data_);
  std::swap(data_, new_data);
  begin_ = deque.begin_;
  end_ = deque.end_;
  return *this;
}

template <typename T>
void Deque<T>::push_back(const T& value) {
  if (empty()) {
    push_if_empty(value);
    return;
  }
  if (end_ == Coord{data_.size() - 1, chunk_size - 1}) {
    resize(2 * data_.size());
  }
  ++end_.second;
  if (end_.second == static_cast<int64_t>(chunk_size)) {
    ++end_.first;
    end_.second = 0;
    data_[end_.first].init(0, 0);
  }
  data_[end_.first].push_back(value);
}

template <typename T>
void Deque<T>::pop_back() noexcept {
  data_[end_.first].pop_back();
  if (end_.second == 0) {
    end_.second = chunk_size - 1;
    --end_.first;
  } else {
    --end_.second;
  }
}

template <typename T>
void Deque<T>::push_front(const T& value) {
  if (empty()) {
    push_if_empty(value);
    return;
  }
  if (begin_ == Coord{0, 0}) {
    resize(2 * data_.size());
  }
  --begin_.second;
  if (begin_.second == -1) {
    --begin_.first;
    begin_.second += chunk_size;
    data_[begin_.first].init(chunk_size, chunk_size);
  }
  data_[begin_.first].push_front(value);
}

template <typename T>
void Deque<T>::pop_front() noexcept {
  data_[begin_.first].pop_front();
  if (begin_.second == chunk_size - 1) {
    begin_.second = 0;
    ++begin_.first;
  } else {
    ++begin_.second;
  }
}

template <typename T>
auto Deque<T>::rend() noexcept {
  return std::reverse_iterator(
      iterator(data_.begin() + begin_.first,
               (empty()) ? nullptr : data_[begin_.first].begin() - 1));
}

template <typename T>
void Deque<T>::resize(size_t size) {
  assert(size >= data_.size());
  size = std::max(size, 2UL);
  DataType new_data(size);
  Coord old_begin = begin_;
  Coord old_end = end_;
  begin_.first = (new_data.size()) / 4;
  end_.first = begin_.first + (old_end.first - old_begin.first);
  if (end_.first == begin_.first) {
    new_data[begin_.first].take(data_[old_begin.first]);
  } else {
    for (int64_t i = 0; i <= old_end.first - old_begin.first; ++i) {
      new_data[end_.first - i].take(data_[old_end.first - i]);
    }
  }
  std::ranges::swap(data_, new_data);
}

template <typename T>
void Deque<T>::init(size_t size) {
  // allocate memory and set coords
  data_.resize(ceil_divide(std::max(size, init_size * chunk_size), chunk_size));
  size_t remains = data_.size() * chunk_size - size;
  size_t chunks_remains = remains / chunk_size;
  remains %= chunk_size;
  begin_ = {chunks_remains / 2, remains / 2};
  if (size <= chunk_size - begin_.second) {
    end_ = {begin_.first, begin_.second + size - 1};
  } else {
    end_.first = begin_.first +
                 ceil_divide(size - (chunk_size - begin_.second), chunk_size);
    end_.second = size - (end_.first - begin_.first - 1) * chunk_size -
                  (chunk_size - begin_.second) - 1;
  }
  assert(size == this->size());
}

template <typename T>
void Deque<T>::fill(const T& value) {
  data_[begin_.first].init(begin_.second, (begin_.first == end_.first)
                                              ? end_.second + 1
                                              : chunk_size);
  data_[begin_.first].fill(value);
  if (begin_.first != end_.first) {
    for (int i = begin_.first + 1; i < end_.first; ++i) {
      data_[i].init(0, chunk_size);
      data_[i].fill(value);
    }
    data_[end_.first].init(0, end_.second + 1);
    data_[end_.first].fill(value);
  }
}

template <typename T>
struct Deque<T>::Chunk {
  Chunk()
      : chunk_begin(0),
        chunk_end(0),
        data() {
  }

  Chunk(const Chunk& chunk)
      : data(nullptr) {
    init(chunk.chunk_begin, chunk.chunk_end);
    std::copy(chunk.begin(), chunk.end(), begin());
  }

  void fill(const T& value);

  void take(Chunk& chunk) noexcept;

  auto& operator=(const Chunk& chunk);

  void reserve() {
    data = reinterpret_cast<T*>(new char[chunk_size * sizeof(T)]);
  }

  void place(size_t index, const T& value) {
    new (data + index) T(value);
  }

  inline T* begin() const noexcept {
    return data + chunk_begin;
  }

  inline T* end() const noexcept {
    return data + chunk_end;
  }

  void init(size_t begin, size_t end);

  T& operator[](size_t i) noexcept {
    return data[i];
  }

  T operator[](size_t i) const noexcept {
    return data[i];
  }

  inline bool empty() const noexcept {
    return chunk_end <= chunk_begin;
  }

  void push_back(const T& val) {
    place(chunk_end, val);
    ++chunk_end;
  }

  void pop_back() noexcept {
    --chunk_end;
    (data + chunk_end)->~T();
  }

  void push_front(const T& val) {
    place(chunk_begin - 1, val);
    --chunk_begin;
  }

  void pop_front() noexcept {
    (data + chunk_begin)->~T();
    ++chunk_begin;
  }

  void clear() noexcept;

  ~Chunk() {
    clear();
  }

  int64_t chunk_begin;
  int64_t chunk_end;
  T* data;
};

template <typename T>
void Deque<T>::Chunk::fill(const T& value) {
  for (auto i = begin(); i < end(); ++i) {
    try {
      new (i) T(value);
    } catch (...) {
      chunk_end = chunk_begin + i - begin();
      throw;
    }
  }
}

template <typename T>
void Deque<T>::Chunk::take(Deque<T>::Chunk& chunk) noexcept {
  if (this == &chunk) {
    return;
  }
  clear();
  chunk_begin = chunk.chunk_begin;
  chunk_end = chunk.chunk_end;
  data = chunk.data;
  chunk.data = nullptr;
}

template <typename T>
auto& Deque<T>::Chunk::operator=(const Deque<T>::Chunk& chunk) {
  if (this == &chunk) {
    return *this;
  }
  if (data != nullptr) {
    clear();
  }
  init(chunk.chunk_begin, chunk.chunk_end);
  std::copy(chunk.begin(), chunk.end(), begin());
  chunk_begin = chunk.chunk_begin;
  chunk_end = chunk.chunk_end;
  return *this;
}

template <typename T>
void Deque<T>::Chunk::init(size_t begin, size_t end) {
  if (data == nullptr) {
    reserve();
  }
  chunk_begin = static_cast<int64_t>(begin);
  chunk_end = static_cast<int64_t>(end);
}

template <typename T>
void Deque<T>::Chunk::clear() noexcept {
  while (!empty()) {
    pop_back();
  }
  if (data != nullptr) {
    delete[] reinterpret_cast<char*>(data);
    data = nullptr;
  }
  chunk_begin = chunk_end = 0;
}

template <typename T>
template <bool is_const>
struct Deque<T>::Iterator {
 public:
  using difference_type = uint32_t;
  using value_type = std::conditional_t<is_const, const T, T>;
  using pointer = value_type*;
  using reference = value_type&;
  using iterator_category = std::random_access_iterator_tag;

  Iterator(
      std::conditional_t<is_const, const typename DataType ::const_iterator&,
                         const typename DataType::iterator&>
          first,
      pointer second)
      : first_iter(first),
        second_iter(second) {
  }

  operator Iterator<true>() const {
    return Iterator<true>(first_iter, second_iter);
  }

  Iterator& operator++();

  Iterator& operator--();

  Iterator operator++(int);

  Iterator operator--(int) {
    Iterator copy = *this;
    operator--();
    return copy;
  }

  Iterator& operator+=(int add) {
    return (*this) = (*this) + add;
  }

  Iterator operator+(int add) const;

  Iterator operator-(int add) const {
    return operator+(-add);
  }

  bool friend operator==(Iterator x, Iterator y) {
    return std::pair{x.first_iter, x.second_iter} ==
           std::pair{y.first_iter, y.second_iter};
  }

  bool friend operator<(Iterator x, Iterator y) {
    return std::pair{x.first_iter, x.second_iter} <
           std::pair{y.first_iter, y.second_iter};
  }

  bool friend operator>(Iterator x, Iterator y) {
    return y < x;
  }

  bool friend operator>=(Iterator x, Iterator y) {
    return y <= x;
  }

  bool friend operator<=(Iterator x, Iterator y) {
    return x < y || x == y;
  }

  auto& operator-=(int add) {
    return operator+=(-add);
  }

  difference_type operator-(const Iterator& it) noexcept;

  reference operator*() noexcept {
    return *second_iter;
  }

  pointer operator->() noexcept {
    return second_iter;
  }

  std::conditional_t<is_const, typename DataType::const_iterator,
                     typename DataType::iterator>
      first_iter;
  pointer second_iter;
};

template <typename T>
template <bool is_const>
typename Deque<T>::template Iterator<is_const>&
Deque<T>::Iterator<is_const>::operator--() {
  if (second_iter == nullptr) {
    return *this;
  }
  if (second_iter == first_iter->data) {
    --first_iter;
    second_iter = (first_iter->end());
  }
  --second_iter;
  return *this;
}

template <typename T>
template <bool is_const>
typename Deque<T>::template Iterator<is_const>&
Deque<T>::Iterator<is_const>::operator++() {
  if (second_iter == nullptr) {
    return *this;
  }
  ++second_iter;
  if (second_iter == first_iter->data + chunk_size) {
    ++first_iter;
    second_iter = first_iter->data;
  }
  return *this;
}

template <typename T>
template <bool is_const>
typename Deque<T>::template Iterator<is_const>
Deque<T>::Iterator<is_const>::operator++(int) {
  auto copy = *this;
  if (second_iter != nullptr) {
    operator++();
  }
  return copy;
}

template <typename T>
template <bool is_const>
typename Deque<T>::template Iterator<is_const>
Deque<T>::Iterator<is_const>::operator+(int add) const {
  if (add == 0 || second_iter == nullptr) {
    return *this;
  }
  auto new_first = first_iter + add / static_cast<int64_t>(chunk_size);
  add %= static_cast<int64_t>(chunk_size);
  int64_t diff = second_iter - first_iter->data + add;
  if (add > 0 && diff >= static_cast<int64_t>(chunk_size)) {
    ++new_first;
    diff -= chunk_size;
  }
  if (add < 0 && diff < 0) {
    --new_first;
    diff += chunk_size;
  }
  return Iterator<is_const>{new_first, new_first->data + diff};
}

template <typename T>
template <bool is_const>
typename Deque<T>::template Iterator<is_const>::difference_type
Deque<T>::Iterator<is_const>::operator-(
    const Deque::Iterator<is_const>& it) noexcept {
  if (first_iter == it.first_iter) {
    return second_iter - it.second_iter;
  }
  return (first_iter - it.first_iter - 1) * chunk_size +
         (second_iter - first_iter->data) +
         (chunk_size - (it.second_iter - it.first_iter->data));
}

template <typename T>
void Deque<T>::erase(Deque::iterator it) {
  if (it == begin()) {
    pop_front();
  } else if (it == --end()) {
    pop_back();
  } else {
    while (it + 1 != end()) {
      std::ranges::swap(*it, *(it + 1));
      ++it;
    }
    pop_back();
  }
}

template <typename T>
void Deque<T>::insert(Deque::iterator it, const T& value) {
  if (it == end()) {
    push_back(value);
  } else {
    auto diff = it - begin();
    push_front(value);
    for (auto i = begin(); i != begin() + diff; ++i) {
      std::ranges::swap(*i, *(i + 1));
    }
  }
}

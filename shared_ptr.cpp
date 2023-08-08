#include <iostream>
template <typename T>
class EnableSharedFromThis;

template <typename Alloc>
void deallocate(Alloc& alloc,
                typename std::allocator_traits<Alloc>::pointer ptr,
                size_t size = 1) {
  std::allocator_traits<Alloc>::deallocate(alloc, ptr, size);
}

template <typename Alloc>
void destroy(Alloc& alloc, typename std::allocator_traits<Alloc>::pointer ptr) {
  std::allocator_traits<Alloc>::destroy(alloc, ptr);
}

template <typename To, typename U>
auto recast_allocator(U from) {
  using NewAlloc = typename std::allocator_traits<U>::template rebind_alloc<To>;
  return NewAlloc(from);
}

struct BaseControlBlock {
  uint shared_count;
  uint weak_count;
  BaseControlBlock(uint shared_count, uint weak_count)
      : shared_count(shared_count),
        weak_count(weak_count) {
  }

  virtual ~BaseControlBlock() = default;
  virtual void dispose() = 0;
  virtual void use_deleter() = 0;
  virtual void* get() = 0;
};

template <typename Real, typename Deleter, typename Allocator>
struct ControlBlockRegular : BaseControlBlock {
  Real* ptr;
  [[no_unique_address]] Deleter deleter;
  [[no_unique_address]] Allocator allocator;

  ControlBlockRegular(Real* ptr, Deleter deleter, Allocator allocator,
                      uint shared_count = 1, uint weak_count = 0)
      : BaseControlBlock(shared_count, weak_count),
        ptr(ptr),
        deleter(deleter),
        allocator(allocator) {
  }

  void use_deleter() override {
    deleter(static_cast<Real*>(this->ptr));
  }

  void dispose() override;

  void* get() override {
    return ptr;
  }
};

template <typename T, typename Allocator>
struct ControlBlockFromMake : BaseControlBlock {
  T object;
  [[no_unique_address]] Allocator allocator;

  void use_deleter() override {
    std::allocator_traits<Allocator>::destroy(allocator, &object);
  }

  template <typename... Args>
  ControlBlockFromMake(Allocator allocator, uint shared_count, uint weak_count,
                       Args&&... args)
      : BaseControlBlock(shared_count, weak_count),
        object(std::forward<Args>(args)...),
        allocator(allocator) {
  }

  void dispose() override;

  void* get() override {
    return &object;
  }
};

template <typename T>
class SharedPtr {
  template <typename U>
  friend class WeakPtr;
  template <typename U>
  friend class SharedPtr;

  BaseControlBlock* control_block_;

 public:
  template <typename U = T, typename Deleter = std::default_delete<T>,
            typename Allocator = std::allocator<T>>
  SharedPtr(U* ptr = nullptr, Deleter deleter = Deleter(),
            Allocator allocator = Allocator());

  SharedPtr(BaseControlBlock* control_block);

  SharedPtr& operator=(const SharedPtr& shared_ptr);

  template <typename U>
  SharedPtr(const SharedPtr<U>& shared_ptr);

  template <typename U>
  SharedPtr(SharedPtr<U>&& shared_ptr);

  SharedPtr(const SharedPtr<T>& shared_ptr);

  T* get() const;

  T& operator*() const {
    return *get();
  }

  T* operator->() const {
    return get();
  }

  size_t use_count() const;

  void swap(SharedPtr& shared_ptr) {
    std::swap(control_block_, shared_ptr.control_block_);
  }

  ~SharedPtr() {
    clear();
  }

  template <typename U = T, typename Deleter = std::default_delete<T>,
            typename Allocator = std::allocator<T>>
  auto reset(U* new_ptr = nullptr, Deleter deleter = Deleter(),
             Allocator allocator = Allocator());

  template <typename Allocator, typename... Args>
  static SharedPtr<T> construct(const Allocator& allocator, Args&&... args);

  SharedPtr(SharedPtr&& ptr);

  SharedPtr& operator=(SharedPtr&& ptr);

 private:
  void clear();

  template <typename U = T, typename Allocator, typename Deleter>
  ControlBlockRegular<U, Deleter, Allocator>* create_by_ptr(U* ptr,
                                                            Allocator allocator,
                                                            Deleter deleter);
};

template <typename T>
class WeakPtr {
  BaseControlBlock* control_block_;

  template <typename U>
  friend class WeakPtr;

 public:
  T* get() const noexcept {
    if (control_block_ == nullptr) {
      return nullptr;
    }
    return reinterpret_cast<T*>(control_block_->get());
  }

  inline bool expired() const noexcept {
    return control_block_ == nullptr || control_block_->shared_count == 0;
  }

  WeakPtr()
      : WeakPtr(SharedPtr<T>()) {
  }

  template <typename U = T>
  WeakPtr(SharedPtr<U> other = SharedPtr<U>());

  template <typename U = T>
  WeakPtr& operator=(SharedPtr<U> other);

  template <typename U = T>
  WeakPtr(WeakPtr<U> other);

  WeakPtr(const WeakPtr& other);

  WeakPtr& operator=(const WeakPtr& other);

  WeakPtr(WeakPtr&& ptr);

  WeakPtr& operator=(WeakPtr&& other);

  SharedPtr<T> lock() const {
    return SharedPtr<T>(control_block_);
  }

  void swap(WeakPtr& ptr) {
    std::swap(control_block_, ptr.control_block_);
  }

  size_t use_count() const noexcept;

  ~WeakPtr();
};
template <typename T>
WeakPtr<T>& WeakPtr<T>::operator=(const WeakPtr& other) {
  WeakPtr copy = other;
  swap(copy);
  return *this;
}

template <typename T>
class EnableSharedFromThis {
  WeakPtr<T> weak_ptr_;
  template <class U>
  friend class SharedPtr;

  template <class U>
  friend class WeakPtr;

 public:
  SharedPtr<T> shared_from_this() const {
    return weak_ptr_.lock();
  }

  WeakPtr<T> weak_from_this() const {
    return weak_ptr_;
  }
};

template <typename Real, typename Deleter, typename Allocator>
void ControlBlockRegular<Real, Deleter, Allocator>::dispose() {
  auto new_allocator =
      recast_allocator<ControlBlockRegular<Real, Deleter, Allocator>>(
          allocator);
  deallocate(new_allocator, this, 1);
}

template <typename T, typename Allocator>
void ControlBlockFromMake<T, Allocator>::dispose() {
  auto new_allocator =
      recast_allocator<ControlBlockFromMake<T, Allocator>>(allocator);
  deallocate(new_allocator, this, 1);
}

template <typename T>
SharedPtr<T>::SharedPtr(BaseControlBlock* control_block)
    : control_block_(control_block) {
  if (control_block == nullptr) {
    throw "bad control block";
  }
  ++control_block_->shared_count;
}
template <typename T>
SharedPtr<T>& SharedPtr<T>::operator=(const SharedPtr& shared_ptr) {
  SharedPtr copy = shared_ptr;
  swap(copy);
  return *this;
}
template <typename T>
template <typename U>
SharedPtr<T>::SharedPtr(const SharedPtr<U>& shared_ptr)
    : control_block_(shared_ptr.control_block_) {
  static_assert(std::is_base_of_v<T, U> || std::is_same_v<T, U>);
  ++control_block_->shared_count;
}

template <typename T>
template <typename U>
SharedPtr<T>::SharedPtr(SharedPtr<U>&& shared_ptr)
    : control_block_(shared_ptr.control_block_) {
  static_assert(std::is_base_of_v<T, U> || std::is_same_v<T, U>);
  ++control_block_->shared_count;
  shared_ptr.reset();
}

template <typename T>
SharedPtr<T>::SharedPtr(const SharedPtr<T>& shared_ptr)
    : control_block_(shared_ptr.control_block_) {
  if (control_block_ != nullptr) {
    ++control_block_->shared_count;
  }
}

template <typename T>
T* SharedPtr<T>::get() const {
  if (control_block_ == nullptr) {
    return nullptr;
  }
  return reinterpret_cast<T*>(control_block_->get());
}

template <typename T>
size_t SharedPtr<T>::use_count() const {
  if (control_block_ == nullptr) {
    return 0;
  }
  return control_block_->shared_count;
}

template <typename T>
template <typename U, typename Deleter, typename Allocator>
auto SharedPtr<T>::reset(U* new_ptr, Deleter deleter, Allocator allocator) {
  static_assert(std::is_base_of_v<T, U> || std::is_same_v<T, U>);
  if (new_ptr != nullptr) {
    SharedPtr copy(new_ptr, deleter, allocator);
    swap(copy);
  } else {
    clear();
    control_block_ = nullptr;
  }
}

template <typename T>
template <typename Allocator, typename... Args>
SharedPtr<T> SharedPtr<T>::construct(const Allocator& allocator,
                                     Args&&... args) {
  using NewAlloc = typename std::allocator_traits<
      Allocator>::template rebind_alloc<ControlBlockFromMake<T, Allocator>>;
  NewAlloc new_alloc = allocator;

  ControlBlockFromMake<T, Allocator>* block =
      std::allocator_traits<NewAlloc>::allocate(new_alloc, 1);
  std::allocator_traits<NewAlloc>::construct(new_alloc, block, allocator, 0, 0,
                                             std::forward<Args>(args)...);
  auto result = SharedPtr<T>(static_cast<BaseControlBlock*>(block));
  if constexpr (std::is_base_of_v<EnableSharedFromThis<T>, T>) {
    result.get()->weak_ptr_ = result;
  }
  return result;
}

template <typename T>
SharedPtr<T>::SharedPtr(SharedPtr&& ptr)
    : control_block_(ptr.control_block_) {
  ++control_block_->shared_count;
  ptr.reset();
}

template <typename T>
SharedPtr<T>& SharedPtr<T>::operator=(SharedPtr&& ptr) {
  auto copy = std::move(ptr);
  swap(copy);
  return *this;
}

template <typename T>
void SharedPtr<T>::clear() {
  if (control_block_ == nullptr) {
    return;
  }
  --control_block_->shared_count;
  if (control_block_->shared_count != 0) {
    return;
  }
  ++control_block_->shared_count;
  control_block_->use_deleter();
  --control_block_->shared_count;
  if (control_block_->weak_count != 0) {
    return;
  }
  control_block_->dispose();
}

template <typename T>
template <typename U, typename Allocator, typename Deleter>
ControlBlockRegular<U, Deleter, Allocator>* SharedPtr<T>::create_by_ptr(
    U* ptr, Allocator allocator, Deleter deleter) {
  using NewAlloc =
      typename std::allocator_traits<Allocator>::template rebind_alloc<
          ControlBlockRegular<U, Deleter, Allocator>>;
  NewAlloc new_allocator = allocator;
  ControlBlockRegular<U, Deleter, Allocator>* block =
      std::allocator_traits<NewAlloc>::allocate(new_allocator, 1);
  std::construct_at(block, ptr, deleter, allocator);
  return block;
}
template <typename T>
template <typename U, typename Deleter, typename Allocator>
SharedPtr<T>::SharedPtr(U* ptr, Deleter deleter, Allocator allocator)
    : control_block_(nullptr) {
  static_assert(std::is_base_of_v<T, U> || std::is_same_v<T, U>);
  if (ptr != nullptr) {
    control_block_ = create_by_ptr(ptr, allocator, deleter);
  }
  if constexpr (std::is_base_of_v<EnableSharedFromThis<T>, T>) {
    if (get() != nullptr) {
      get()->weak_ptr_ = *this;
    }
  }
}

template <typename T>
WeakPtr<T>::WeakPtr(const WeakPtr& other)
    : control_block_(other.control_block_) {
  ++control_block_->weak_count;
}

template <typename T>
WeakPtr<T>::WeakPtr(WeakPtr&& ptr)
    : control_block_(ptr.control_block_) {
  ++control_block_->weak_count;
  auto tmp = WeakPtr(SharedPtr<T>());
  ptr.swap(tmp);
  if constexpr (std::is_base_of_v<EnableSharedFromThis<T>, T>) {
    this->weak_ptr_.reset(get());
  }
}
template <typename T>
WeakPtr<T>& WeakPtr<T>::operator=(WeakPtr&& other) {
  WeakPtr<T> copy = std::move(other);
  swap(copy);
  return *this;
}
template <typename T>
size_t WeakPtr<T>::use_count() const noexcept {
  if (control_block_ == nullptr) {
    return 0;
  }
  return control_block_->shared_count;
}
template <typename T>
WeakPtr<T>::~WeakPtr() {
  if (control_block_ == nullptr) {
    return;
  }
  --control_block_->weak_count;
  if (control_block_->shared_count == 0 && control_block_->weak_count == 0) {
    control_block_->dispose();
  }
}
template <typename T>
template <typename U>
WeakPtr<T>::WeakPtr(WeakPtr<U> other)
    : control_block_(other.control_block_) {
  static_assert(std::is_base_of_v<T, U> || std::is_same_v<T, U>);
  ++control_block_->weak_count;
  if constexpr (std::is_base_of_v<EnableSharedFromThis<T>, T>) {
    this->weak_ptr_ = *this;
  }
}
template <typename T>
template <typename U>
WeakPtr<T>& WeakPtr<T>::operator=(SharedPtr<U> other) {
  WeakPtr<T> copy = other;
  swap(copy);
  return *this;
}
template <typename T>
template <typename U>
WeakPtr<T>::WeakPtr(SharedPtr<U> other)
    : control_block_(other.control_block_) {
  static_assert(std::is_base_of_v<T, U> || std::is_same_v<T, U>);
  if (control_block_ != nullptr) {
    ++control_block_->weak_count;
  }
  if constexpr (std::is_base_of_v<EnableSharedFromThis<T>, T>) {
    if (get() != nullptr) {
      get()->weak_ptr_ = *this;
    }
  }
}

template <typename T, typename Allocator, typename... Args>
SharedPtr<T> allocateShared(const Allocator& allocator, Args&&... args) {
  return SharedPtr<T>::construct(allocator, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
  return SharedPtr<T>::construct(std::allocator<T>(),
                                 std::forward<Args>(args)...);
}

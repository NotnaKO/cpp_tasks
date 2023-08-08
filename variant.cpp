#include <array>
#include <functional>

class BadVariantAccess : std::exception {};

template <typename Head, typename... Tail>
struct GetMaxSizeOf
    : std::conditional_t<(sizeof(Head) > GetMaxSizeOf<Tail...>::value),
                         std::integral_constant<size_t, sizeof(Head)>,
                         GetMaxSizeOf<Tail...>> {};

template <typename Head>
struct GetMaxSizeOf<Head> : std::integral_constant<size_t, sizeof(Head)> {};

template <typename Head, typename... Tail>
struct GetMaxAlignOf
    : std::conditional_t<(alignof(Head) > GetMaxAlignOf<Tail...>::value),
                         std::integral_constant<size_t, alignof(Head)>,
                         GetMaxAlignOf<Tail...>> {};

template <typename Head>
struct GetMaxAlignOf<Head> : std::integral_constant<size_t, alignof(Head)> {};

template <typename... Types>
struct VariantStorage {
  alignas(GetMaxAlignOf<Types...>::value)
      std::array<std::byte, GetMaxSizeOf<Types...>::value> buffer{};
  size_t current_index = sizeof...(Types);
};

template <typename T, typename Head, typename... Tail>
struct GetIndexByType
    : std::integral_constant<size_t, GetIndexByType<T, Tail...>::value + 1> {};

template <typename T, typename... Tail>
struct GetIndexByType<T, T, Tail...> : std::integral_constant<size_t, 0> {};

template <typename T, typename... Tail>
constexpr size_t get_index_v = GetIndexByType<T, Tail...>::value;

template <size_t N, typename, typename... Tail>
struct GetTypeByIndex : GetTypeByIndex<N - 1, Tail...> {};

template <typename Head, typename... Tail>
struct GetTypeByIndex<0, Head, Tail...> {
  using type = Head;
};

template <size_t N, typename... Tail>
using get_type_t = typename GetTypeByIndex<N, Tail...>::type;

template <typename T, typename... Args>
struct ContainedInPackage : std::disjunction<std::is_same<T, Args>...> {};

template <typename... Types>
class Variant;

template <typename T, typename... Types>
struct VariantChoice {
  using Derived = Variant<Types...>;

  VariantChoice() = default;
  VariantChoice(const VariantChoice&) = delete;
  VariantChoice& operator=(const VariantChoice&) = delete;

  static constexpr size_t type_index = get_index_v<T, Types...>;

  Derived& my_variant() noexcept {
    return static_cast<Derived&>(*this);
  }

  T* get_ptr() noexcept {
    return reinterpret_cast<T*>(std::launder(&my_variant().buffer));
  }

  template <typename U, typename = std::enable_if_t<
                            !std::is_same_v<std::decay_t<U>, Derived>, bool>>
  void construct(U&& value);

  void construct(const Derived& variant);

  void construct(Derived&& variant);

  VariantChoice(const T& value) {
    construct(value);
  }

  VariantChoice(T&& value) {
    construct(std::move(value));
  }

  template <typename U, std::enable_if_t<std::is_constructible_v<T, U> &&
                                             !std::is_convertible_v<T, U>,
                                         bool> = true>
  VariantChoice(U&& value) {
    construct(std::forward<U>(value));
  }

  void assign(const Derived& variant);

  void assign(Derived&& variant);

  void destroy();

  template <typename U>
  bool contain() const;
};

template <typename Head, typename... Args>
struct First {
  using type = Head;
};

template <typename... Types>
class Variant : private VariantStorage<Types...>,
                private VariantChoice<Types, Types...>... {
  template <typename T, typename... Args>
  friend struct VariantChoice;

  template <typename T, typename... Args,
            std::enable_if_t<ContainedInPackage<T, Args...>::value, bool>>
  friend const T& get(const Variant<Args...>&);

  template <typename T, typename... Args,
            std::enable_if_t<ContainedInPackage<T, Args...>::value, bool>>
  friend T& get(Variant<Args...>&);

  template <typename T, typename... Args,
            std::enable_if_t<ContainedInPackage<T, Args...>::value, bool>>
  friend T&& get(Variant<Args...>&&);

  void clear() {
    (VariantChoice<Types, Types...>::destroy(), ...);
  }

 public:
  using VariantChoice<Types, Types...>::VariantChoice...;

  Variant()
      : VariantChoice<typename First<Types...>::type, Types...>(
            typename First<Types...>::type()){};

  Variant(const Variant& variant)
      : VariantStorage<Types...>(),
        VariantChoice<Types, Types...>()... {
    (VariantChoice<Types, Types...>::construct(variant), ...);
  }

  auto& operator=(const Variant& variant);

  auto& operator=(Variant&& variant);

  Variant(Variant&& variant) {
    (VariantChoice<Types, Types...>::construct(std::move(variant)), ...);
  };

  size_t index() const noexcept {
    return this->current_index;
  }

  template <size_t N, typename... Args>
  decltype(auto) emplace(Args&&... args);

  bool valueless_by_exception() const noexcept {
    return index() == sizeof...(Types);
  }

  template <typename T, typename... Args>
  T& emplace(Args&&... args);

  template <typename T, typename U>
  T& emplace(std::initializer_list<U> args);

  ~Variant() {
    clear();
  }
};

template <typename T, typename... Types>
template <typename U, typename>
void VariantChoice<T, Types...>::construct(U&& value) {
  std::construct_at(get_ptr(), std::forward<decltype(value)>(value));
  my_variant().current_index = get_index_v<T, Types...>;
}

template <typename T, typename... Types>
void VariantChoice<T, Types...>::construct(const Derived& variant) {
  if (variant.index() == type_index) {
    construct(*reinterpret_cast<const T*>(std::launder(&variant.buffer)));
    my_variant().current_index = type_index;
  }
}

template <typename T, typename... Types>
void VariantChoice<T, Types...>::construct(Derived&& variant) {
  if (variant.index() == type_index) {
    construct(std::move(*reinterpret_cast<T*>(std::launder(&variant.buffer))));
  }
}

template <typename T, typename... Types>
void VariantChoice<T, Types...>::assign(const Derived& variant) {
  auto& this_variant = my_variant();
  if (type_index != variant.current_index) {
    return;
  }
  const auto& new_value =
      *reinterpret_cast<const T*>(std::launder(&variant.buffer));
  if (this_variant.current_index == type_index) {
    auto& value = *get_ptr();
    value = new_value;
  } else {
    this_variant.clear();
    construct(new_value);
  }
}

template <typename T, typename... Types>
void VariantChoice<T, Types...>::assign(Derived&& variant) {
  auto& this_variant = my_variant();
  if (type_index == variant.current_index) {
    auto&& new_value =
        std::move(*reinterpret_cast<T*>(std::launder(&variant.buffer)));
    if (this_variant.current_index == type_index) {
      auto& value = *get_ptr();
      value = std::move(new_value);
    } else {
      this_variant.clear();
      construct(std::move(new_value));
    }
  }
}

template <typename T, typename... Types>
void VariantChoice<T, Types...>::destroy() {
  auto& variant = my_variant();
  if (variant.current_index == type_index) {
    std::destroy_at(get_ptr());
    variant.current_index = sizeof...(Types);
  }
}

template <typename T, typename... Types>
template <typename U>
bool VariantChoice<T, Types...>::contain() const {
  if constexpr (std::is_same_v<T, U>) {
    const Derived& variant = static_cast<const Derived&>(*this);
    return variant.current_index == type_index;
  }
  return false;
}

template <typename... Types>
auto& Variant<Types...>::operator=(const Variant& variant) {
  if (this != &variant) {
    (VariantChoice<Types, Types...>::assign(variant), ...);
  }
  return *this;
}

template <typename... Types>
auto& Variant<Types...>::operator=(Variant&& variant) {
  if (this != &variant) {
    (VariantChoice<Types, Types...>::assign(std::move(variant)), ...);
  }
  return *this;
};

template <typename... Types>
template <size_t N, typename... Args>
decltype(auto) Variant<Types...>::emplace(Args&&... args) {
  return emplace<get_type_t<N, Types...>>(std::forward<Args>(args)...);
}

template <typename... Types>
template <typename T, typename... Args>
T& Variant<Types...>::emplace(Args&&... args) {
  clear();
  std::construct_at(VariantChoice<T, Types...>::get_ptr(),
                    std::forward<Args>(args)...);
  this->current_index = get_index_v<T, Types...>;
  return *VariantChoice<T, Types...>::get_ptr();
}

template <typename... Types>
template <typename T, typename U>
T& Variant<Types...>::emplace(std::initializer_list<U> args) {
  clear();
  std::construct_at(VariantChoice<T, Types...>::get_ptr(), args);
  this->current_index = get_index_v<T, Types...>;
  return *VariantChoice<T, Types...>::get_ptr();
}

template <typename T, typename... Types,
          std::enable_if_t<ContainedInPackage<T, Types...>::value, bool> = true>
bool holds_alternative(const Variant<Types...>& variant) {
  return variant.index() == get_index_v<T, Types...>;
}

template <typename T, typename... Types>
void check_variant_type(const Variant<Types...>& variant) {
  if (!holds_alternative<T>(variant)) {
    throw BadVariantAccess();
  }
}

template <size_t N, typename... Args,
          std::enable_if_t<(N < sizeof...(Args)), bool> = true>
auto& get(Variant<Args...>& variant) {
  return get<typename GetTypeByIndex<N, Args...>::type>(variant);
}

template <typename T, typename... Args,
          std::enable_if_t<ContainedInPackage<T, Args...>::value, bool> = true>
T& get(Variant<Args...>& variant) {
  check_variant_type<T>(variant);
  return *std::launder(reinterpret_cast<T*>(&(variant.buffer)));
}

template <size_t N, typename... Args,
          std::enable_if_t<(N < sizeof...(Args)), bool> = true>
const auto& get(const Variant<Args...>& variant) {
  return get<typename GetTypeByIndex<N, Args...>::type>(variant);
}

template <typename T, typename... Types,
          std::enable_if_t<ContainedInPackage<T, Types...>::value, bool> = true>
const T& get(const Variant<Types...>& variant) {
  check_variant_type<T>(variant);
  return reinterpret_cast<const T&>(variant.buffer);
}

template <size_t N, typename... Args,
          std::enable_if_t<(N < sizeof...(Args)), bool> = true>
auto&& get(Variant<Args...>&& variant) {
  return get<typename GetTypeByIndex<N, Args...>::type>(std::move(variant));
}

template <typename T, typename... Types,
          std::enable_if_t<ContainedInPackage<T, Types...>::value, bool> = true>
T&& get(Variant<Types...>&& variant) {
  check_variant_type<T>(variant);
  return std::move(reinterpret_cast<T&>(variant.buffer));
}

template <typename T>
struct default_integral_constant : std::integral_constant<T, T{}> {};

template <typename T1, typename T2>
struct Concatenation;

template <size_t... I1, size_t... I2>
struct Concatenation<std::index_sequence<I1...>, std::index_sequence<I2...>>
    : default_integral_constant<std::index_sequence<I1..., I2...>> {};
template <size_t N>
struct NullSequence : Concatenation<typename NullSequence<N - 1>::value_type,
                                    std::index_sequence<0>> {};

template <>
struct NullSequence<0> : default_integral_constant<std::index_sequence<>> {};

template <typename... Variants>
struct VariantSizeSequence;

template <typename... Types, typename... Tail>
struct VariantSizeSequence<Variant<Types...>, Tail...>
    : Concatenation<std::index_sequence<sizeof...(Types) - 1>,
                    typename VariantSizeSequence<Tail...>::value_type> {};

template <typename... Types>
struct VariantSizeSequence<Variant<Types...>>
    : default_integral_constant<std::index_sequence<sizeof...(Types) - 1>> {};

template <typename T1, typename T2>
struct Equal;

template <typename T1, typename T2>
struct Match;

template <size_t N, size_t M, size_t... I1, size_t... I2>
struct Match<std::index_sequence<N, I1...>, std::index_sequence<M, I2...>>
    : std::conditional_t<
          sizeof...(I1) == Match<std::index_sequence<I1...>,
                                 std::index_sequence<I2...>>::value,
          std::integral_constant<size_t,
                                 Match<std::index_sequence<I1...>,
                                       std::index_sequence<I2...>>::value +
                                     static_cast<size_t>(N == M)>,
          std::integral_constant<size_t,
                                 Match<std::index_sequence<I1...>,
                                       std::index_sequence<I2...>>::value>> {};
template <size_t N, size_t M>
struct Match<std::index_sequence<N>, std::index_sequence<M>>
    : std::integral_constant<size_t, static_cast<size_t>(N == M)> {};

template <size_t... I1, size_t... I2>
struct Equal<std::index_sequence<I1...>, std::index_sequence<I2...>>
    : std::conjunction<
          std::bool_constant<sizeof...(I1) == sizeof...(I2)>,
          std::bool_constant<Match<std::index_sequence<I1...>,
                                   std::index_sequence<I2...>>::value ==
                             sizeof...(I1)>> {};

template <size_t N>
constexpr auto make_null_sequence = NullSequence<N>::value;

template <typename... Args>
struct Full;

template <typename... Variants, size_t... I>
struct Full<std::index_sequence<I...>, Variants...>
    : Equal<
          std::index_sequence<I...>,
          typename VariantSizeSequence<std::decay_t<Variants>...>::value_type> {
};

template <typename... Args>
struct Next;

template <size_t N, typename T, typename U>
struct Helper;

template <size_t N, size_t Head, size_t... I1, size_t... I2>
struct Helper<N, std::index_sequence<I1...>, std::index_sequence<Head, I2...>>
    : std::conditional_t<
          sizeof...(I2) != N,
          Helper<N, std::index_sequence<I1..., Head>,
                 std::index_sequence<I2...>>,
          default_integral_constant<typename Concatenation<
              std::index_sequence<I1..., Head + 1>,
              typename NullSequence<sizeof...(I2)>::value_type>::value_type>> {
};

template <size_t... I, typename... Variants>
struct Next<std::index_sequence<I...>, Variants...>
    : Helper<Match<std::index_sequence<I...>,
                   typename VariantSizeSequence<
                       std::decay_t<Variants>...>::value_type>::value,
             std::index_sequence<>, std::index_sequence<I...>> {};

template <typename Visitor, size_t... I, typename... Variants>
auto visit_impl(Visitor&& visitor, const std::index_sequence<I...>&,
                Variants&&... variants) {
  if constexpr (!Full<std::index_sequence<I...>, Variants...>::value) {
    if (((variants.index() == I) && ...)) {
      return std::invoke(std::forward<Visitor>(visitor),
                         get<I>(std::forward<Variants>(variants))...);
    }
    return visit_impl(std::forward<Visitor>(visitor),
                      Next<std::index_sequence<I...>, Variants...>::value,
                      std::forward<Variants>(variants)...);
  } else {
    return std::invoke(std::forward<Visitor>(visitor),
                       get<I>(std::forward<Variants>(variants))...);
  }
}

template <typename Visitor, typename... Variants>
auto visit(Visitor&& visitor, Variants&&... variants) {
  return visit_impl(std::forward<Visitor>(visitor),
                    make_null_sequence<sizeof...(Variants)>,
                    std::forward<Variants>(variants)...);
}

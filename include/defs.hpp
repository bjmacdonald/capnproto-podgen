#pragma once

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <iostream>
#include <type_traits>
#include <cstdint>
#include <algorithm>


namespace podgen {

/// alias for our plain version of the capnp Data (blob) type
using Data = std::vector<std::byte>;

/// for identifying types that can iterate with a for-range
template <typename T>
concept is_iterable = requires (T t) { t.begin(); };

/// for identifying a pair
template <typename T>
concept is_pair = requires (T t) { t.first; };

/// for identifying an optional
template <typename T>
concept is_optional = requires (T t) { t.has_value(); };

/// for identifying a variant
template <typename T>
concept is_variant = requires (T t) { t.valueless_by_exception(); };

/// for identifying types that have a << operator defined
template <typename T>
concept is_outputtable = requires(std::ostream& os, T t) {
    os << t;
};

/// container is iterable but doesn't have an output defined (excludes std::string)template <typename T>
template <typename T>
concept is_container = is_iterable<T> && !is_outputtable<T>;

/// whether an ADL function on this type requires a namespace qualifier
template <typename T>
concept is_ambiguous_adl = is_optional<T> || is_variant<T> || is_pair<T> || is_container<T>;

template <typename T>
concept has_std_hash = requires(T t)
{
  { std::hash<T>{}(t) } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept has_hasher = requires (T t) {
  { T::_Hasher()(t) } -> std::convertible_to<std::size_t>;
};

/// Convert enum value to string name
template <typename T> requires std::is_enum_v<T>
auto enumToName(T e, const std::vector<std::string>& names) {
  return names[static_cast<uint16_t>(e)];
}

/// Convert string name to enum value
template <typename T> requires std::is_enum_v<T>
std::optional<T> enumFromName(const std::string_view& name, const std::vector<std::string>& names) {
  auto it = std::find(names.begin(), names.end(), name);
  if (it != names.end()) {
    return static_cast<T>(std::distance(names.begin(), it));
  } else {
    return {};
  }
}

/// Universal hash function for objects with a podgen _Hasher or std::hash specialization
template <typename T>
std::size_t hash(const T& obj) {
  if constexpr (has_hasher<T>) {
    return typename T::_Hasher()(obj);
  } else {
    return std::hash<T>()(obj);
  }
}

template <typename T>
std::size_t hashCombine(std::size_t s, const std::optional<T>& obj);

template <typename... Types>
std::size_t hashCombine(std::size_t s, const std::variant<Types...>& obj);

template <typename A, typename B>
std::size_t hashCombine(std::size_t s, const std::pair<A, B>& obj);

template <typename... Args>
std::size_t hashCombine(std::size_t s, const std::tuple<Args...>& obj);

template <typename T, typename... Args>
std::size_t hashCombine(std::size_t s, const T& obj, Args&&... args);

/// Universal hash combine function
template <typename T>
std::size_t hashCombine(std::size_t s, const T& obj) {
  if constexpr (is_iterable<T> && !has_std_hash<T> && !has_hasher<T>) {
    for (const auto& o : obj) {
      s = hashCombine(s, o);
    }
    return s;
  } else {
    return s ^ hash(obj) + 0x9e3779b9 + (s << 6) + (s >> 2);  // from boost::hash_combine
  }
}

/// Hash combine arbitrary varargs
template <typename T, typename... Args>
std::size_t hashCombine(std::size_t s, const T& obj, Args&&... args) {
  return hashCombine(hashCombine(s, obj), args...);
}

/// Hash arbitrary varargs together
template <typename T, typename... Args>
std::size_t hash(const T& obj, Args&&... args) {
  return hashCombine(0, obj, args...);
}

/// Hash combine an optional
template <typename T>
std::size_t hashCombine(std::size_t s, const std::optional<T>& obj) {
  return obj.has_value() ? hashCombine(s, *obj) : hashCombine(s, 0);
}

/// Hash combine a variant
template <typename... Types>
std::size_t hashCombine(std::size_t s, const std::variant<Types...>& obj) {
  return std::visit([s](auto&& arg) { return hashCombine(s, arg); }, obj);
}

/// Hash combine a pair
template <typename A, typename B>
std::size_t hashCombine(std::size_t s, const std::pair<A, B>& obj) {
  return hashCombine(s, obj.first, obj.second);
}

/// Hash combine a tuple
template <typename... Args>
std::size_t hashCombine(std::size_t s, const std::tuple<Args...>& obj) {
  return std::apply([s](auto&&... args) { return (hashCombine(s, args), ...); }, obj);
}

}  // namespace podgen

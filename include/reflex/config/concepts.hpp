// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <concepts>
#include <cstddef>
#include <limits>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include <boost/json/fwd.hpp>

#include <reflex/enum_indexed_fwd.hpp>
#include <reflex/meta.hpp>

#include <reflex/config/fwd.hpp>
#include <reflex/config/traits.hpp>

namespace reflex::config {

template<typename T>
concept unqualified = !std::is_const_v<T> && !std::is_volatile_v<T>;

template<typename T, typename... Types>
concept one_of = (... || std::same_as<T, Types>);

template<typename T>
concept numeric = std::numeric_limits<T>::digits != 0 &&
                  !one_of<std::remove_cv_t<T>, bool, char, char8_t, char16_t, char32_t, wchar_t>;

enum class numeric_category_e {
    signed_integral,
    unsigned_integral,
    floating_point,
};

template<numeric T>
struct numeric_category_of;

template<numeric T>
    requires(std::numeric_limits<T>::is_integer && std::numeric_limits<T>::is_signed)
struct numeric_category_of<T> : std::integral_constant<numeric_category_e, numeric_category_e::signed_integral> {};

template<numeric T>
    requires(std::numeric_limits<T>::is_integer && !std::numeric_limits<T>::is_signed)
struct numeric_category_of<T> : std::integral_constant<numeric_category_e, numeric_category_e::unsigned_integral> {};

template<numeric T>
    requires(!std::numeric_limits<T>::is_integer &&
             std::numeric_limits<T>::max_exponent != 0) // max_exponent is only valid for floating point types
struct numeric_category_of<T> : std::integral_constant<numeric_category_e, numeric_category_e::floating_point> {};

template<numeric T>
inline constexpr numeric_category_e numeric_category_of_v = numeric_category_of<T>::value;

template<typename T, typename U>
concept same_numeric_category_as = numeric<T> && numeric<U> && numeric_category_of_v<T> == numeric_category_of_v<U>;

template<typename T>
concept container = std::ranges::common_range<T> && !std::ranges::borrowed_range<T>;

template<typename T>
concept associative_container = container<T> && requires { typename T::key_type; };

template<typename T>
concept sequence_container = container<T> && !associative_container<T>;

template<typename T>
concept string_like = sequence_container<T> && std::ranges::contiguous_range<T> &&
                      std::same_as<std::ranges::range_value_t<T>, char>;

template<typename T>
concept map_like = associative_container<T> && requires { typename T::mapped_type; };

template<typename T>
concept set_like = associative_container<T> && !map_like<T>;

template<typename T>
concept array_like = requires(const T &arr, const std::size_t idx) {
    typename array_traits<T>::value_type;
    { array_traits<T>::extent } -> std::convertible_to<std::size_t>;
    std::size_t{array_traits<T>::extent}; // no narrowing conversions
    { arr[idx] } -> std::convertible_to<const typename array_traits<T>::value_type &>;
};

template<typename T>
concept optional_like = requires(const T &opt) {
    typename optional_traits<T>::value_type;
    bool(opt); // contextually convertible to bool
    bool(!opt);
    { *opt } -> std::convertible_to<const typename optional_traits<T>::value_type &>;
};

template<typename T>
inline constexpr bool is_variant = false;

template<typename... Ts>
inline constexpr bool is_variant<std::variant<Ts...>> = true;

template<typename T>
inline constexpr bool is_enum_indexed_array_v = false;

template<typename T, typename E>
inline constexpr bool is_enum_indexed_array_v<reflex::EnumIndexedArray<T, E>> = true;

namespace detail {

template<typename Array>
struct enum_indexed_array_traits;

template<typename T, typename E>
struct enum_indexed_array_traits<reflex::EnumIndexedArray<T, E>> {
    using value_type = std::remove_cv_t<T>;
    using key_type = E;
};

} // namespace detail

template<typename T>
concept custom_type = !std::derived_from<custom_traits<std::remove_cv_t<T>>, detail::unspecialized_custom_traits>;

template<typename T>
concept custom_type_with_to_string = custom_type<T> &&
                                     std::constructible_from<std::string_view,
                                                             decltype(custom_traits<std::remove_cv_t<T>>::to_string(
                                                                     std::declval<const std::remove_cv_t<T> &>()))>;

template<typename T>
concept custom_type_with_try_deserialize_string = custom_type<T> && requires(DeserializationContext &context,
                                                                             const context_entry_s &entry,
                                                                             const std::string_view str) {
    {
        custom_traits<std::remove_cv_t<T>>::try_deserialize_string(context, entry, str)
    } -> std::same_as<expected_t<std::remove_cv_t<T>>>;
};

namespace detail {

template<typename Handler, typename T, typename... Args>
[[nodiscard]] constexpr decltype(auto) handle_value(Args &&...args) {
    if constexpr (custom_type<T>) {
        return Handler::template handle_custom<T>(std::forward<Args>(args)...);
    } else if constexpr (std::same_as<T, std::monostate>) {
        return Handler::handle_monostate(std::forward<Args>(args)...);
    } else if constexpr (std::same_as<T, bool>) {
        return Handler::handle_bool(std::forward<Args>(args)...);
    } else if constexpr (numeric<T>) {
        return Handler::template handle_numeric<T>(std::forward<Args>(args)...);
    } else if constexpr (string_like<T>) {
        return Handler::template handle_string<T>(std::forward<Args>(args)...);
    } else if constexpr (optional_like<T>) {
        return Handler::template handle_optional<T>(std::forward<Args>(args)...);
    } else if constexpr (is_variant<T>) {
        return Handler::template handle_variant<T>(std::forward<Args>(args)...);
    } else if constexpr (std::same_as<T, boost::json::value>) {
        return Handler::handle_boost_json_value(std::forward<Args>(args)...);
    } else if constexpr (std::same_as<T, boost::json::object>) {
        return Handler::handle_boost_json_object(std::forward<Args>(args)...);
    } else if constexpr (std::same_as<T, boost::json::array>) {
        return Handler::handle_boost_json_array(std::forward<Args>(args)...);
    } else if constexpr (is_enum_indexed_array_v<T>) {
        return Handler::template handle_enum_indexed_array<T>(std::forward<Args>(args)...);
    } else if constexpr (array_like<T>) {
        return Handler::template handle_array<T>(std::forward<Args>(args)...);
    } else if constexpr (set_like<T>) {
        return Handler::template handle_set<T>(std::forward<Args>(args)...);
    } else if constexpr (map_like<T>) {
        return Handler::template handle_map<T>(std::forward<Args>(args)...);
    } else if constexpr (sequence_container<T>) {
        return Handler::template handle_sequence<T>(std::forward<Args>(args)...);
    } else if constexpr (reflex::reflected_enum<T>) {
        return Handler::template handle_enum<T>(std::forward<Args>(args)...);
    } else if constexpr (reflex::reflected_class<T>) {
        return Handler::template handle_class<T>(std::forward<Args>(args)...);
    } else {
        static_assert(false, "Unsupported type for [de]serialization/validation");
    }
}

} // namespace detail

} // namespace reflex::config

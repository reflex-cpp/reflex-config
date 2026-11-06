// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>

#include <reflex/meta.hpp>
#include <reflex/tuple.hpp>

#include <reflex/config/traits.hpp>

namespace reflex::config {

[[nodiscard]] constexpr std::string_view serialized_name(const auto &metadata) noexcept {
    if constexpr (reflex::has_property<rename, decltype(metadata.properties)>()) {
        return reflex::get<rename>(metadata.properties).name();
    } else {
        return metadata.name;
    }
}

template<reflex::reflected_enum E>
[[nodiscard]] constexpr std::optional<std::string_view> serialized_enumerator_name(const E e) noexcept {
    std::string_view result;
    const auto check_enumerator = [&](const E value, const std::string_view name) {
        if (e == value) {
            result = name;
            return true;
        }
        return false;
    };
    const auto found = reflex::apply(
            [&](const auto &...info) { return (... || check_enumerator(info.value, serialized_name(info))); },
            reflex::meta<E>::enumerators);
    if (!found) [[unlikely]] {
        return std::nullopt;
    }
    return result;
}

template<reflex::enumeration T, std::size_t EnumeratorCount>
struct enum_info_s {
    std::array<T, EnumeratorCount> values;
    std::array<std::string_view, EnumeratorCount> names;
};

template<reflex::enumeration T>
inline constexpr auto enum_info = reflex::apply(
        [](const auto &...enumerator) {
            return enum_info_s<T, sizeof...(enumerator)>{{enumerator.value...}, {serialized_name(enumerator)...}};
        },
        reflex::meta<T>::enumerators);

template<reflex::enumeration E>
struct enumerator_info_s {
    E value;
    std::string_view name;
};

// The names of E's enumerators, sorted by enumerator value.
template<reflex::enumeration E>
inline constexpr auto names_of_sorted_enumerators = reflex::apply(
        [](const auto &...enumerator) {
            auto entries = std::array<enumerator_info_s<E>, sizeof...(enumerator)>{
                    {{.value = enumerator.value, .name = serialized_name(enumerator)}...}};
            std::ranges::sort(entries, std::ranges::less{}, [](const auto &entry) {
                return std::to_underlying(entry.value);
            });
            std::array<std::string_view, sizeof...(enumerator)> names;
            std::ranges::transform(entries, names.begin(), [](const auto &entry) { return entry.name; });
            return names;
        },
        reflex::meta<E>::enumerators);

} // namespace reflex::config

// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include <reflex/meta.hpp>

namespace reflex::config {

template<typename T>
struct optional_traits {};

template<typename T>
struct optional_traits<std::optional<T>> {
    using value_type = std::remove_cv_t<T>;

    template<typename... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] static constexpr std::optional<T> construct(Args &&...args) //
            noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        return std::optional<T>(std::in_place, std::forward<Args>(args)...);
    }
};

template<typename T>
struct optional_traits<std::unique_ptr<T>> {
    using value_type = std::remove_cv_t<T>;

    template<typename... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] static constexpr std::unique_ptr<T> construct(Args &&...args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
};

template<typename T>
struct array_traits {};

template<typename T, std::size_t N>
struct array_traits<std::array<T, N>> {
    using value_type = std::remove_cv_t<T>;
    static constexpr auto extent = N;
};


namespace detail {

struct unspecialized_custom_traits {};

} // namespace detail

template<typename T>
struct custom_traits : detail::unspecialized_custom_traits {};

struct rename {
    const char *name_data;
    std::size_t name_size;

    constexpr rename(const std::string_view value) : name_data{value.data()}, name_size{value.size()} {}

    [[nodiscard]] constexpr std::string_view name() const noexcept { return {name_data, name_size}; }
};

struct ignore_t {
    explicit constexpr ignore_t() = default;
};

inline constexpr ignore_t ignore{};

template<typename MemberMeta>
[[nodiscard]] constexpr bool is_ignored() noexcept {
    return reflex::has_property<ignore_t, decltype(MemberMeta::properties)>();
}

} // namespace reflex::config

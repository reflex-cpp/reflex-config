// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <boost/json/value.hpp>

#include <reflex/config/context.hpp>
#include <reflex/config/errors.hpp>
#include <reflex/config/fixed_value.hpp>
#include <reflex/config/traits.hpp>
#include <reflex/config/validators.hpp>

namespace reflex::config {

enum class [[reflex::reflect]] test_enum : std::int32_t { first, second, third };

enum class [[reflex::reflect]] empty_enum {};

enum class [[reflex::reflect]] enum_with_rename {
    int_ [[reflex::prop(rename{"int"})]],
};

struct [[reflex::reflect]] custom_data_type {
    std::size_t count = 1;
    std::string name;
    const std::vector<bool> flags{};
    test_enum variant;

    [[nodiscard]] friend bool operator==(const custom_data_type &lhs, const custom_data_type &rhs) = default;
};
static_assert(!std::is_move_assignable_v<custom_data_type>);

struct [[reflex::reflect]] derived_from_custom_data_type : custom_data_type {
    int extra_value;

    [[nodiscard]] friend bool operator==(const derived_from_custom_data_type &lhs,
                                         const derived_from_custom_data_type &rhs) = default;
};

struct [[reflex::reflect]] class_with_non_assignable_member {
    custom_data_type member;

    [[nodiscard]] friend bool operator==(const class_with_non_assignable_member &lhs,
                                         const class_with_non_assignable_member &rhs) = default;
};

struct [[reflex::reflect]] empty_struct {
    [[nodiscard]] friend bool operator==(const empty_struct &lhs, const empty_struct &rhs) = default;
};

struct [[reflex::reflect]] inner_class_with_validation {
    [[reflex::prop(min{1uz})]] const std::size_t thread_count = 22;

    friend constexpr auto operator<=>(const inner_class_with_validation &lhs,
                                      const inner_class_with_validation &rhs) = default;
};

struct [[reflex::reflect]] outer_class_with_validation {
    const inner_class_with_validation inner{};
};

enum class [[reflex::reflect,
             reflex::prop(check_that{[](auto val) { return val != decltype(val){}; }, "not default initialized"})]] //
non_zero_enum : std::int32_t{
        first = 1,
        second = 2,
};

struct [[reflex::reflect]] int_pair {
    int a;
    int b;
    [[reflex::prop(ignore)]] int c;
};

struct [[reflex::reflect, reflex::prop(custom_check{[](const auto &context, const auto &self) -> expected_t<void> {
             if (self.first == self.second) {
                 return format_validation_error(context, "first must not equal second ({})", self.second);
             }
             return {};
         }})]] //
class_with_invariant {
    std::size_t first;
    std::size_t second;

    friend bool operator==(const class_with_invariant &, const class_with_invariant &) = default;
};


struct [[reflex::reflect]] class_with_rename {
    [[using reflex: prop(rename{"int"}), prop(min{100})]] int _int;

    [[nodiscard]] friend bool operator==(const class_with_rename &lhs, const class_with_rename &rhs) = default;
};


class CustomClass {
public:
    explicit CustomClass(const int val) noexcept : m_val{val} {}

    [[nodiscard]] int val() const noexcept { return m_val; }

    friend bool operator==(const CustomClass &, const CustomClass &) = default;
    friend auto operator<=>(const CustomClass &, const CustomClass &) = default;

private:
    int m_val;
};

template<>
struct custom_traits<CustomClass> {
    static expected_t<boost::json::value> try_serialize(Context & /*context*/,
                                                        const context_entry_s & /*entry*/,
                                                        const CustomClass &value,
                                                        const boost::json::storage_ptr &memory_resource) {
        return expected_t<boost::json::value>{std::in_place, value.val(), memory_resource};
    }

    static expected_t<CustomClass> try_deserialize(DeserializationContext &context,
                                                   const context_entry_s &entry,
                                                   const boost::json::value &value) {
        if (auto exp_val = try_deserialize_value<int>(context, entry, value)) {
            return CustomClass{*exp_val};
        } else {
            return std::unexpected{std::move(exp_val).error()};
        }
    }

    static expected_t<void> try_validate(Context & /*context*/,
                                         const context_entry_s & /*entry*/,
                                         const CustomClass & /*value*/) {
        return {};
    }
};

struct [[reflex::reflect]] class_with_ignored_member {
    int field;
    [[reflex::prop(ignore)]] int ignored_field = 42;
};

template<bool Optimized>
class CustomStringTag {
public:
    explicit CustomStringTag(std::string str) : m_value{std::move(str)} {}

    [[nodiscard]] std::string_view value() const noexcept { return m_value; }
    [[nodiscard]] bool empty() const noexcept { return m_value.empty(); }

    [[nodiscard]] friend auto operator<=>(const CustomStringTag &lhs, const CustomStringTag &rhs) noexcept = default;

private:
    std::string m_value;
};

template<bool Optimized>
struct custom_traits<CustomStringTag<Optimized>> {
    static expected_t<boost::json::value> try_serialize(Context & /*context*/,
                                                        const context_entry_s & /*entry*/,
                                                        const CustomStringTag<Optimized> &value,
                                                        const boost::json::storage_ptr &memory_resource) {
        return expected_t<boost::json::value>{std::in_place, value.value(), memory_resource};
    }

    static expected_t<CustomStringTag<Optimized>> try_deserialize(DeserializationContext &context,
                                                                  const context_entry_s &entry,
                                                                  const boost::json::value &value) {
        if (auto exp_str = try_deserialize_value<std::string>(context, entry, value)) {
            return CustomStringTag<Optimized>{*std::move(exp_str)};
        } else {
            return std::unexpected{std::move(exp_str).error()};
        }
    }

    // Use std::vector<char> to check support for explicit conversion to std::string_view via range constructor.
    [[nodiscard]] static std::vector<char> to_string(const CustomStringTag<Optimized> &value) noexcept
        requires Optimized
    {
        return std::vector(value.value().begin(), value.value().end());
    }

    static expected_t<CustomStringTag<Optimized>> try_deserialize_string(DeserializationContext & /*context*/,
                                                                         const context_entry_s & /*entry*/,
                                                                         const std::string_view value)
        requires Optimized
    {
        return CustomStringTag<Optimized>{std::string{value}};
    }

    static expected_t<void> try_validate(Context & /*context*/,
                                         const context_entry_s & /*entry*/,
                                         const CustomStringTag<Optimized> & /*value*/) {
        return {};
    }
};

template<typename T>
struct custom_allocator {
    using value_type = T;

    custom_allocator() = default;

    template<typename U>
    explicit(false) constexpr custom_allocator(const custom_allocator<U> &) noexcept {}

    [[nodiscard]] T *allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length{};
        }
        if constexpr (alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            return static_cast<T *>(::operator new(n * sizeof(T)));
        } else {
            return static_cast<T *>(::operator new(n * sizeof(T), std::align_val_t{alignof(T)}));
        }
    }

    void deallocate(T *const p, const std::size_t n) noexcept {
        if constexpr (alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            return ::operator delete(static_cast<void *>(p), n * sizeof(T));
        } else {
            return ::operator delete(static_cast<void *>(p), n * sizeof(T), std::align_val_t{alignof(T)});
        }
    }
};

template<typename T, typename U>
constexpr bool operator==(const custom_allocator<T> &, const custom_allocator<U> &) noexcept {
    return true;
}

template<typename T>
struct custom_number {
    custom_number() = default;
    custom_number(const T &v) noexcept : value(v) {}
    operator T &() noexcept { return value; }
    operator const T &() const noexcept { return value; }
    T value;
};

struct [[reflex::reflect]] first_alternative {
    FixedValue<test_enum::first> type;
    int x;

    friend bool operator==(const first_alternative &, const first_alternative &) = default;
};

struct [[reflex::reflect]] second_alternative {
    FixedValue<test_enum::second> type;
    int x;

    friend bool operator==(const second_alternative &, const second_alternative &) = default;
};

} // namespace reflex::config

template<typename T>
class std::numeric_limits<reflex::config::custom_number<T>> : public std::numeric_limits<T> {};

#ifndef __REFLEX_TOOL__
    #include "test_types.meta.inc"
#endif
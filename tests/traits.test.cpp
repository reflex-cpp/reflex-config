// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <atomic>
#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <reflex/config/traits.hpp>

namespace reflex::config {

namespace {

using namespace std::string_view_literals;

struct immovable_type {
    explicit immovable_type(const int value_) noexcept : value{value_} {}

    std::atomic_int value;
};

} // namespace

TEST_CASE("optional_traits allow creating an std::optional") {
    using opt_t = std::optional<std::string>;
    using traits_t = optional_traits<opt_t>;
    STATIC_REQUIRE(std::same_as<typename traits_t::value_type, std::string>);
    constexpr auto test_str = "Totally a string"sv;
    STATIC_REQUIRE(std::same_as<decltype(traits_t::construct(test_str)), std::optional<std::string>>);
    REQUIRE(traits_t::construct(test_str).value() == test_str);
}

TEST_CASE("optional_traits allow creating an std::optional of an immovable type") {
    using opt_t = std::optional<immovable_type>;
    using traits_t = optional_traits<opt_t>;
    STATIC_REQUIRE(std::same_as<typename traits_t::value_type, immovable_type>);
    constexpr auto test_val = 123;
    REQUIRE(traits_t::construct(test_val).value().value == test_val);
}

TEST_CASE("optional_traits allow creating a std::unique_ptr") {
    using vec_t = std::vector<int>;
    using ptr_t = std::unique_ptr<vec_t>;
    using traits_t = optional_traits<ptr_t>;
    STATIC_REQUIRE(std::same_as<typename traits_t::value_type, vec_t>);
    const auto test_vec = std::vector{1, 2, 3};
    const auto ptr = traits_t::construct(test_vec);
    REQUIRE(ptr);
    REQUIRE(*ptr == test_vec);
}

TEST_CASE("optional_traits allow creating a std::unique_ptr of an immovable type") {
    using ptr_t = std::unique_ptr<immovable_type>;
    using traits_t = optional_traits<ptr_t>;
    STATIC_REQUIRE(std::same_as<typename traits_t::value_type, immovable_type>);
    constexpr auto test_val = -42;
    const auto ptr = traits_t::construct(test_val);
    REQUIRE(ptr);
    REQUIRE(ptr->value == test_val);
}

TEST_CASE("array_traits defined value_type and extent for std::array") {
    constexpr auto extent = 4uz;
    using array_t = std::array<std::string, extent>;
    using traits_t = array_traits<array_t>;
    STATIC_REQUIRE(std::same_as<typename traits_t::value_type, std::string>);
    STATIC_REQUIRE(traits_t::extent == extent);
}

} // namespace reflex::config

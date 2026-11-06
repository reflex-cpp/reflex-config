// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <reflex/enum_indexed_array.hpp>

#include <reflex/config/context.hpp>
#include <reflex/config/errors.hpp>
#include <reflex/config/validate.hpp>
#include <reflex/config/validators.hpp>

#include "error_helpers.hpp"
#include "test_types.hpp"

namespace reflex::config {

namespace {
using namespace std::string_literals;
}

TEST_CASE("Validating a bool") {
    auto context = Context{};
    REQUIRE(try_validate_value(context, {}, true));
    REQUIRE(try_validate_value(context, {}, false));
}

TEST_CASE("Validating numbers") {
    auto context = Context{};
    REQUIRE(try_validate_value(context, {}, 72u));
    REQUIRE(try_validate_value(context, {}, -43));
    REQUIRE(try_validate_value(context, {}, 13.6667));
}

TEST_CASE("Validating strings") {
    auto context = Context{};
    REQUIRE(try_validate_value(context, {}, std::string{"Hello, World!"}));
    using custom_string_t = std::basic_string<char, std::char_traits<char>, custom_allocator<char>>;
    REQUIRE(try_validate_value(context, {}, custom_string_t{"A super custom string"}));
}

TEST_CASE("Validating optional-like types") {
    auto context = Context{};
    {
        const auto not_a_string = std::optional<std::string>{};
        REQUIRE(try_validate_value(context, {}, not_a_string));
    }
    {
        const auto actually_a_string = std::optional<std::string>{"nullopt"};
        REQUIRE(try_validate_value(context, {}, actually_a_string));
    }
    {
        const auto not_a_class = std::optional<inner_class_with_validation>{};
        REQUIRE(try_validate_value(context, {}, not_a_class));
    }
    {
        const auto actually_a_class = std::optional{inner_class_with_validation{.thread_count = 10}};
        REQUIRE(try_validate_value(context, {}, actually_a_class));
    }
    {
        const auto actually_an_invalid_class = std::optional{inner_class_with_validation{.thread_count = 0}};
        REQUIRE_THAT(try_validate_value(context, {}, actually_an_invalid_class),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("\"/thread_count\"") &&
                             ExpectedErrorContains("min 1"));
    }
    {
        const auto null_ptr_to_str = std::unique_ptr<std::string>{};
        REQUIRE(try_validate_value(context, {}, null_ptr_to_str));
    }
    {
        const auto ptr_to_a_valid_class = std::make_unique<inner_class_with_validation>(
                inner_class_with_validation{.thread_count = 1});
        REQUIRE(try_validate_value(context, {}, ptr_to_a_valid_class));
    }
    {
        const auto ptr_to_an_invalid_class = std::make_unique<inner_class_with_validation>(
                inner_class_with_validation{.thread_count = 0});
        REQUIRE_THAT(try_validate_value(context, {}, ptr_to_an_invalid_class),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("\"/thread_count\"") &&
                             ExpectedErrorContains("min 1"));
    }
}

TEST_CASE("Validating variants") {
    auto context = Context{};
    {
        const auto plain_variant = std::variant<int, std::string>{"Hello"};
        REQUIRE(try_validate_value(context, {}, plain_variant));
    }
    {
        const auto variant_with_alternative_validation = std::variant<int, inner_class_with_validation>{
                inner_class_with_validation{.thread_count = 1}};
        REQUIRE(try_validate_value(context, {}, variant_with_alternative_validation));
    }
    {
        const auto variant_with_alternative_validation = std::variant<int, inner_class_with_validation>{
                inner_class_with_validation{.thread_count = 0}};
        REQUIRE_THAT(try_validate_value(context, {}, variant_with_alternative_validation),
                     ExpectedErrorContains("\"/thread_count\"") && ExpectedErrorContains(strings::validation_error) &&
                             ExpectedErrorContains("min 1"));
    }
}

TEST_CASE("Validating boost::json::value") {
    auto context = Context{};
    REQUIRE(try_validate_value(context, {}, boost::json::value(12345)));
    REQUIRE(try_validate_value(context, {}, boost::json::value(nullptr)));
    REQUIRE(try_validate_value(context, {}, boost::json::value("My text")));
    REQUIRE(try_validate_value(context, {}, boost::json::value{1, 2, "three"}));
}

TEST_CASE("Validating boost::json::object") {
    auto context = Context{};
    REQUIRE(try_validate_value(context, {}, boost::json::object{{"first", 1}, {"second", "2"}}));
}

TEST_CASE("Validating boost::json::array") {
    auto context = Context{};
    REQUIRE(try_validate_value(context, {}, boost::json::array{1, "two", 3, nullptr}));
}

TEST_CASE("Validating reflex::EnumIndexedArray") {
    auto context = Context{};
    {
        const auto arr = reflex::EnumIndexedArray<int, test_enum>{1, 2, 3};
        REQUIRE(try_validate_value(context, {}, arr));
    }
    {
        const auto arr = reflex::EnumIndexedArray<inner_class_with_validation, test_enum>{
                inner_class_with_validation{.thread_count = 1},
                inner_class_with_validation{.thread_count = 2},
                inner_class_with_validation{.thread_count = 3}};
        REQUIRE(try_validate_value(context, {}, arr));
    }
    {
        const auto arr = reflex::EnumIndexedArray<inner_class_with_validation, test_enum>{
                inner_class_with_validation{.thread_count = 2},
                inner_class_with_validation{.thread_count = 1},
                inner_class_with_validation{.thread_count = 0}};
        REQUIRE_THAT(try_validate_value(context, {}, arr),
                     ExpectedErrorContains(strings::validation_error) &&
                             ExpectedErrorContains("\"/third/thread_count\"") && ExpectedErrorContains("min 1"));
    }
}

TEST_CASE("Validating array-like types") {
    auto context = Context{};
    {
        const auto int_array = std::array{1, 2, 3};
        REQUIRE(try_validate_value(context, {}, int_array));
    }
    {
        const auto string_array = std::array{"Hello,"s, "World!"s};
        REQUIRE(try_validate_value(context, {}, string_array));
    }
    {
        const auto class_array = std::array{inner_class_with_validation{.thread_count = 1},
                                            inner_class_with_validation{.thread_count = 2},
                                            inner_class_with_validation{.thread_count = 4}};
        REQUIRE(try_validate_value(context, {}, class_array));
    }
    {
        const auto invalid_class_array = std::array{inner_class_with_validation{.thread_count = 1},
                                                    inner_class_with_validation{.thread_count = 0},
                                                    inner_class_with_validation{.thread_count = 4}};
        REQUIRE_THAT(try_validate_value(context, {}, invalid_class_array),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("\"/1/thread_count\"") &&
                             ExpectedErrorContains("min 1"));
    }
}

TEST_CASE("Validating set-like types") {
    auto context = Context{};
    {
        const auto empty_set = std::set<int>{};
        REQUIRE(try_validate_value(context, {}, empty_set));
    }
    {
        const auto valid_class_set = std::set{inner_class_with_validation{.thread_count = 1},
                                              inner_class_with_validation{.thread_count = 10}};
        REQUIRE(try_validate_value(context, {}, valid_class_set));
    }
    {
        const auto invalid_class_set = std::set{inner_class_with_validation{.thread_count = 1},
                                                inner_class_with_validation{.thread_count = 0}};
        REQUIRE_THAT(try_validate_value(context, {}, invalid_class_set),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("\"/0/thread_count\"") &&
                             ExpectedErrorContains("min 1"));
    }
}

TEST_CASE("Validating map-like types") {
    auto context = Context{};
    {
        const auto empty_map = std::map<std::string, std::size_t>{};
        REQUIRE(try_validate_value(context, {}, empty_map));
    }
    {
        const auto valid_class_map = std::map<std::string, inner_class_with_validation>{
                {"seven"s, inner_class_with_validation{.thread_count = 7}},
                {"eleven"s, inner_class_with_validation{.thread_count = 11}}};
        REQUIRE(try_validate_value(context, {}, valid_class_map));
    }
    {
        const auto invalid_class_map = std::map<std::string, inner_class_with_validation>{
                {"seven"s, inner_class_with_validation{.thread_count = 7}},
                {"zero"s, inner_class_with_validation{.thread_count = 0}}};
        REQUIRE_THAT(try_validate_value(context, {}, invalid_class_map),
                     ExpectedErrorContains(strings::validation_error) &&
                             ExpectedErrorContains("\"/zero/thread_count\"") && ExpectedErrorContains("min 1"));
    }
}

TEST_CASE("Validating map-like types with non-string keys") {
    auto context = Context{};
    {
        const auto enum_map = std::map<non_zero_enum, inner_class_with_validation>{
                {non_zero_enum::first, inner_class_with_validation{.thread_count = 10}}};
        REQUIRE(try_validate_value(context, {}, enum_map));
    }
    {
        const auto map_with_invalid_key = std::map<non_zero_enum, inner_class_with_validation>{
                {non_zero_enum{}, inner_class_with_validation{.thread_count = 1}}};
        REQUIRE_THAT(try_validate_value(context, {}, map_with_invalid_key),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("\"/\"") &&
                             ExpectedErrorContains("not default initialized"));
    }
    {
        const auto map_with_invalid_value = std::map<non_zero_enum, inner_class_with_validation>{
                {non_zero_enum::first, inner_class_with_validation{.thread_count = 0}}};
        REQUIRE_THAT(try_validate_value(context, {}, map_with_invalid_value),
                     ExpectedErrorContains(strings::validation_error) &&
                             ExpectedErrorContains("\"/first/thread_count\"") && ExpectedErrorContains("min 1"));
    }
    {
        const auto map_with_custom_string_key = std::map<CustomStringTag<false>, inner_class_with_validation>{
                {CustomStringTag<false>{"hello"}, inner_class_with_validation{.thread_count = 0}}};
        REQUIRE_THAT(try_validate_value(context, {}, map_with_custom_string_key),
                     ExpectedErrorContains(strings::validation_error) &&
                             ExpectedErrorContains("\"/hello/thread_count\"") && ExpectedErrorContains("min 1"));
    }
    {
        const auto map_with_custom_string_key = std::map<CustomStringTag<true>, inner_class_with_validation>{
                {CustomStringTag<true>{"hello"}, inner_class_with_validation{.thread_count = 0}}};
        REQUIRE_THAT(try_validate_value(context, {}, map_with_custom_string_key),
                     ExpectedErrorContains(strings::validation_error) &&
                             ExpectedErrorContains("\"/hello/thread_count\"") && ExpectedErrorContains("min 1"));
    }
}

TEST_CASE("Validating sequence containers") {
    auto context = Context{};
    {
        const auto empty_vector = std::vector<float>{};
        REQUIRE(try_validate_value(context, {}, empty_vector));
    }
    {
        const auto valid_class_deque = std::deque{inner_class_with_validation{.thread_count = 77},
                                                  inner_class_with_validation{.thread_count = 1}};
        REQUIRE(try_validate_value(context, {}, valid_class_deque));
    }
    {
        const auto invalid_class_deque = std::deque{inner_class_with_validation{.thread_count = 77},
                                                    inner_class_with_validation{.thread_count = 0}};
        REQUIRE_THAT(try_validate_value(context, {}, invalid_class_deque),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("\"/1/thread_count\"") &&
                             ExpectedErrorContains("min 1"));
    }
}

TEST_CASE("Validating an enum") {
    auto context = Context{};
    REQUIRE(try_validate_value(context, {}, test_enum::first));
    REQUIRE(try_validate_value(context, {}, test_enum::second));
    REQUIRE(try_validate_value(context, {}, static_cast<test_enum>(123)));

    REQUIRE(try_validate_value(context, {}, non_zero_enum::first));
    REQUIRE(try_validate_value(context, {}, non_zero_enum::second));

    REQUIRE_THAT(try_validate_value(context, {}, non_zero_enum{}),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("\"\"") &&
                         ExpectedErrorContains("not default initialized"));
}

TEST_CASE("Validating a class") {
    auto context = Context{};

    {
        const auto valid_inner = inner_class_with_validation{};
        REQUIRE(try_validate_value(context, {}, valid_inner));
    }
    {
        const auto valid_outer = outer_class_with_validation{};
        REQUIRE(try_validate_value(context, {}, valid_outer));
    }
    {
        const auto invalid_inner = inner_class_with_validation{.thread_count = 0};
        REQUIRE_THAT(try_validate_value(context, {}, invalid_inner),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("\"/thread_count\"") &&
                             ExpectedErrorContains("min 1"));
    }
    {
        const auto invalid_outer = outer_class_with_validation{.inner{.thread_count = 0}};
        REQUIRE_THAT(try_validate_value(context, {}, invalid_outer),
                     ExpectedErrorContains(strings::validation_error) &&
                             ExpectedErrorContains("\"/inner/thread_count\"") && ExpectedErrorContains("min 1"));
    }
    {
        const auto valid_obj = class_with_invariant{.first = 0, .second = 1};
        REQUIRE(try_validate_value(context, {}, valid_obj));
    }
    {
        const auto invalid_obj = class_with_invariant{.first = 42, .second = 42};
        REQUIRE_THAT(try_validate_value(context, {}, invalid_obj),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("\"\"") &&
                             ExpectedErrorContains("first must not equal second (42)"));
    }
    {
        const auto invalid_obj = class_with_rename{._int = 0};
        REQUIRE_THAT(try_validate_value(context, {}, invalid_obj),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("\"/int\""));
    }
}

} // namespace reflex::config

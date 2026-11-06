// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <boost/json/value.hpp>
#include <fmt/format.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <reflex/enum_indexed_array.hpp>

#include <reflex/config/context.hpp>
#include <reflex/config/errors.hpp>
#include <reflex/config/validators.hpp>

#include "error_helpers.hpp"
#include "test_types.hpp"

namespace reflex::config {

using Catch::Matchers::ContainsSubstring;

TEST_CASE("Constructing and using the check_values validator wrapper") {
    auto ctx = Context{};
    const auto g0 = ContextGuard{ctx, {}};
    const auto g1 = ContextGuard{ctx, {.id = "values"}};

    constexpr auto check_values_positive = check_values(greater_than(0));
    constexpr auto check_values_not_empty = check_values(
            check_that([](const auto &value) { return !value.empty(); }, "not empty"));
    constexpr auto check_values_not_null = check_values(
            check_that([](const boost::json::value &value) { return value.kind() != boost::json::kind::null; },
                       "not null"));
    constexpr auto check_values_not_second = check_values(
            check_that([](const test_enum e) { return e != test_enum::second; }, "not second"));
    constexpr auto check_values_true = check_values(check_that([](const bool &b) { return b; }, "must be true"));

    SECTION("int") {
        REQUIRE(check_values_positive.validate(ctx, 10));
        REQUIRE_THAT(check_values_positive.validate(ctx, 0),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values\""));
        REQUIRE_THAT(check_values_positive.validate(ctx, -10),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values\""));
    }

    SECTION("bool") {
        REQUIRE(check_values_true.validate(ctx, true));
        REQUIRE_THAT(check_values_true.validate(ctx, false),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("must be true") &&
                             ExpectedErrorContains("\"/values\""));
    }

    SECTION("std::string") {
        REQUIRE(check_values_not_empty.validate(ctx, std::string{"hello"}));
        REQUIRE_THAT(check_values_not_empty.validate(ctx, std::string{}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("not empty") &&
                             ExpectedErrorContains("\"/values\""));
    }

    SECTION("reflected_enum") {
        REQUIRE(check_values_not_second.validate(ctx, test_enum::first));
        REQUIRE_THAT(check_values_not_second.validate(ctx, test_enum::second),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("not second") &&
                             ExpectedErrorContains("\"/values\""));
    }

    SECTION("custom_type") {
        constexpr auto check_values_val_positive = check_values(
                check_that([](const CustomClass &cc) { return cc.val() > 0; }, "val is positive"));
        REQUIRE(check_values_val_positive.validate(ctx, CustomClass{42}));
        REQUIRE_THAT(check_values_val_positive.validate(ctx, CustomClass{-1}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("val is positive") &&
                             ExpectedErrorContains("\"/values\""));
    }

    SECTION("std::optional<int>") {
        REQUIRE(check_values_positive.validate(ctx, std::optional<int>{}));
        REQUIRE(check_values_positive.validate(ctx, std::optional{10}));
        REQUIRE_THAT(check_values_positive.validate(ctx, std::optional{-10}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values\""));
        REQUIRE_THAT(check_values_positive.validate(ctx, std::optional{0}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values\""));
    }

    SECTION("std::variant<std::vector, std::map>") {
        using variant_t = std::variant<std::vector<int>, std::map<std::string, int>>;
        REQUIRE(check_values_not_empty.validate(ctx, variant_t{std::vector{1, 2, 3}}));
        REQUIRE(check_values_not_empty.validate(ctx, variant_t{std::map<std::string, int>{{"one", 1}}}));
        REQUIRE_THAT(check_values_not_empty.validate(ctx, variant_t{std::vector<int>{}}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("not empty") &&
                             ExpectedErrorContains("\"/values\""));
        REQUIRE_THAT(check_values_not_empty.validate(ctx, variant_t{std::map<std::string, int>{}}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("not empty") &&
                             ExpectedErrorContains("\"/values\""));
    }

    SECTION("boost::json::value") {
        constexpr auto is_truthy = []<typename T>(const T &value) {
            if constexpr (one_of<T, bool, std::int64_t, std::uint64_t, double>) {
                return value != static_cast<T>(0);
            } else if constexpr (one_of<T, boost::json::string, boost::json::array, boost::json::object>) {
                return !value.empty();
            } else {
                static_assert(false);
            }
        };
        constexpr auto check_values_truthy = check_values{check_that{is_truthy, "is truthy"}};

        REQUIRE(check_values_truthy.validate(ctx, boost::json::value{})); // null is empty - not checked
        REQUIRE(check_values_truthy.validate(ctx, boost::json::value(true)));
        REQUIRE(check_values_truthy.validate(ctx, boost::json::value(1)));
        REQUIRE(check_values_truthy.validate(ctx, boost::json::value{1, 2, nullptr}));
        REQUIRE(check_values_truthy.validate(ctx, boost::json::value{{"one", 1}, {"two", 2}}));
        REQUIRE(check_values_truthy.validate(ctx, boost::json::value("hello")));
        REQUIRE_THAT(check_values_truthy.validate(ctx, boost::json::value(false)),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("is truthy") &&
                             ExpectedErrorContains("\"/values\""));
        REQUIRE_THAT(check_values_truthy.validate(ctx, boost::json::value("")),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("is truthy") &&
                             ExpectedErrorContains("\"/values\""));
        REQUIRE_THAT(check_values_truthy.validate(ctx, boost::json::value(boost::json::array_kind)),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("is truthy") &&
                             ExpectedErrorContains("\"/values\""));
        REQUIRE_THAT(check_values_truthy.validate(ctx, boost::json::value(boost::json::object_kind)),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("is truthy") &&
                             ExpectedErrorContains("\"/values\""));
    }

    SECTION("boost::json::object") {
        REQUIRE(check_values_not_null.validate(ctx, boost::json::object{{"key", boost::json::value(1)}}));
        REQUIRE_THAT(check_values_not_null.validate(ctx, boost::json::object{{"key", boost::json::value()}}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("not null") &&
                             ExpectedErrorContains("\"/values/key\""));
    }

    SECTION("boost::json::array") {
        REQUIRE(check_values_not_null.validate(ctx, boost::json::array{boost::json::value(1)}));
        REQUIRE_THAT(check_values_not_null.validate(ctx, boost::json::array{boost::json::value()}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("not null") &&
                             ExpectedErrorContains("\"/values/0\""));
    }

    SECTION("reflex::EnumIndexedArray") {
        using array_t = reflex::EnumIndexedArray<int, test_enum>;
        REQUIRE(check_values_positive.validate(ctx, array_t{1, 2, 3}));
        REQUIRE_THAT(check_values_positive.validate(ctx, array_t{0, 1, 2}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values/first\""));
    }

    SECTION("std::array<int>") {
        using array_t = std::array<int, 2>;
        REQUIRE(check_values_positive.validate(ctx, array_t{1, 2}));
        REQUIRE_THAT(check_values_positive.validate(ctx, array_t{1, 0}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values/1\""));
    }

    SECTION("std::deque<int>") {
        using deque_t = std::deque<int>;
        REQUIRE(check_values_positive.validate(ctx, deque_t{1, 2}));
        REQUIRE_THAT(check_values_positive.validate(ctx, deque_t{1, 0}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values/1\""));
    }

    SECTION("std::set<int>") {
        using set_t = std::set<int>;
        REQUIRE(check_values_positive.validate(ctx, set_t{1, 2, 77}));
        REQUIRE_THAT(check_values_positive.validate(ctx, set_t{1, 2, -10}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values/0\""));
    }

    SECTION("std::map<std::string, int>") {
        using map_t = std::map<std::string, int>;
        REQUIRE(check_values_positive.validate(ctx, map_t{{"one", 1}, {"two", 2}}));
        REQUIRE_THAT(check_values_positive.validate(ctx, map_t{{"one", 1}, {"zero", 0}}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values/zero\""));
    }

    SECTION("std::map<std::varint<std::string>, int>") {
        using map_t = std::map<std::variant<std::string>, int>;
        REQUIRE(check_values_positive.validate(ctx, map_t{{"one", 1}, {"two", 2}}));
        REQUIRE_THAT(check_values_positive.validate(ctx, map_t{{"one", 1}, {"zero", 0}}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values/zero\""));
    }

    SECTION("std::map<CustomStringTag<Optimized = false>, int>") {
        using tag_t = CustomStringTag<false>;
        using map_t = std::map<tag_t, int>;
        REQUIRE(check_values_positive.validate(ctx, map_t{{tag_t{"one"}, 1}, {tag_t{"two"}, 2}}));
        REQUIRE_THAT(check_values_positive.validate(ctx, map_t{{tag_t{"one"}, 1}, {tag_t{"zero"}, 0}}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values/zero\""));
    }

    SECTION("std::map<CustomStringTag<Optimized = true>, int>") {
        using tag_t = CustomStringTag<true>;
        using map_t = std::map<tag_t, int>;
        REQUIRE(check_values_positive.validate(ctx, map_t{{tag_t{"one"}, 1}, {tag_t{"two"}, 2}}));
        REQUIRE_THAT(check_values_positive.validate(ctx, map_t{{tag_t{"one"}, 1}, {tag_t{"zero"}, 0}}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values/zero\""));
    }

    SECTION("std::map<reflected_enum, int>") {
        using map_t = std::map<test_enum, int>;
        REQUIRE(check_values_positive.validate(ctx, map_t{{test_enum::first, 1}}));
        REQUIRE_THAT(check_values_positive.validate(ctx, map_t{{test_enum::first, 1}, {test_enum::second, 0}}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values/second\""));
    }

    SECTION("reflected_class") {
        REQUIRE(check_values_positive.validate(ctx, int_pair{1, 2, -100}));
        REQUIRE_THAT(check_values_positive.validate(ctx, int_pair{42, -10, 100}),
                     ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 0") &&
                             ExpectedErrorContains("\"/values/b\""));
    }
}

TEST_CASE("Constructing and using the custom_check validator") {
    auto ctx = Context{};
    constexpr auto first_does_not_equal_second = custom_check{
            [](const auto &context, const auto &pair) -> expected_t<void> {
                if (pair.first == pair.second) {
                    return format_validation_error(context, "first must not equal second ({})", pair.second);
                }
                return {};
            }};
    REQUIRE(first_does_not_equal_second.validate(ctx, std::pair{42, 43}));
    REQUIRE_THAT(first_does_not_equal_second.validate(ctx, std::pair{42, 42}),
                 ExpectedErrorContains(strings::validation_error) &&
                         ExpectedErrorContains("first must not equal second (42)"));
}

TEST_CASE("Constructing and using the check_that validator") {
    auto ctx = Context{};
    constexpr auto is_even = check_that{[](const std::int32_t val) { return (val % 2) == 0; }, "is even"};
    REQUIRE(is_even.validate(ctx, 2));
    REQUIRE_THAT(is_even.validate(ctx, 3),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("is even"));

    constexpr auto first_does_not_equal_second = check_that{
            [](const auto &pair) -> decltype(pair.first != pair.second) { return pair.first != pair.second; },
            "first does not equal second"};
    REQUIRE(first_does_not_equal_second.validate(ctx, std::pair{std::string{"Hello"}, std::string{"World"}}));
    REQUIRE_THAT(
            first_does_not_equal_second.validate(ctx, std::pair{1.0f, 1.0f}),
            ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("first does not equal second"));
}

TEST_CASE("Constructing and using the signed min validator") {
    auto ctx = Context{};
    constexpr auto min_1 = min{1};
    REQUIRE(min_1.validate(ctx, 1));
    REQUIRE(min_1.validate(ctx, std::int16_t{123}));
    REQUIRE_THAT(min_1.validate(ctx, std::int8_t{-42}),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("min 1"));
    REQUIRE_THAT(min_1.validate(ctx, 0),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("min 1"));
}

TEST_CASE("Constructing and using the unsigned min validator") {
    auto ctx = Context{};
    constexpr auto min_2 = min{2uz};
    REQUIRE(min_2.validate(ctx, 2u));
    REQUIRE(min_2.validate(ctx, 77uz));
    REQUIRE(min_2.validate(ctx, std::uint8_t{42}));
    REQUIRE_THAT(min_2.validate(ctx, 1u),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("min 2"));
    REQUIRE_THAT(min_2.validate(ctx, std::uint16_t{1}),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("min 2"));
}

TEST_CASE("Constructing and using the floating point min validator") {
    auto ctx = Context{};
    constexpr auto min_neg1_337 = min{-1.337f};
    REQUIRE(min_neg1_337.validate(ctx, -1.0));
    REQUIRE(min_neg1_337.validate(ctx, -0.5l));
    REQUIRE(min_neg1_337.validate(ctx, -0.7f));
    REQUIRE_THAT(min_neg1_337.validate(ctx, -1.4),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("min -1.337"));
    REQUIRE_THAT(min_neg1_337.validate(ctx, -1337.0l),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("min -1.337"));
}

TEST_CASE("Constructing and using the signed max validator") {
    auto ctx = Context{};
    constexpr auto max_neg55 = max{std::int8_t{-55}};
    REQUIRE(max_neg55.validate(ctx, -1337));
    REQUIRE(max_neg55.validate(ctx, -7894561234567567856));
    REQUIRE(max_neg55.validate(ctx, std::int8_t{-55}));
    REQUIRE_THAT(max_neg55.validate(ctx, std::int64_t{-54}),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("max -55"));
    REQUIRE_THAT(max_neg55.validate(ctx, 0),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("max -55"));
    REQUIRE_THAT(max_neg55.validate(ctx, std::int16_t{77}),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("max -55"));
}


TEST_CASE("Constructing and using the unsigned max validator") {
    auto ctx = Context{};
    constexpr auto max_123456 = max{std::uint32_t{123456}};
    REQUIRE(max_123456.validate(ctx, 123456u));
    REQUIRE(max_123456.validate(ctx, 0uz));
    REQUIRE(max_123456.validate(ctx, std::uint8_t{42}));
    REQUIRE_THAT(max_123456.validate(ctx, std::uint32_t{123457}),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("max 123456"));
    REQUIRE_THAT(max_123456.validate(ctx, std::numeric_limits<std::uint64_t>::max()),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("max 123456"));
}

TEST_CASE("Constructing and using the floating point max validator") {
    auto ctx = Context{};
    constexpr auto max_0_0 = max{0.0l};
    REQUIRE(max_0_0.validate(ctx, 0.0f));
    REQUIRE(max_0_0.validate(ctx, -475.0l));
    REQUIRE(max_0_0.validate(ctx, -1.0));
    REQUIRE_THAT(max_0_0.validate(ctx, 0.1f),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("max 0"));
    REQUIRE_THAT(max_0_0.validate(ctx, 14542515485.2634874154654531),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("max 0"));
}

TEST_CASE("Constructing and using the signed less_than validator") {
    auto ctx = Context{};
    constexpr auto lt_7 = less_than{std::int8_t{7}};
    REQUIRE(lt_7.validate(ctx, 6));
    REQUIRE(lt_7.validate(ctx, -452358945));
    REQUIRE_THAT(lt_7.validate(ctx, 7),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("less than 7"));
    REQUIRE_THAT(lt_7.validate(ctx, 2345678642),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("less than 7"));
}

TEST_CASE("Constructing and using the unsigned less_than validator") {
    auto ctx = Context{};
    constexpr auto lt_128 = less_than{128uz};
    REQUIRE(lt_128.validate(ctx, 127u));
    REQUIRE(lt_128.validate(ctx, 0uz));
    REQUIRE_THAT(lt_128.validate(ctx, std::uint16_t{128}),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("less than 128"));
    REQUIRE_THAT(lt_128.validate(ctx, std::numeric_limits<std::size_t>::max()),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("less than 128"));
}

TEST_CASE("Constructing and using the floating point less_than validator") {
    auto ctx = Context{};
    constexpr auto lt_0_0 = less_than{0.0};
    REQUIRE(lt_0_0.validate(ctx, -1.0f));
    REQUIRE(lt_0_0.validate(ctx, -0.1l));
    REQUIRE(lt_0_0.validate(ctx, -596268961515.562621));
    REQUIRE_THAT(lt_0_0.validate(ctx, 0.0l),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("less than 0"));
    REQUIRE_THAT(lt_0_0.validate(ctx, -0.0f),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("less than 0"));
    REQUIRE_THAT(lt_0_0.validate(ctx, 756.7f),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("less than 0"));
}

TEST_CASE("Constructing and using the signed greater_than validator") {
    auto ctx = Context{};
    constexpr auto gt_i16min = greater_than{std::numeric_limits<std::int16_t>::min()};
    REQUIRE(gt_i16min.validate(ctx, std::numeric_limits<std::int16_t>::min() + 1));
    REQUIRE(gt_i16min.validate(ctx, std::int64_t{0}));
    REQUIRE(gt_i16min.validate(ctx, 1337));
    REQUIRE(gt_i16min.validate(ctx, -8534));
    REQUIRE_THAT(gt_i16min.validate(ctx, std::numeric_limits<std::int16_t>::min()),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than -32768"));
    REQUIRE_THAT(gt_i16min.validate(ctx, std::numeric_limits<std::int64_t>::min()),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than -32768"));
}

TEST_CASE("Constructing and using the unsigned greater_than validator") {
    auto ctx = Context{};
    constexpr auto gt_4 = greater_than{std::uint32_t{4}};
    REQUIRE(gt_4.validate(ctx, std::numeric_limits<std::uintmax_t>::max()));
    REQUIRE(gt_4.validate(ctx, 77uz));
    REQUIRE(gt_4.validate(ctx, 5uz));
    REQUIRE_THAT(gt_4.validate(ctx, std::uint8_t{4}),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 4"));
    REQUIRE_THAT(gt_4.validate(ctx, 0u),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 4"));
}

TEST_CASE("Constructing and using the floating point greater_than validator") {
    auto ctx = Context{};
    constexpr auto gt_13_37 = greater_than{13.37};
    REQUIRE(gt_13_37.validate(ctx, 13.38f));
    REQUIRE(gt_13_37.validate(ctx, 1337.768l));
    REQUIRE_THAT(gt_13_37.validate(ctx, 13.37),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 13.37"));
    REQUIRE_THAT(gt_13_37.validate(ctx, -13.37f),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("greater than 13.37"));
}

TEST_CASE("Constructing and using the signed in_closed_range validator") {
    auto ctx = Context{};
    constexpr auto in_range = in_closed_range{-42, 42};
    REQUIRE(in_range.validate(ctx, -42));
    REQUIRE(in_range.validate(ctx, std::int64_t{0}));
    REQUIRE(in_range.validate(ctx, std::int16_t{42}));
    REQUIRE_THAT(in_range.validate(ctx, std::int16_t{-43}),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("in range [-42, 42]"));
    REQUIRE_THAT(in_range.validate(ctx, 43),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("in range [-42, 42]"));
    REQUIRE_THAT(in_range.validate(ctx, std::int64_t{238474859374584}),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("in range [-42, 42]"));
}

TEST_CASE("Constructing and using the unsigned in_closed_range validator") {
    auto ctx = Context{};
    constexpr auto in_range = in_closed_range{10uz, 100uz};
    REQUIRE(in_range.validate(ctx, 10u));
    REQUIRE(in_range.validate(ctx, std::uint8_t{50}));
    REQUIRE(in_range.validate(ctx, std::uint16_t{100}));
    REQUIRE_THAT(in_range.validate(ctx, std::uint32_t{0}),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("in range [10, 100]"));
    REQUIRE_THAT(in_range.validate(ctx, std::uint8_t{9}),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("in range [10, 100]"));
    REQUIRE_THAT(in_range.validate(ctx, 101uz),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("in range [10, 100]"));
}

TEST_CASE("Constructing and using the floating point in_closed_range validator") {
    auto ctx = Context{};
    constexpr auto in_range = in_closed_range{-0.5f, 123.5f};
    REQUIRE(in_range.validate(ctx, -0.5));
    REQUIRE(in_range.validate(ctx, 0.0f));
    REQUIRE(in_range.validate(ctx, 123.5l));
    REQUIRE_THAT(in_range.validate(ctx, -0.51),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("in range [-0.5, 123.5]"));
    REQUIRE_THAT(in_range.validate(ctx, 123.501f),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("in range [-0.5, 123.5]"));
    REQUIRE_THAT(in_range.validate(ctx, std::numeric_limits<float>::infinity()),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("in range [-0.5, 123.5]"));
}

TEST_CASE("Constructing and using the power_or_two validator") {
    auto ctx = Context{};
    constexpr auto pow_of_2 = power_of_two{};
    REQUIRE(pow_of_2.validate(ctx, 1));
    REQUIRE(pow_of_2.validate(ctx, 16uz));
    REQUIRE(pow_of_2.validate(ctx, std::int8_t{64}));
    REQUIRE_THAT(pow_of_2.validate(ctx, static_cast<std::int8_t>(-128)),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("power of two"));
    REQUIRE_THAT(pow_of_2.validate(ctx, 0),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("power of two"));
    REQUIRE_THAT(pow_of_2.validate(ctx, std::numeric_limits<std::uint16_t>::max()),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("power of two"));
    REQUIRE_THAT(pow_of_2.validate(ctx, 7uz),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("power of two"));
}

TEST_CASE("Constructing and using the ip_v4_address validator") {
    auto ctx = Context{};
    constexpr auto ip_v4 = ip_v4_address{};
    REQUIRE(ip_v4.validate(ctx, "192.168.3.1"));
    REQUIRE(ip_v4.validate(ctx, "192.168.0.255"));
    REQUIRE_THAT(ip_v4.validate(ctx, "192.168.0.256"),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("valid IPv4 address"));
    REQUIRE_THAT(ip_v4.validate(ctx, "1920.168.0.256"),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("valid IPv4 address"));
    REQUIRE_THAT(ip_v4.validate(ctx, "127.0.0"),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("valid IPv4 address"));
    REQUIRE_THAT(ip_v4.validate(ctx, "192.168.25"),
                 ExpectedErrorContains(strings::validation_error) && ExpectedErrorContains("valid IPv4 address"));
}

} // namespace reflex::config

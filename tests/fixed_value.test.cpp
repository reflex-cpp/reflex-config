// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <array>
#include <concepts>
#include <variant>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <reflex/config/context.hpp>
#include <reflex/config/deserialize.hpp>
#include <reflex/config/fixed_value.hpp>
#include <reflex/config/serialize.hpp>
#include <reflex/config/validate.hpp>

#include "error_helpers.hpp"
#include "test_types.hpp"

namespace reflex::config {

namespace {
using Catch::Matchers::ContainsSubstring;
}

TEST_CASE("FixedValue is default-initializable") {
    STATIC_REQUIRE(std::default_initializable<FixedValue<42>>);
    STATIC_REQUIRE(std::default_initializable<FixedValue<test_enum::second>>);
    STATIC_REQUIRE(std::default_initializable<FixedValue<class_with_invariant{1, 2}>>);
}

TEST_CASE("FixedValue<V> is comparable to V") {
    STATIC_REQUIRE(std::equality_comparable<FixedValue<42>>);
    STATIC_REQUIRE(std::equality_comparable_with<FixedValue<42>, int>);

    STATIC_REQUIRE(std::equality_comparable<FixedValue<test_enum::first>>);
    STATIC_REQUIRE(std::equality_comparable_with<FixedValue<test_enum::first>, test_enum>);

    STATIC_REQUIRE(std::equality_comparable<FixedValue<class_with_invariant{3, 4}>>);
    STATIC_REQUIRE(std::equality_comparable_with<FixedValue<class_with_invariant{3, 4}>, class_with_invariant>);

    STATIC_REQUIRE(FixedValue<42>{} == 42);
    STATIC_REQUIRE(FixedValue<test_enum::second>{} == test_enum::second);
    STATIC_REQUIRE(FixedValue<class_with_invariant{6, 7}>{} == class_with_invariant{6, 7});

    STATIC_REQUIRE(42 == FixedValue<42>{});
    STATIC_REQUIRE(test_enum::second == FixedValue<test_enum::second>{});
    STATIC_REQUIRE(class_with_invariant{6, 7} == FixedValue<class_with_invariant{6, 7}>{});

    STATIC_REQUIRE(FixedValue<42>{} != 123);
    STATIC_REQUIRE(FixedValue<test_enum::second>{} != test_enum::third);
    STATIC_REQUIRE(FixedValue<class_with_invariant{6, 7}>{} != class_with_invariant{1, 2});

    STATIC_REQUIRE(123 != FixedValue<42>{});
    STATIC_REQUIRE(test_enum::third != FixedValue<test_enum::second>{});
    STATIC_REQUIRE(class_with_invariant{1, 2} != FixedValue<class_with_invariant{6, 7}>{});
}

TEST_CASE("FixedValue<V> is convertible to V") {
    STATIC_REQUIRE(std::convertible_to<FixedValue<123>, int>);
    STATIC_REQUIRE(std::convertible_to<FixedValue<test_enum::third>, test_enum>);
    STATIC_REQUIRE(std::convertible_to<FixedValue<class_with_invariant{3, 4}>, class_with_invariant>);

    constexpr auto convert_to = []<typename T>(T v) -> T { return v; };
    STATIC_REQUIRE(convert_to.operator()<int>(FixedValue<42>{}) == 42);
    STATIC_REQUIRE(convert_to.operator()<test_enum>(FixedValue<test_enum::first>{}) == test_enum::first);
    STATIC_REQUIRE(convert_to.operator()<class_with_invariant>(FixedValue<class_with_invariant{123, 234}>{}) ==
                   class_with_invariant{123, 234});
}

namespace detail {

TEST_CASE("Formatting a 'wrong fixed value' error") {
    auto context = Context{};
    const auto g0 = ContextGuard{context, {}};
    const auto g1 = ContextGuard{context, {.id = "member"}};
    const auto g2 = ContextGuard{context, {.id = "type"}};

    REQUIRE_THAT(make_wrong_fixed_value(context).error().what(),
                 ContainsSubstring("\"/member/type\"") && ContainsSubstring("wrong fixed value"));
}

} // namespace detail

TEST_CASE("FixedValue<V> serializes the same as V") {
    auto context = Context{};

    REQUIRE(try_serialize_value(context, {}, FixedValue<42>{}, {}) == boost::json::value{42});
    REQUIRE(try_serialize_value(context, {}, FixedValue<test_enum::second>{}, {}) == boost::json::value{"second"});
    {
        auto res = boost::json::monotonic_resource{};
        auto json = try_serialize_value(context, {}, FixedValue<class_with_invariant{4, 3}>{}, &res);
        REQUIRE(json == boost::json::value{{"first", 4}, {"second", 3}});
        REQUIRE(json->storage() == &res);
    }
}

TEST_CASE("FixedValue<V> deserializes the same as V") {
    auto context = DeserializationContext{};

    REQUIRE(try_deserialize_value<FixedValue<123>>(context, {}, 123) == FixedValue<123>{});
    REQUIRE(try_deserialize_value<FixedValue<test_enum::third>>(context, {}, "third") ==
            FixedValue<test_enum::third>{});
    REQUIRE(try_deserialize_value<FixedValue<class_with_invariant{7, 8}>>(context, {}, {{"first", 7}, {"second", 8}}) ==
            FixedValue<class_with_invariant{7, 8}>{});

    REQUIRE_THAT(try_deserialize_value<FixedValue<123>>(context, {}, "123"),
                 ExpectedErrorContains("\"\"") && ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT((try_deserialize_value<FixedValue<class_with_invariant{6, 7}>>(context, {}, {{"first", 6}})),
                 ExpectedErrorContains("\"\"") && ExpectedErrorContains(strings::missing_key));
}

TEST_CASE("FixedValue<V> fails deserialization when the value does not match V") {
    auto context = DeserializationContext{};

    REQUIRE_THAT(try_deserialize_value<FixedValue<42>>(context, {}, 123),
                 ExpectedErrorContains("\"\"") && ExpectedErrorContains(strings::wrong_fixed_value));
    REQUIRE_THAT(try_deserialize_value<FixedValue<test_enum::first>>(context, {}, "second"),
                 ExpectedErrorContains("\"\"") && ExpectedErrorContains(strings::wrong_fixed_value));
    REQUIRE_THAT(
            (try_deserialize_value<FixedValue<class_with_invariant{1, 2}>>(context, {}, {{"first", 0}, {"second", 1}})),
            ExpectedErrorContains("\"\"") && ExpectedErrorContains(strings::wrong_fixed_value));
}

TEST_CASE("FixedValue<V> validates the same as V") {
    auto context = Context{};

    REQUIRE(try_validate_value(context, {}, FixedValue<123>{}));
    REQUIRE(try_validate_value(context, {}, FixedValue<test_enum::first>{}));
    REQUIRE(try_validate_value(context, {}, FixedValue<class_with_invariant{1, 2}>{}));

    REQUIRE_THAT(try_validate_value(context, {}, FixedValue<class_with_invariant{5, 5}>{}),
                 ExpectedErrorContains("\"\"") && ExpectedErrorContains(strings::validation_error) &&
                         ExpectedErrorContains("first must not equal second (5)"));
}

TEST_CASE("FixedValue<V> can be used to discriminate std::variant alternatives during deserialization") {
    auto context = DeserializationContext{};

    using variant_t = std::variant<first_alternative, second_alternative>;

    REQUIRE(try_deserialize_value<variant_t>(context, {}, {{"type", "first"}, {"x", 1}}) ==
            variant_t{first_alternative{.type = {}, .x = 1}});
    REQUIRE(try_deserialize_value<variant_t>(context, {}, {{"type", "second"}, {"x", 2}}) ==
            variant_t{second_alternative{.type = {}, .x = 2}});

    {
        const auto exp = try_deserialize_value<variant_t>(context, {}, {{"type", "third"}, {"x", 3}});
        REQUIRE(!exp.has_value());
        REQUIRE_THAT(exp.error().what(),
                     ContainsSubstring("\"\"") && ContainsSubstring(strings::no_alternatives_matched));
        REQUIRE(exp.error().nested().size() == 2);
        REQUIRE_THAT(exp.error().nested()[0].what(),
                     ContainsSubstring("\"/type\"") && ContainsSubstring(strings::wrong_fixed_value));
        REQUIRE_THAT(exp.error().nested()[1].what(),
                     ContainsSubstring("\"/type\"") && ContainsSubstring(strings::wrong_fixed_value));
    }
}

} // namespace reflex::config

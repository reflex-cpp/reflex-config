// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <catch2/catch_test_macros.hpp>

#include <boost/json/string.hpp>

#include <reflex/config/context.hpp>

namespace reflex::config {

TEST_CASE("Context nullability querying and formatting") {
    auto context = Context{};
    REQUIRE(!context.is_nullable());
    REQUIRE(context.as_json_pointer() == "");

    const auto g0 = ContextGuard{context, {}};
    const auto g1 = ContextGuard{context, {.id = 0uz}};
    REQUIRE(!context.is_nullable());
    REQUIRE(context.as_json_pointer() == "/0");

    {
        const auto g2 = ContextGuard{context, {.id = "omg_a_key", .is_nullable = true}};
        REQUIRE(context.is_nullable());
        REQUIRE(context.as_json_pointer() == "/0/omg_a_key");
    }
    REQUIRE(!context.is_nullable());
    REQUIRE(context.as_json_pointer() == "/0");

    const auto g3 = ContextGuard{context, {.id = "escape~this/or_fail"}};
    REQUIRE(!context.is_nullable());
    REQUIRE(context.as_json_pointer() == "/0/escape~0this~1or_fail");

    const auto g4 = ContextGuard{context, {.id = 42uz, .is_nullable = true}};
    REQUIRE(context.is_nullable());
    REQUIRE(context.as_json_pointer() == "/0/escape~0this~1or_fail/42");
}

TEST_CASE("Context error formatting suppression") {
    auto context = Context{};

    REQUIRE(!context.should_suppress_error_formatting());
    {
        const auto g0 = ErrorFormattingSuppressionGuard{context};
        REQUIRE(context.should_suppress_error_formatting());
        {
            const auto g1 = ErrorFormattingSuppressionGuard{context};
            REQUIRE(context.should_suppress_error_formatting());
        }
        REQUIRE(context.should_suppress_error_formatting());
    }
    REQUIRE(!context.should_suppress_error_formatting());
}

} // namespace reflex::config

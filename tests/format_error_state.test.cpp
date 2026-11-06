// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <ranges>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <fmt/ranges.h>

#include <reflex/config/context.hpp>
#include <reflex/config/format_error_state.hpp>

#include "error_helpers.hpp"

namespace reflex::config {

namespace {

using Catch::Matchers::ContainsSubstring;

auto make_errors_to_nest() {
    return std::array{
            format_error_state("Missing key \"{}\"", "type").error(),
            format_error_state("Wrong kind: {}", "number").error(),
            format_error_state("Unexpected value: {}", 77).error(),
    };
}

void check_nested_errors(const ErrorState &error, const auto &to_nest) {
    REQUIRE(error.nested().size() == 3);
    REQUIRE(error.nested()[0].message() == "Missing key \"type\"");
    REQUIRE(error.nested()[1].message() == "Wrong kind: number");
    REQUIRE(error.nested()[2].message() == "Unexpected value: 77");
    REQUIRE(std::ranges::all_of(to_nest, [](const auto &err) { return err.message().empty(); }));
}

} // namespace

TEST_CASE("format_error_state correctly formats ErrorState messages") {
    REQUIRE(format_error_state("Hello {}", 123).error().message() == "Hello 123");
    REQUIRE(format_error_state("Hello {} world (values: {})", 42, fmt::join(std::views::iota(0, 3), " | "))
                    .error()
                    .message() == "Hello 42 world (values: 0 | 1 | 2)");
}

TEST_CASE("format_error_state moves an ErrorState array into the returned ErrorState") {
    auto to_nest = make_errors_to_nest();
    auto combined = format_error_state(move_from, to_nest, "Everything failed at \"{}\"", "member_name").error();
    REQUIRE(combined.message() == "Everything failed at \"member_name\"");
    check_nested_errors(combined, to_nest);
}

TEST_CASE("format_serialization_error formats 'serialization error' with context") {
    auto ctx = Context{};
    REQUIRE_THAT(format_serialization_error(ctx, "invalid input {}", 42).error().what(),
                 ContainsSubstring(strings::serialization_error) && ContainsSubstring("\"\"") &&
                         ContainsSubstring("invalid input 42"));

    {
        auto g0 = ContextGuard{ctx, {}};
        auto g2 = ContextGuard{ctx, {.id = "abc"}};
        auto g1 = ContextGuard{ctx, {.id = 7uz}};
        REQUIRE_THAT(format_serialization_error(ctx, "invalid input {} ({})", 42, "test").error().what(),
                     ContainsSubstring(strings::serialization_error) && ContainsSubstring("\"/abc/7\"") &&
                             ContainsSubstring("invalid input 42 (test)"));
    }
    {
        auto g0 = ContextGuard{ctx, {}};
        auto g2 = ContextGuard{ctx, {.id = "abc"}};
        auto g1 = ContextGuard{ctx, {.id = 7uz}};
        const auto suppress_format = ErrorFormattingSuppressionGuard{ctx};
        const auto error = format_serialization_error(ctx, "invalid input {} ({})", 42, "test").error();
        REQUIRE(error.message().empty());
        REQUIRE(error.nested().empty());
    }
    {
        auto to_nest = make_errors_to_nest();
        const auto combined = format_serialization_error(ctx, move_from, to_nest, "total failure {}", 76).error();
        REQUIRE_THAT(combined.what(),
                     ContainsSubstring(strings::serialization_error) && ContainsSubstring("\"\"") &&
                             ContainsSubstring("total failure 76"));
        check_nested_errors(combined, to_nest);
    }
}

TEST_CASE("format_deserialization_error formats 'deserialization error' with context") {
    auto ctx = Context{};
    REQUIRE_THAT(format_deserialization_error(ctx, "invalid input {}", 42).error().what(),
                 ContainsSubstring(strings::deserialization_error) && ContainsSubstring("\"\"") &&
                         ContainsSubstring("invalid input 42"));

    {
        auto g0 = ContextGuard{ctx, {}};
        auto g2 = ContextGuard{ctx, {.id = "abc"}};
        auto g1 = ContextGuard{ctx, {.id = 7uz}};
        REQUIRE_THAT(format_deserialization_error(ctx, "invalid input {} ({})", 42, "test").error().what(),
                     ContainsSubstring(strings::deserialization_error) && ContainsSubstring("\"/abc/7\"") &&
                             ContainsSubstring("invalid input 42 (test)"));
    }
    {
        auto g0 = ContextGuard{ctx, {}};
        auto g2 = ContextGuard{ctx, {.id = "abc"}};
        auto g1 = ContextGuard{ctx, {.id = 7uz}};
        const auto suppress_format = ErrorFormattingSuppressionGuard{ctx};
        const auto error = format_deserialization_error(ctx, "invalid input {} ({})", 42, "test").error();
        REQUIRE(error.message().empty());
        REQUIRE(error.nested().empty());
    }
    {
        auto to_nest = make_errors_to_nest();
        const auto combined = format_deserialization_error(ctx, move_from, to_nest, "total failure {}", 123).error();
        REQUIRE_THAT(combined.what(),
                     ContainsSubstring(strings::deserialization_error) && ContainsSubstring("\"\"") &&
                             ContainsSubstring("total failure 123"));
        check_nested_errors(combined, to_nest);
    }
}

TEST_CASE("format_validation_error formats 'validation error' with context") {
    auto ctx = Context{};
    REQUIRE_THAT(format_validation_error(ctx, "my error {}", 123).error().what(),
                 ContainsSubstring(strings::validation_error) && ContainsSubstring("\"\"") &&
                         ContainsSubstring("my error 123"));
    {
        auto g0 = ContextGuard{ctx, {}};
        auto g1 = ContextGuard{ctx, {.id = 2uz}};
        auto g2 = ContextGuard{ctx, {.id = "abc"}};
        REQUIRE_THAT(format_validation_error(ctx, "my error {}/{}", 123, "hello").error().what(),
                     ContainsSubstring(strings::validation_error) && ContainsSubstring("\"/2/abc\"") &&
                             ContainsSubstring("my error 123/hello"));
    }
    {
        auto g0 = ContextGuard{ctx, {}};
        auto g1 = ContextGuard{ctx, {.id = 2uz}};
        auto g2 = ContextGuard{ctx, {.id = "abc"}};
        const auto suppress_format = ErrorFormattingSuppressionGuard{ctx};
        const auto error = format_validation_error(ctx, "my error {}/{}", 123, "hello").error();
        REQUIRE(error.message().empty());
        REQUIRE(error.nested().empty());
    }
    {
        auto to_nest = make_errors_to_nest();
        const auto combined = format_validation_error(ctx, move_from, to_nest, "total failure {}", 77).error();
        REQUIRE_THAT(combined.what(),
                     ContainsSubstring(strings::validation_error) && ContainsSubstring("\"\"") &&
                             ContainsSubstring("total failure 77"));
        check_nested_errors(combined, to_nest);
    }
}

} // namespace reflex::config

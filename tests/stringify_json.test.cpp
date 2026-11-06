// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <catch2/catch_test_macros.hpp>

#include <limits>

#include <boost/json/value.hpp>

#include <reflex/config/stringify_json.hpp>

#include "generate_stringify_json_options.hpp"

namespace reflex::config {

namespace {

namespace json = boost::json;

} // namespace

TEST_CASE("stringify_json prints simple types correctly") {
    const auto options = generate_stringify_json_options();
    REQUIRE(stringify_json(json::value{}, options) == "null");
    REQUIRE(stringify_json(json::value(false), options) == "false");
    REQUIRE(stringify_json(json::value(true), options) == "true");
    REQUIRE(stringify_json(json::value(12345), options) == "12345");
    REQUIRE(stringify_json(json::value(-67890), options) == "-67890");
    REQUIRE(stringify_json(json::value(12345u), options) == "12345");
    if (options.pretty) {
        REQUIRE(stringify_json(json::value(42.42), options) == "42.42");
        REQUIRE(stringify_json(json::value(-13.37), options) == "-13.37");
    } else {
        REQUIRE(stringify_json(json::value(42.42), options) == "4.242E1");
        REQUIRE(stringify_json(json::value(-13.37), options) == "-1.337E1");
    }
    if (options.allow_infinity_and_nan) {
        REQUIRE(stringify_json(json::value(std::numeric_limits<double>::infinity()), options) == "Infinity");
        REQUIRE(stringify_json(json::value(-std::numeric_limits<double>::infinity()), options) == "-Infinity");
        REQUIRE(stringify_json(json::value(std::numeric_limits<double>::quiet_NaN()), options) == "NaN");
        REQUIRE(stringify_json(json::value(-std::numeric_limits<double>::quiet_NaN()), options) == "NaN");
    } else {
        REQUIRE(stringify_json(json::value(std::numeric_limits<double>::infinity()), options) == "1e99999");
        REQUIRE(stringify_json(json::value(-std::numeric_limits<double>::infinity()), options) == "-1e99999");
        REQUIRE(stringify_json(json::value(std::numeric_limits<double>::quiet_NaN()), options) == "null");
        REQUIRE(stringify_json(json::value(-std::numeric_limits<double>::quiet_NaN()), options) == "null");
    }
    REQUIRE(stringify_json(json::value("Hello, World!")) == "\"Hello, World!\"");
    REQUIRE(stringify_json(json::value("Hello,\nWorld!")) == "\"Hello,\\nWorld!\"");
}

TEST_CASE("stringify_json prints arrays correctly") {
    const auto options = generate_stringify_json_options();
    REQUIRE(stringify_json(json::value(json::array_kind), options) == "[]");
    {
        const auto str = stringify_json(json::value{1, 2, 3}, options);
        if (options.pretty) {
            REQUIRE(str == "[\n  1,\n  2,\n  3\n]");

        } else {
            REQUIRE(str == "[1,2,3]");
        }
    }
    {
        const auto str = stringify_json(
                json::value{json::value{{"one", 1}, {"two", 2}}, json::value{{"three", 3}, {"four", 4}}},
                options);
        if (options.pretty) {
            REQUIRE(str == R"([
  {
    "one": 1,
    "two": 2
  },
  {
    "three": 3,
    "four": 4
  }
])");
        } else {
            REQUIRE(str == R"([{"one":1,"two":2},{"three":3,"four":4}])");
        }
    }
}

TEST_CASE("stringify_json prints objects correctly") {
    const auto options = generate_stringify_json_options();
    REQUIRE(stringify_json(json::value(json::object_kind), options) == "{}");
    {
        const auto str = stringify_json(json::value{{"one", 1}, {"two", "2"}, {"three", 3.0}}, options);
        if (options.pretty) {
            REQUIRE(str == R"({
  "one": 1,
  "two": "2",
  "three": 3
})");
        } else {
            REQUIRE(str == R"({"one":1,"two":"2","three":3E0})");
        }
    }
    {
        const auto str = stringify_json(json::value{{"null", nullptr}, {"int", 42}, {"array", json::value{1, 2, 3}}},
                                        options);
        if (options.pretty) {
            if (options.omit_null_object_values) {
                REQUIRE(str == R"({
  "int": 42,
  "array": [
    1,
    2,
    3
  ]
})");
            } else {
                REQUIRE(str == R"({
  "null": null,
  "int": 42,
  "array": [
    1,
    2,
    3
  ]
})");
            }
        } else {
            REQUIRE(str == R"({"null":null,"int":42,"array":[1,2,3]})");
        }
    }
    {
        const auto str = stringify_json(json::value{{"null", nullptr}, {"other_null", nullptr}}, options);
        if (options.pretty) {
            if (options.omit_null_object_values) {
                REQUIRE(str == "{}");
            } else {
                REQUIRE(str == R"({
  "null": null,
  "other_null": null
})");
            }
        } else {
            REQUIRE(str == R"({"null":null,"other_null":null})");
        }
    }
}

} // namespace reflex::config

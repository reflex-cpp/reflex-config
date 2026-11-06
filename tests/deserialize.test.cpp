// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <boost/json/error.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/monotonic_resource.hpp>
#include <boost/json/value.hpp>

#include <reflex/enum_indexed_array.hpp>
#include <reflex/meta.hpp>

#include <reflex/config/deserialize.hpp>
#include <reflex/config/errors.hpp>
#include <reflex/config/validators.hpp>

#include "error_helpers.hpp"
#include "test_types.hpp"

namespace reflex::config {

namespace {

using Catch::Matchers::ContainsSubstring;

using namespace std::string_literals;
using namespace std::string_view_literals;

[[nodiscard]] DeserializationContext generate_all_contexts() {
    auto options = deserialization_options_s{};
    options.allow_unknown_keys = GENERATE(false, true);
    CAPTURE(options.allow_unknown_keys);
    return DeserializationContext{std::move(options)};
}

} // namespace

namespace detail {

TEST_CASE("Formatting a 'wrong kind' error") {
    auto context = Context{};
    const auto g0 = ContextGuard{context, {}};
    const auto g1 = ContextGuard{context, {.id = "member"}};
    const auto g2 = ContextGuard{context, {.id = 77uz}};

    {
        constexpr auto string_kind = boost::json::kind::string;
        const auto err = make_wrong_kind(context, {&string_kind, 1}, boost::json::kind::object).error();
        REQUIRE_THAT(err.what(),
                     ContainsSubstring("\"/member/77\"") && ContainsSubstring(strings::wrong_value_kind) &&
                             ContainsSubstring("string") && ContainsSubstring("object"));
    }
    {
        const auto multiple_kinds = {boost::json::kind::object, boost::json::kind::array};
        const auto err = make_wrong_kind(context, multiple_kinds, boost::json::kind::string).error();
        REQUIRE_THAT(err.what(),
                     ContainsSubstring("\"/member/77\"") && ContainsSubstring(strings::wrong_value_kind) &&
                             ContainsSubstring("object | array") && ContainsSubstring("string"));
    }
}

TEST_CASE("Formatting errors from to_number error codes") {
    auto context = Context{};
    const auto g0 = ContextGuard{context, {}};
    const auto g1 = ContextGuard{context, {.id = 123uz}};
    const auto g2 = ContextGuard{context, {.id = "this~for~me?"}};
    constexpr auto expected_json_pointer = "\"/123/this~0for~0me?\"";

    {
        const auto err = make_to_number_ec(context, "123", boost::json::error::not_number, "int").error();
        REQUIRE_THAT(err.what(),
                     ContainsSubstring(expected_json_pointer) && ContainsSubstring(strings::wrong_value_kind) &&
                             ContainsSubstring("number") && ContainsSubstring("string"));
    }

    {
        const auto err = make_to_number_ec(context, -42, boost::json::error::not_exact, "unsigned long").error();
        REQUIRE_THAT(err.what(),
                     ContainsSubstring(expected_json_pointer) && ContainsSubstring(strings::no_exact_conversion) &&
                             ContainsSubstring("unsigned long") && ContainsSubstring("-42") &&
                             ContainsSubstring("int64"));
    }

    {
        const auto other_ec = std::make_error_code(std::errc::connection_aborted);
        const auto err = make_to_number_ec(context, {1, 2, 3}, other_ec, "int").error();
        REQUIRE_THAT(err.what(), ContainsSubstring(expected_json_pointer) && ContainsSubstring(other_ec.message()));
    }
}

TEST_CASE("Formatting a 'wrong array size' error") {
    auto context = Context{};
    const auto g0 = ContextGuard{context, {}};
    const auto g2 = ContextGuard{context, {.id = "my_array_of_arrays"}};
    const auto g3 = ContextGuard{context, {.id = 725uz}};

    const auto err = make_wrong_array_size(context, 42, 72).error();
    REQUIRE_THAT(err.what(),
                 ContainsSubstring("\"/my_array_of_arrays/725\"") && ContainsSubstring(strings::wrong_array_size) &&
                         ContainsSubstring("42") && ContainsSubstring("72"));
}

TEST_CASE("Formatting an 'unknown enumerator name' error") {
    auto context = Context{};
    const auto g0 = ContextGuard{context, {}};
    const auto g1 = ContextGuard{context, {.id = "engine"}};
    const auto g2 = ContextGuard{context, {.id = "mode"}};

    const auto err = make_unknown_enumerator(context, "sEnSoR").error();
    REQUIRE_THAT(err.what(),
                 ContainsSubstring("\"/engine/mode\"") && ContainsSubstring(strings::unknown_enumerator) &&
                         ContainsSubstring("\"sEnSoR\""));
}

TEST_CASE("Formatting a 'missing key' error") {
    auto context = Context{};
    const auto g0 = ContextGuard{context, {}};
    const auto g1 = ContextGuard{context, {.id = "plugins"}};
    const auto g2 = ContextGuard{context, {.id = 4uz}};

    const auto err = make_missing_key(context, "location").error();
    REQUIRE_THAT(err.what(),
                 ContainsSubstring("\"/plugins/4\"") && ContainsSubstring(strings::missing_key) &&
                         ContainsSubstring("\"location\""));
}

TEST_CASE("Formatting an 'unknown key' error") {
    auto context = Context{};
    const auto g0 = ContextGuard{context, {}};
    const auto g1 = ContextGuard{context, {.id = "plugins"}};
    const auto g2 = ContextGuard{context, {.id = 6uz}};

    const auto err = make_unknown_key(context, "lcation").error();
    REQUIRE_THAT(err.what(),
                 ContainsSubstring("\"/plugins/6\"") && ContainsSubstring(strings::unknown_key) &&
                         ContainsSubstring("\"lcation\""));
}

TEST_CASE("Formatting an 'ignored member' error") {
    auto context = Context{};
    const auto g0 = ContextGuard{context, {}};
    const auto g1 = ContextGuard{context, {.id = "camera"}};

    const auto err = make_ignored_member(context, "matrices").error();
    REQUIRE_THAT(err.what(),
                 ContainsSubstring("\"/camera\"") && ContainsSubstring(strings::ignored_member) &&
                         ContainsSubstring("\"matrices\""));
}

} // namespace detail

TEST_CASE("check_kind succeeds when the value kind is expected") {
    auto context = Context{};
    REQUIRE(check_kind(context, boost::json::value(boost::json::string_kind), boost::json::kind::string).has_value());
}

TEST_CASE("check_kind fails when the value kind is unexpected") {
    auto context = Context{};
    REQUIRE_THAT(check_kind(context, boost::json::value(boost::json::string_kind), boost::json::kind::int64),
                 ExpectedErrorContains(strings::wrong_value_kind));
}

TEST_CASE("The error returned by check_kind contains the current context") {
    auto context = Context{};
    const auto g0 = ContextGuard{context, {}};
    const auto g1 = ContextGuard{context, {.id = "plugins"}};
    const auto g2 = ContextGuard{context, {.id = 6uz}};

    REQUIRE_THAT(check_kind(context, boost::json::value(42), boost::json::kind::object),
                 ExpectedErrorContains("\"/plugins/6\""));
}


TEST_CASE("Deserializing std::monostate") {
    auto context = DeserializationContext{};

    REQUIRE(try_deserialize_value<std::monostate>(context, {}, {}) == std::monostate{});

    REQUIRE_THAT(try_deserialize_value<std::monostate>(context, {}, false),
                 ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT(try_deserialize_value<std::monostate>(context, {}, "null"),
                 ExpectedErrorContains(strings::wrong_value_kind));
}

TEST_CASE("Deserializing bool") {
    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<bool>(context, {}, true) == true);
    REQUIRE(try_deserialize_value<bool>(context, {}, false) == false);

    REQUIRE_THAT(try_deserialize_value<bool>(context, {}, {}), ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT(try_deserialize_value<bool>(context, {}, 0), ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT(try_deserialize_value<bool>(context, {}, "true"), ExpectedErrorContains(strings::wrong_value_kind));
}

TEST_CASE("Deserializing numbers") {
    auto context = generate_all_contexts();

    {
        constexpr auto small_int = std::int8_t{42};
        const auto small_int_val = boost::json::value(small_int);
        REQUIRE(try_deserialize_value<std::int8_t>(context, {}, small_int_val) == small_int);
        REQUIRE(try_deserialize_value<std::int16_t>(context, {}, small_int_val) == small_int);
        REQUIRE(try_deserialize_value<std::int32_t>(context, {}, small_int_val) == small_int);
        REQUIRE(try_deserialize_value<std::int64_t>(context, {}, small_int_val) == small_int);
        REQUIRE(try_deserialize_value<std::uint8_t>(context, {}, small_int_val) == small_int);
        REQUIRE(try_deserialize_value<std::uint16_t>(context, {}, small_int_val) == small_int);
        REQUIRE(try_deserialize_value<std::uint32_t>(context, {}, small_int_val) == small_int);
        REQUIRE(try_deserialize_value<std::uint64_t>(context, {}, small_int_val) == small_int);
        REQUIRE(try_deserialize_value<float>(context, {}, small_int_val) == static_cast<float>(small_int));
        REQUIRE(try_deserialize_value<double>(context, {}, small_int_val) == static_cast<double>(small_int));
    }
    {
        constexpr auto large_int = std::numeric_limits<std::int32_t>::max();
        const auto large_int_val = boost::json::value(large_int);
        REQUIRE_THAT(try_deserialize_value<std::int8_t>(context, {}, large_int_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE_THAT(try_deserialize_value<std::int16_t>(context, {}, large_int_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE(try_deserialize_value<std::int32_t>(context, {}, large_int_val) == large_int);
        REQUIRE(try_deserialize_value<std::int64_t>(context, {}, large_int_val) == large_int);
        REQUIRE_THAT(try_deserialize_value<std::uint8_t>(context, {}, large_int_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE_THAT(try_deserialize_value<std::uint16_t>(context, {}, large_int_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE(try_deserialize_value<std::uint32_t>(context, {}, large_int_val) == large_int);
        REQUIRE(try_deserialize_value<std::uint64_t>(context, {}, large_int_val) == large_int);
        REQUIRE(try_deserialize_value<float>(context, {}, large_int_val) == static_cast<float>(large_int));
        REQUIRE(try_deserialize_value<double>(context, {}, large_int_val) == static_cast<double>(large_int));
    }
    {
        constexpr auto negative_int = std::int16_t{-1337};
        const auto negative_int_val = boost::json::value(negative_int);
        REQUIRE_THAT(try_deserialize_value<std::int8_t>(context, {}, negative_int_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE(try_deserialize_value<std::int16_t>(context, {}, negative_int_val) == negative_int);
        REQUIRE(try_deserialize_value<std::int32_t>(context, {}, negative_int_val) == negative_int);
        REQUIRE(try_deserialize_value<std::int64_t>(context, {}, negative_int_val) == negative_int);
        REQUIRE_THAT(try_deserialize_value<std::uint8_t>(context, {}, negative_int_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE_THAT(try_deserialize_value<std::uint16_t>(context, {}, negative_int_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE_THAT(try_deserialize_value<std::uint32_t>(context, {}, negative_int_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE_THAT(try_deserialize_value<std::uint64_t>(context, {}, negative_int_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE(try_deserialize_value<float>(context, {}, negative_int_val) == static_cast<float>(negative_int));
        REQUIRE(try_deserialize_value<double>(context, {}, negative_int_val) == static_cast<double>(negative_int));
    }

    {
        constexpr auto fract_f64 = double{1.234};
        const auto fract_f64_val = boost::json::value(fract_f64);
        REQUIRE_THAT(try_deserialize_value<std::int8_t>(context, {}, fract_f64_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE_THAT(try_deserialize_value<std::int16_t>(context, {}, fract_f64_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE_THAT(try_deserialize_value<std::int32_t>(context, {}, fract_f64_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE_THAT(try_deserialize_value<std::int64_t>(context, {}, fract_f64_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE_THAT(try_deserialize_value<std::uint8_t>(context, {}, fract_f64_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE_THAT(try_deserialize_value<std::uint16_t>(context, {}, fract_f64_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE_THAT(try_deserialize_value<std::uint32_t>(context, {}, fract_f64_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE_THAT(try_deserialize_value<std::uint64_t>(context, {}, fract_f64_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE(try_deserialize_value<float>(context, {}, fract_f64_val) == static_cast<float>(fract_f64));
        REQUIRE(try_deserialize_value<double>(context, {}, fract_f64_val) == fract_f64);
    }
    {
        constexpr auto integral_i16 = std::int16_t{12345};
        constexpr auto integral_f64 = double{integral_i16};
        const auto integral_f64_val = boost::json::value(integral_f64);
        REQUIRE_THAT(try_deserialize_value<std::int8_t>(context, {}, integral_f64_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE(try_deserialize_value<std::int16_t>(context, {}, integral_f64_val) == integral_i16);
        REQUIRE(try_deserialize_value<std::int32_t>(context, {}, integral_f64_val) == integral_i16);
        REQUIRE(try_deserialize_value<std::int64_t>(context, {}, integral_f64_val) == integral_i16);
        REQUIRE_THAT(try_deserialize_value<std::uint8_t>(context, {}, integral_f64_val),
                     ExpectedErrorContains(strings::no_exact_conversion));
        REQUIRE(try_deserialize_value<std::uint16_t>(context, {}, integral_f64_val) == integral_i16);
        REQUIRE(try_deserialize_value<std::uint32_t>(context, {}, integral_f64_val) == integral_i16);
        REQUIRE(try_deserialize_value<std::uint64_t>(context, {}, integral_f64_val) == integral_i16);
        REQUIRE(try_deserialize_value<float>(context, {}, integral_f64_val) == float{integral_i16});
        REQUIRE(try_deserialize_value<double>(context, {}, integral_f64_val) == integral_f64);
    }

    REQUIRE_THAT(try_deserialize_value<std::int32_t>(context, {}, true),
                 ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT(try_deserialize_value<std::int8_t>(context, {}, "Hello, JSON"),
                 ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT(try_deserialize_value<std::uint16_t>(context, {}, {1, 2, 3}),
                 ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT(try_deserialize_value<float>(context, {}, {}), ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT(try_deserialize_value<double>(context, {}, {{"first", "A string"}, {"second", 42}}),
                 ExpectedErrorContains(strings::wrong_value_kind));
}

TEST_CASE("Deserializing strings") {
    auto context = generate_all_contexts();
    constexpr auto test_str = "A totally valid UTF-8 string"sv;
    using custom_string_t = std::basic_string<char, std::char_traits<char>, custom_allocator<char>>;

    REQUIRE(try_deserialize_value<std::string>(context, {}, boost::json::value(test_str)) == test_str);
    REQUIRE(try_deserialize_value<custom_string_t>(context, {}, boost::json::value(test_str)) == test_str);

    REQUIRE_THAT(try_deserialize_value<std::string>(context, {}, 42), ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT(try_deserialize_value<custom_string_t>(context, {}, {1, 2, 3}),
                 ExpectedErrorContains(strings::wrong_value_kind));
}

TEST_CASE("Deserializing optional-like types") {
    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<std::optional<std::uint16_t>>(context, {}, {}) == std::nullopt);
    REQUIRE(try_deserialize_value<std::optional<std::uint16_t>>(context, {}, 1234) == std::uint16_t{1234});
    REQUIRE(try_deserialize_value<std::optional<std::string>>(context, {}, "Hello") == "Hello");

    REQUIRE(try_deserialize_value<std::unique_ptr<std::string>>(context, {}, {}) == nullptr);
    {
        const auto ex_ptr = try_deserialize_value<std::unique_ptr<std::vector<std::string>>>(
                context,
                {},
                {"Hello", "again, ", "World"});
        REQUIRE(ex_ptr.has_value());
        REQUIRE(*ex_ptr);
        REQUIRE(**ex_ptr == std::vector<std::string>{"Hello", "again, ", "World"});
    }

    REQUIRE_THAT(try_deserialize_value<std::optional<std::string>>(context, {}, 123),
                 ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT(try_deserialize_value<std::unique_ptr<std::vector<std::uint64_t>>>(context, {}, "vector of uint64_t"),
                 ExpectedErrorContains(strings::wrong_value_kind));
}

TEST_CASE("Deserializing variants") {
    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<std::variant<std::string, int>>(context, {}, "Hello") ==
            std::variant<std::string, int>{std::string{"Hello"}});
    REQUIRE(try_deserialize_value<std::variant<std::string, int>>(context, {}, 33) ==
            std::variant<std::string, int>{33});
    {
        const auto ex = try_deserialize_value<std::variant<std::string, int>>(context, {}, 123.456);
        REQUIRE(!ex.has_value());
        const auto &err = ex.error();
        REQUIRE_THAT(err.what(), ContainsSubstring("\"\"") && ContainsSubstring(strings::no_alternatives_matched));
        REQUIRE(err.nested().size() == 2);
        REQUIRE_THAT(err.nested()[0].what(),
                     ContainsSubstring("\"\"") && ContainsSubstring(strings::wrong_value_kind) &&
                             ContainsSubstring("string") && ContainsSubstring("double"));
        REQUIRE_THAT(err.nested()[1].what(),
                     ContainsSubstring("\"\"") && ContainsSubstring(strings::no_exact_conversion));
    }
    {
        const auto ex = try_deserialize_value<std::variant<custom_data_type, int_pair>>(
                context,
                {},
                boost::json::value{{"name", "custom"}, {"variant", "second"}});
        REQUIRE(ex.has_value());
        REQUIRE(std::holds_alternative<custom_data_type>(*ex));
        const auto &val = std::get<custom_data_type>(*ex);
        REQUIRE(val.name == "custom");
        REQUIRE(val.variant == test_enum::second);
    }
    {
        const auto ex = try_deserialize_value<std::variant<custom_data_type, int_pair>>(
                context,
                {},
                boost::json::value{{"a", 123}, {"b", 456}});
        REQUIRE(ex.has_value());
        REQUIRE(std::holds_alternative<int_pair>(*ex));
        const auto &val = std::get<int_pair>(*ex);
        REQUIRE(val.a == 123);
        REQUIRE(val.b == 456);
    }
    {
        const auto ex = try_deserialize_value<std::variant<custom_data_type, int_pair>>(
                context,
                {},
                boost::json::value{{"name", "custom"}, {"variant", "fourth"}});
        REQUIRE(!ex.has_value());
        const auto &err = ex.error();
        REQUIRE_THAT(err.what(), ContainsSubstring("\"\"") && ContainsSubstring(strings::no_alternatives_matched));
        REQUIRE(err.nested().size() == 2);
        REQUIRE_THAT(err.nested()[0].what(),
                     ContainsSubstring("\"/variant\"") && ContainsSubstring(strings::unknown_enumerator) &&
                             ContainsSubstring("\"fourth\""));
        REQUIRE_THAT(
                err.nested()[1].what(),
                ContainsSubstring("\"\"") && ContainsSubstring(strings::missing_key) && ContainsSubstring("\"a\""));
    }
    {
        using nullable_variant_t = std::variant<std::monostate, std::string, int>;
        REQUIRE(try_deserialize_value<std::variant<std::monostate, std::string, int>>(context,
                                                                                      {},
                                                                                      boost::json::value{}) ==
                nullable_variant_t{std::monostate{}});
        REQUIRE(try_deserialize_value<std::variant<std::monostate, std::string, int>>(context, {}, "Hello") ==
                nullable_variant_t{std::string{"Hello"}});
        REQUIRE(try_deserialize_value<std::variant<std::monostate, std::string, int>>(context, {}, 42) ==
                nullable_variant_t{42});

        {
            const auto exp = try_deserialize_value<std::variant<std::monostate, std::string, int>>(
                    context,
                    {},
                    boost::json::value(boost::json::array_kind));
            REQUIRE(!exp.has_value());
            const auto &err = exp.error();
            REQUIRE_THAT(err.what(), ContainsSubstring("\"\"") && ContainsSubstring(strings::no_alternatives_matched));
            REQUIRE(err.nested().size() == 3);
            REQUIRE_THAT(err.nested()[0].what(),
                         ContainsSubstring("\"\"") && ContainsSubstring(strings::wrong_value_kind));
            REQUIRE_THAT(err.nested()[1].what(),
                         ContainsSubstring("\"\"") && ContainsSubstring(strings::wrong_value_kind));
            REQUIRE_THAT(err.nested()[2].what(),
                         ContainsSubstring("\"\"") && ContainsSubstring(strings::wrong_value_kind));
        }
    }
}

TEST_CASE("Deserializing boost::json::value") {
    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<boost::json::value>(context, {}, nullptr) == boost::json::value{});
    REQUIRE(try_deserialize_value<boost::json::value>(context, {}, 12345) == boost::json::value(12345));
    {
        auto res = boost::json::monotonic_resource{};
        const auto result = try_deserialize_value<boost::json::value>(context, {}, boost::json::value("Hello", &res));
        REQUIRE(result.has_value());
        REQUIRE(*result == boost::json::value("Hello"));
        REQUIRE(result->storage() != &res);
    }
    {
        const auto result = try_deserialize_value<boost::json::value>(context, {}, {1, 2, "three"});
        REQUIRE(result.has_value());
        REQUIRE(result->is_array());
        REQUIRE(*result == boost::json::array{1, 2, "three"});
    }
    {
        const auto result = try_deserialize_value<boost::json::value>(context, {}, {{"first", 2}, {"second", 1}});
        REQUIRE(result.has_value());
        REQUIRE(result->is_object());
        REQUIRE(*result == boost::json::object{{"first", 2}, {"second", 1}});
    }
}

TEST_CASE("Deserializing boost::json::object") {
    auto context = generate_all_contexts();

    {
        auto res = boost::json::monotonic_resource{};
        const auto result = try_deserialize_value<boost::json::object>(
                context,
                {},
                boost::json::value({{"first", 2}, {"second", 1}}, &res));
        REQUIRE(result.has_value());
        REQUIRE(*result == boost::json::object{{"first", 2}, {"second", 1}});
        REQUIRE(result->storage() != &res);
    }
    REQUIRE_THAT((try_deserialize_value<boost::json::object>(context, {}, 12345)),
                 ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT((try_deserialize_value<boost::json::object>(context, {}, "Hello")),
                 ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT((try_deserialize_value<boost::json::object>(context, {}, {"Hello", "World"})),
                 ExpectedErrorContains(strings::wrong_value_kind));
}

TEST_CASE("Deserializing boost::json::array") {
    auto context = generate_all_contexts();

    {
        auto res = boost::json::monotonic_resource{};
        const auto result = try_deserialize_value<boost::json::array>(context,
                                                                      {},
                                                                      boost::json::value({"first", 2, nullptr}, &res));
        REQUIRE(result.has_value());
        REQUIRE(*result == boost::json::array{"first", 2, nullptr});
        REQUIRE(result->storage() != &res);
    }
    REQUIRE_THAT((try_deserialize_value<boost::json::array>(context, {}, 12345)),
                 ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT((try_deserialize_value<boost::json::array>(context, {}, "Hello")),
                 ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT((try_deserialize_value<boost::json::array>(context, {}, {{"first", "1"}, {"second", 2}})),
                 ExpectedErrorContains(strings::wrong_value_kind));
}

TEST_CASE("Deserializing reflex::EnumIndexedArray") {
    auto context = generate_all_contexts();

    {
        const auto json = boost::json::value{{"first", 1}, {"third", 3}, {"second", 2}};
        const auto deserialized = try_deserialize_value<reflex::EnumIndexedArray<int, test_enum>>(context, {}, json);
        REQUIRE(deserialized.has_value());
        REQUIRE(std::ranges::equal(*deserialized, std::array{1, 2, 3}));
    }
    {
        const auto json = boost::json::value{{"first", 1}, {"second", 2}};
        REQUIRE_THAT((try_deserialize_value<reflex::EnumIndexedArray<int, test_enum>>(context, {}, json)),
                     ExpectedErrorContains(strings::missing_key) && ExpectedErrorContains("\"third\""));
    }
    {
        const auto json = boost::json::value{{"first", 1}, {"second", 2}, {"third", 3}, {"fourth", 4}};
        const auto result = try_deserialize_value<reflex::EnumIndexedArray<int, test_enum>>(context, {}, json);
        if (context.options().allow_unknown_keys) {
            REQUIRE(result.has_value());
            REQUIRE(std::ranges::equal(*result, std::array{1, 2, 3}));
        } else {
            REQUIRE_THAT(result, ExpectedErrorContains(strings::unknown_key) && ExpectedErrorContains("\"fourth\""));
        }
    }
}

TEST_CASE("Deserializing fixed-length sequence containers") {
    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<std::array<std::int32_t, 3>>(context, {}, {1, 2, 3}) == std::array{1, 2, 3});
    REQUIRE(try_deserialize_value<std::array<std::string, 1>>(context, {}, {{"first"}}) ==
            std::array<std::string, 1>{"first"});
    REQUIRE(try_deserialize_value<std::array<std::int32_t, 0>>(context,
                                                               {},
                                                               boost::json::value(boost::json::array_kind)) ==
            std::array<std::int32_t, 0>{});

    REQUIRE_THAT((try_deserialize_value<std::array<std::int32_t, 1>>(context, {}, 42)),
                 ExpectedErrorContains(strings::wrong_value_kind));

    REQUIRE_THAT((try_deserialize_value<std::array<std::string, 2>>(context, {}, {1, 2})),
                 ExpectedErrorContains(strings::wrong_value_kind) && ExpectedErrorContains("\"/0\""));

    REQUIRE_THAT((try_deserialize_value<std::array<std::string, 2>>(context, {}, {{"just one"}})),
                 ExpectedErrorContains(strings::wrong_array_size));
    REQUIRE_THAT((try_deserialize_value<std::array<std::string, 2>>(context, {}, {{"key", "value"}})),
                 ExpectedErrorContains(strings::wrong_value_kind));
}

TEST_CASE("Deserializing set-like containers") {
    auto context = generate_all_contexts();

    {
        using set_t = std::unordered_set<std::string>;

        REQUIRE(try_deserialize_value<set_t>(context, {}, boost::json::array{"first", "second"}) ==
                set_t{"first", "second"});
        REQUIRE(try_deserialize_value<set_t>(context, {}, boost::json::value(boost::json::array_kind)) == set_t{});
    }

    {
        using str_t = std::basic_string<char, std::char_traits<char>, custom_allocator<char>>;
        using set_t = std::set<str_t>;
        REQUIRE(try_deserialize_value<set_t>(context, {}, boost::json::array{"one", "two"}) == set_t{"one", "two"});
    }

    REQUIRE_THAT((try_deserialize_value<std::set<std::string>>(context, {}, {})),
                 ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT((try_deserialize_value<std::set<std::string>>(context, {}, {1, 2, 3})),
                 ExpectedErrorContains(strings::wrong_value_kind));

    {
        const auto object_with_wrong_kind = boost::json::array{"one", 2};
        REQUIRE_THAT((try_deserialize_value<std::unordered_set<std::size_t>>(context, {}, object_with_wrong_kind)),
                     ExpectedErrorContains(strings::wrong_value_kind) && ExpectedErrorContains("\"/0\""));
    }
}

TEST_CASE("Deserializing set-like containers with non-string keys") {
    auto context = generate_all_contexts();
    {
        using enum_key_set_t = std::unordered_set<test_enum>;
        REQUIRE(try_deserialize_value<enum_key_set_t>(context, {}, {"first", "third"}) ==
                enum_key_set_t{test_enum::first, test_enum::third});
        REQUIRE(try_deserialize_value<enum_key_set_t>(context, {}, boost::json::value(boost::json::array_kind)) ==
                enum_key_set_t{});

        const auto object_with_bad_enum = boost::json::value{"first", "fourth"};
        REQUIRE_THAT(try_deserialize_value<enum_key_set_t>(context, {}, object_with_bad_enum),
                     ExpectedErrorContains("/") && ExpectedErrorContains(strings::unknown_enumerator) &&
                             ExpectedErrorContains("\"fourth\""));
    }
    {
        using tag_t = CustomStringTag<false>;
        using custom_key_set_t = std::set<tag_t>;
        REQUIRE(try_deserialize_value<custom_key_set_t>(context, {}, {"hello", "world"}) ==
                custom_key_set_t{tag_t{"hello"}, tag_t{"world"}});
    }
    {
        using tag_t = CustomStringTag<true>;
        using custom_key_set_t = std::set<tag_t>;
        REQUIRE(try_deserialize_value<custom_key_set_t>(context, {}, {"hello", "world"}) ==
                custom_key_set_t{tag_t{"hello"}, tag_t{"world"}});
    }
}


TEST_CASE("Deserializing map-like containers") {
    auto context = generate_all_contexts();

    {
        using map_t = std::map<std::string, std::uint16_t>;
        REQUIRE(try_deserialize_value<map_t>(context, {}, {{"first", 42}, {"second", 127}}) ==
                map_t{{"first", std::uint16_t{42}}, {"second", std::uint16_t{127}}});
        REQUIRE(try_deserialize_value<map_t>(context, {}, boost::json::value(boost::json::object_kind)) == map_t{});
    }
    {
        using str_t = std::basic_string<char, std::char_traits<char>, custom_allocator<char>>;
        using map_t = std::map<str_t, std::vector<std::int64_t>>;
        REQUIRE(try_deserialize_value<map_t>(context, {}, {{"one", {1}}, {"two", {1, 2}}}) ==
                map_t{{"one", {1}}, {"two", {1, 2}}});
    }

    REQUIRE_THAT((try_deserialize_value<std::map<std::string, std::string>>(context, {}, {})),
                 ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT((try_deserialize_value<std::map<std::string, std::int32_t>>(context, {}, {1, 2, 3})),
                 ExpectedErrorContains(strings::wrong_value_kind));

    {
        const auto object_with_wrong_kind = boost::json::value{{"one", "one"}, {"two", 2}};
        REQUIRE_THAT((try_deserialize_value<std::unordered_map<std::string, std::int32_t>>(context,
                                                                                           {},
                                                                                           object_with_wrong_kind)),
                     ExpectedErrorContains(strings::wrong_value_kind) && ExpectedErrorContains("\"/one\""));
    }
}

TEST_CASE("Deserializing map-like containers with non-string keys") {
    auto context = generate_all_contexts();
    {
        using enum_key_map_t = std::unordered_map<test_enum, std::int32_t>;
        REQUIRE(try_deserialize_value<enum_key_map_t>(context, {}, {{"first", 42}, {"third", 127}}) ==
                enum_key_map_t{{test_enum::first, std::uint16_t{42}}, {test_enum::third, std::uint16_t{127}}});
        REQUIRE(try_deserialize_value<enum_key_map_t>(context, {}, boost::json::value(boost::json::object_kind)) ==
                enum_key_map_t{});

        const auto object_with_bad_enum = boost::json::value{{"first", 42}, {"fourth", 1337}};
        REQUIRE_THAT(try_deserialize_value<enum_key_map_t>(context, {}, object_with_bad_enum),
                     ExpectedErrorContains("\"/\"") && ExpectedErrorContains(strings::unknown_enumerator) &&
                             ExpectedErrorContains("\"fourth\""));
    }
    {
        using tag_t = CustomStringTag<false>;
        using custom_key_map_t = std::map<tag_t, std::string>;
        REQUIRE(try_deserialize_value<custom_key_map_t>(context, {}, {{"hello", "1"}, {"world", "2"}}) ==
                custom_key_map_t{{tag_t{"hello"}, "1"}, {tag_t{"world"}, "2"}});
    }
    {
        using tag_t = CustomStringTag<true>;
        using custom_key_map_t = std::map<tag_t, std::string>;
        REQUIRE(try_deserialize_value<custom_key_map_t>(context, {}, {{"hello", "1"}, {"world", "2"}}) ==
                custom_key_map_t{{tag_t{"hello"}, "1"}, {tag_t{"world"}, "2"}});
    }
    {
        using variant_key_map_t = std::map<std::variant<std::string>, std::int32_t>;
        REQUIRE(try_deserialize_value<variant_key_map_t>(context, {}, {{"hello", 1}, {"world", 2}}) ==
                variant_key_map_t{{"hello", 1}, {"world", 2}});
    }
}

TEST_CASE("Deserializing sequence containers") {
    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<std::vector<int>>(context, {}, {1, 2, 3}) == std::vector{1, 2, 3});
    REQUIRE(try_deserialize_value<std::vector<std::array<int, 3>>>(context, {}, {{1, 2, 3}, {4, 5, 6}}) ==
            std::vector{std::array{1, 2, 3}, std::array{4, 5, 6}});
    REQUIRE(try_deserialize_value<std::deque<std::string>>(context, {}, {"first", "second"}) ==
            std::deque{"first"s, "second"s});

    REQUIRE_THAT(try_deserialize_value<std::vector<std::string>>(context, {}, 42),
                 ExpectedErrorContains(strings::wrong_value_kind));

    {
        const auto array_with_wrong_kind = boost::json::value{1, "omg, not a string!", 3};
        REQUIRE_THAT(try_deserialize_value<std::deque<std::int32_t>>(context, {}, array_with_wrong_kind),
                     ExpectedErrorContains(strings::wrong_value_kind) && ExpectedErrorContains("\"/1\""));
    }
}

TEST_CASE("Deserializing an enum") {
    STATIC_REQUIRE(reflex::reflected_enum<test_enum>);

    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<test_enum>(context, {}, "first") == test_enum::first);
    REQUIRE(try_deserialize_value<test_enum>(context, {}, "third") == test_enum::third);

    REQUIRE_THAT(try_deserialize_value<test_enum>(context, {}, "fourth"),
                 ExpectedErrorContains(strings::unknown_enumerator) && ExpectedErrorContains("\"fourth\""));
    REQUIRE_THAT(try_deserialize_value<test_enum>(context, {}, 123),
                 ExpectedErrorContains(strings::wrong_value_kind) && ExpectedErrorContains("int64"));
}

TEST_CASE("Deserializing an enum with no enumerators") {
    STATIC_REQUIRE(reflex::reflected_enum<empty_enum>);

    auto context = generate_all_contexts();

    REQUIRE_THAT(try_deserialize_value<empty_enum>(context, {}, "test"),
                 ExpectedErrorContains(strings::unknown_enumerator) && ExpectedErrorContains("\"test\""));
    REQUIRE_THAT(try_deserialize_value<empty_enum>(context, {}, false),
                 ExpectedErrorContains(strings::wrong_value_kind) && ExpectedErrorContains("bool"));
}

TEST_CASE("Deserializing a renamed enumerator") {
    STATIC_REQUIRE(reflex::reflected_enum<enum_with_rename>);

    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<enum_with_rename>(context, {}, "int") == enum_with_rename::int_);
    REQUIRE_THAT(try_deserialize_value<enum_with_rename>(context, {}, "int_"),
                 ExpectedErrorContains(strings::unknown_enumerator) && ExpectedErrorContains("\"int_\""));
}

TEST_CASE("Deserializing a class") {
    STATIC_REQUIRE(reflex::reflected_class<custom_data_type>);

    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<custom_data_type>(context,
                                                    {},
                                                    {{"count", 2}, {"name", "Test"}, {"variant", "second"}}) ==
            custom_data_type{.count = 2, .name = "Test", .flags = {}, .variant = test_enum::second});

    {
        const auto object_with_missing_key = boost::json::value{{"count", 2}, {"variant", "second"}};
        REQUIRE_THAT(try_deserialize_value<custom_data_type>(context, {}, object_with_missing_key),
                     ExpectedErrorContains("\"\"") && ExpectedErrorContains(strings::missing_key) &&
                             ExpectedErrorContains("\"name\""));
    }
    {
        const auto object_with_wrong_kind = boost::json::value{{"count", 2}, {"name", 42}, {"variant", "second"}};
        REQUIRE_THAT(try_deserialize_value<custom_data_type>(context, {}, object_with_wrong_kind),
                     ExpectedErrorContains("\"/name\"") && ExpectedErrorContains(strings::wrong_value_kind));
    }
    {
        const auto object_with_extra_key = boost::json::value{{"count", 2},
                                                              {"name", "Test"},
                                                              {"variant", "second"},
                                                              {"extra", true}};
        if (context.options().allow_unknown_keys) {
            REQUIRE(try_deserialize_value<custom_data_type>(context, {}, object_with_extra_key) ==
                    custom_data_type{.count = 2, .name = "Test", .flags = {}, .variant = test_enum::second});
        } else {
            REQUIRE_THAT(try_deserialize_value<custom_data_type>(context, {}, object_with_extra_key),
                         ExpectedErrorContains("\"\"") && ExpectedErrorContains(strings::unknown_key) &&
                                 ExpectedErrorContains("\"extra\""));
        }
    }
}

TEST_CASE("Deserializing a derived class") {
    STATIC_REQUIRE(reflex::reflected_class<derived_from_custom_data_type>);

    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<derived_from_custom_data_type>(
                    context,
                    {},
                    {{"count", 2}, {"name", "Test"}, {"variant", "second"}, {"extra_value", 7}}) ==
            derived_from_custom_data_type{{.count = 2, .name = "Test", .flags = {}, .variant = test_enum::second}, 7});
    {
        const auto object_with_missing_key = boost::json::value{{"count", 2}, {"name", "Test"}, {"variant", "second"}};
        REQUIRE_THAT(try_deserialize_value<derived_from_custom_data_type>(context, {}, object_with_missing_key),
                     ExpectedErrorContains("\"\"") && ExpectedErrorContains(strings::missing_key) &&
                             ExpectedErrorContains("\"extra_value\""));
    }
}

TEST_CASE("Deserializing an empty class") {
    STATIC_REQUIRE(reflex::reflected_class<empty_struct>);

    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<empty_struct>(context, {}, boost::json::object_kind) == empty_struct{});

    REQUIRE_THAT(try_deserialize_value<empty_struct>(context, {}, {}),
                 ExpectedErrorContains(strings::wrong_value_kind));
    REQUIRE_THAT(try_deserialize_value<empty_struct>(context, {}, {1, 2, 3}),
                 ExpectedErrorContains(strings::wrong_value_kind));
    if (context.options().allow_unknown_keys) {
        REQUIRE(try_deserialize_value<empty_struct>(context, {}, {{"key", "value"}}) == empty_struct{});
    } else {
        REQUIRE_THAT(try_deserialize_value<empty_struct>(context, {}, {{"key", "value"}}),
                     ExpectedErrorContains(strings::unknown_key) && ExpectedErrorContains("\"key\""));
    }
}

TEST_CASE("Deserializing a class with a renamed member") {
    STATIC_REQUIRE(reflex::reflected_class<class_with_rename>);

    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<class_with_rename>(context, {}, boost::json::value{{"int", 123}}) ==
            class_with_rename{._int = 123});
    REQUIRE_THAT(try_deserialize_value<class_with_rename>(context, {}, boost::json::value{{"_int", 123}}),
                 ExpectedErrorContains(strings::missing_key) && ExpectedErrorContains("\"int\""));
    REQUIRE_THAT(try_deserialize_value<class_with_rename>(context, {}, boost::json::value{{"int", "123"}}),
                 ExpectedErrorContains("\"/int\"") && ExpectedErrorContains(strings::wrong_value_kind));
}

TEST_CASE("Deserializing a class with an unassignable member") {
    STATIC_REQUIRE(reflex::reflected_class<class_with_non_assignable_member>);

    auto context = generate_all_contexts();

    REQUIRE(try_deserialize_value<class_with_non_assignable_member>(
                    context,
                    {},
                    boost::json::value{{"member", {{"name", "hello"}, {"variant", "second"}}}}) ==
            class_with_non_assignable_member{.member = {.name = "hello", .variant = test_enum::second}});
}

TEST_CASE("Deserializing a custom type") {
    auto context = generate_all_contexts();
    {
        const auto result = try_deserialize_value<CustomClass>(context, {}, boost::json::value(456));
        REQUIRE(result.has_value());
        REQUIRE(result->val() == 456);
    }
    REQUIRE_THAT(try_deserialize_value<CustomClass>(context, {}, boost::json::value("123")),
                 ExpectedErrorContains(strings::wrong_value_kind));
}

TEST_CASE("Deserializing a class with an unknown key calls the on_unknown_key callback") {
    bool callback_called = false;

    auto options = deserialization_options_s{};
    options.allow_unknown_keys = GENERATE(false, true);
    options.on_unknown_key = [&](const boost::json::string &where, const boost::json::string &key) -> expected_t<void> {
        callback_called = true;
        REQUIRE(where == "/test");
        REQUIRE(key == "extra");
        return {};
    };
    CAPTURE(options.allow_unknown_keys);
    auto context = DeserializationContext{std::move(options)};

    auto guard = ContextGuard{context, {}};

    const auto clean_object = boost::json::value{{"count", 2}, {"name", "Test"}, {"variant", "second"}};
    REQUIRE(try_deserialize_value<custom_data_type>(context, {}, clean_object) ==
            custom_data_type{.count = 2, .name = "Test", .flags = {}, .variant = test_enum::second});
    REQUIRE(!callback_called);

    const auto object_with_extra_key = boost::json::value{{"count", 2},
                                                          {"name", "Test"},
                                                          {"variant", "second"},
                                                          {"extra", true}};
    if (context.options().allow_unknown_keys) {
        REQUIRE(try_deserialize_value<custom_data_type>(context, {.id = "test"}, object_with_extra_key) ==
                custom_data_type{.count = 2, .name = "Test", .flags = {}, .variant = test_enum::second});
        REQUIRE(callback_called);
    } else {
        REQUIRE_THAT(try_deserialize_value<custom_data_type>(context, {.id = "test"}, object_with_extra_key),
                     ExpectedErrorContains("\"/test\"") && ExpectedErrorContains(strings::unknown_key));
        REQUIRE(!callback_called);
    }
}

TEST_CASE("Deserializing a class with an ignored member") {
    STATIC_REQUIRE(reflex::reflected_class<class_with_ignored_member>);

    auto context = generate_all_contexts();

    {
        const auto deserialized = try_deserialize_value<class_with_ignored_member>(context,
                                                                                   {},
                                                                                   boost::json::value{{"field", 123}});
        REQUIRE(deserialized.has_value());
        REQUIRE(deserialized->field == 123);
        REQUIRE(deserialized->ignored_field == 42);
    }

    {
        const auto invalid_object = boost::json::value{{"field", 123}, {"ignored_field", 456}};

        REQUIRE_THAT(try_deserialize_value<class_with_ignored_member>(context, {}, invalid_object),
                     ExpectedErrorContains("\"\"") && ExpectedErrorContains(strings::ignored_member) &&
                             ExpectedErrorContains("\"ignored_field\""));
    }
}

TEST_CASE("Deserializing from a string considers parse_options and deserialization_options") {
    auto parse_options = boost::json::parse_options{};
    parse_options.allow_comments = GENERATE(false, true);
    CAPTURE(parse_options.allow_comments);

    auto deserialization_options = deserialization_options_s{};
    const auto allow_unknown_keys = GENERATE(false, true);
    deserialization_options.allow_unknown_keys = allow_unknown_keys;
    CAPTURE(deserialization_options.allow_unknown_keys);

    SECTION("Valid JSON") {
        constexpr auto json_str =
                R"({"count": 10, "name": "First", "flags": [false, true, false], "variant": "second"})"sv;
        REQUIRE(deserialize_config_from_string<custom_data_type>(json_str,
                                                                 parse_options,
                                                                 std::move(deserialization_options)) ==
                custom_data_type{.count = 10,
                                 .name = "First",
                                 .flags = {false, true, false},
                                 .variant = test_enum::second});
    }
    SECTION("JSON with comments") {
        constexpr auto json_str =
                R"({/*"count": 10,*/ "name": "First", "flags": [false, true, false], "variant": "second"})"sv;
        if (parse_options.allow_comments) {
            REQUIRE(deserialize_config_from_string<custom_data_type>(json_str,
                                                                     parse_options,
                                                                     std::move(deserialization_options)) ==
                    custom_data_type{.name = "First", .flags = {false, true, false}, .variant = test_enum::second});
        } else {
            REQUIRE_THROWS_AS(deserialize_config_from_string<custom_data_type>(json_str,
                                                                               parse_options,
                                                                               std::move(deserialization_options)),
                              boost::system::system_error);
        }
    }
    SECTION("JSON with an unknown key") {
        constexpr auto json_str =
                R"({"count": 10, "new_key": 42, "name": "First", "flags": [false, true, false], "variant": "second"})"sv;
        if (allow_unknown_keys) {
            REQUIRE(deserialize_config_from_string<custom_data_type>(json_str,
                                                                     parse_options,
                                                                     std::move(deserialization_options)) ==
                    custom_data_type{.count = 10,
                                     .name = "First",
                                     .flags = {false, true, false},
                                     .variant = test_enum::second});
        } else {
            REQUIRE_THROWS_AS(deserialize_config_from_string<custom_data_type>(json_str,
                                                                               parse_options,
                                                                               std::move(deserialization_options)),
                              DeserializationError);
        }
    }
}

} // namespace reflex::config

// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <array>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <boost/json/monotonic_resource.hpp>
#include <boost/json/value.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <reflex/enum_indexed_array.hpp>

#include <reflex/config/errors.hpp>
#include <reflex/config/serialize.hpp>

#include "error_helpers.hpp"
#include "generate_stringify_json_options.hpp"
#include "test_types.hpp"

namespace reflex::config {

namespace {

using Catch::Matchers::ContainsSubstring;

} // namespace

namespace detail {

TEST_CASE("Formatting a 'key did not serialize to string' error") {
    auto context = Context{};
    const auto g0 = ContextGuard{context, {}};
    const auto g1 = ContextGuard{context, {.id = "member"}};
    const auto g2 = ContextGuard{context, {}};

    REQUIRE_THAT(make_key_did_not_serialized_to_string(context, boost::json::kind::int64).error().what(),
                 ContainsSubstring("\"/member/\"") && ContainsSubstring(strings::key_did_not_serialize_to_string) &&
                         ContainsSubstring("actual kind: int64"));
}

TEST_CASE("Formatting an 'unknown enum value' error") {
    auto context = Context{};
    const auto g0 = ContextGuard{context, {}};
    const auto g1 = ContextGuard{context, {.id = "member"}};
    const auto g2 = ContextGuard{context, {.id = 77uz}};

    REQUIRE_THAT(make_unknown_enum_value(context, 123).error().what(),
                 ContainsSubstring(strings::unknown_enumeration_value));
    REQUIRE_THAT(make_unknown_enum_value(context, std::int64_t{-456}).error().what(),
                 ContainsSubstring(strings::unknown_enumeration_value));

    REQUIRE_THAT(make_unknown_enum_value(context, std::uint32_t{453218}).error().what(),
                 ContainsSubstring("\"/member/77\"") && ContainsSubstring(strings::unknown_enumeration_value) &&
                         ContainsSubstring("453218"));
    REQUIRE_THAT(make_unknown_enum_value(context, std::int16_t{-1234}).error().what(),
                 ContainsSubstring("\"/member/77\"") && ContainsSubstring(strings::unknown_enumeration_value) &&
                         ContainsSubstring("-1234"));
    REQUIRE_THAT(make_unknown_enum_value(context, std::numeric_limits<std::uint64_t>::max()).error().what(),
                 ContainsSubstring("\"/member/77\"") && ContainsSubstring(strings::unknown_enumeration_value) &&
                         ContainsSubstring(std::to_string(std::numeric_limits<std::uint64_t>::max())));
    REQUIRE_THAT(make_unknown_enum_value(context, std::numeric_limits<std::uint64_t>::min()).error().what(),
                 ContainsSubstring("\"/member/77\"") && ContainsSubstring(strings::unknown_enumeration_value) &&
                         ContainsSubstring(std::to_string(std::numeric_limits<std::uint64_t>::min())));
}

} // namespace detail

TEST_CASE("Serializing std::monostate") {
    auto context = Context{};

    auto res = boost::json::monotonic_resource{};
    const auto exp = try_serialize_value(context, {}, std::monostate{}, &res);
    REQUIRE(exp.has_value());
    REQUIRE(exp->is_null());
    REQUIRE(exp->storage() == &res);
}

TEST_CASE("Serializing a bool") {
    auto context = Context{};
    {
        auto res = boost::json::monotonic_resource{};
        const auto true_val = try_serialize_value(context, {}, true, &res);
        REQUIRE(true_val.has_value());
        REQUIRE(*true_val == true);
        REQUIRE(true_val->storage() == &res);
    }
    REQUIRE(try_serialize_value(context, {}, false, {}) == false);
}

TEST_CASE("Serializing numbers") {
    auto context = Context{};

    REQUIRE(try_serialize_value(context, {}, std::int8_t{42}, {}) == 42);
    REQUIRE(try_serialize_value(context, {}, std::int16_t{16721}, {}) == 16721);
    REQUIRE(try_serialize_value(context, {}, std::int32_t{-156222}, {}) == -156222);
    {
        auto res = boost::json::monotonic_resource{};
        const auto ten_billion = try_serialize_value(context, {}, std::int64_t{10'000'000'000}, &res);
        REQUIRE(ten_billion.has_value());
        REQUIRE(*ten_billion == 10'000'000'000);
        REQUIRE(ten_billion->storage() == &res);
    }

    REQUIRE(try_serialize_value(context, {}, std::uint8_t{123}, {}) == 123);
    REQUIRE(try_serialize_value(context, {}, std::uint16_t{5362}, {}) == 5362);
    REQUIRE(try_serialize_value(context, {}, std::uint32_t{5553642}, {}) == 5553642);
    REQUIRE(try_serialize_value(context, {}, std::uint64_t{12'853'282'628}, {}) == 12'853'282'628);

    REQUIRE(try_serialize_value(context, {}, 1358.45652f, {}) == static_cast<double>(1358.45652f));
    REQUIRE(try_serialize_value(context, {}, -125.4561965, {}) == -125.4561965);
    REQUIRE(try_serialize_value(context, {}, 4354515.496525L, {}) == static_cast<double>(4354515.496525L));
}

TEST_CASE("Serializing strings") {
    using custom_string_t = std::basic_string<char, std::char_traits<char>, custom_allocator<char>>;
    const auto test_str = std::string{"Just a test string"};
    const auto custom_test_str = custom_string_t{"A custom test string"};

    auto context = Context{};

    REQUIRE(try_serialize_value(context, {}, test_str, {}) == boost::json::value(test_str));
    {
        auto res = boost::json::monotonic_resource{};
        const auto custom_str_value = try_serialize_value(context, {}, custom_test_str, &res);
        REQUIRE(custom_str_value.has_value());
        REQUIRE(*custom_str_value == boost::json::value(custom_test_str));
        REQUIRE(custom_str_value->storage() == &res);
    }
}

TEST_CASE("Serializing optional-like types") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};
    {
        const auto empty_optional = std::optional<std::string>{};
        const auto empty_opt_value = try_serialize_value(context, {}, empty_optional, &res);
        REQUIRE(empty_opt_value.has_value());
        REQUIRE(*empty_opt_value == boost::json::value());
        REQUIRE(empty_opt_value->storage() == &res);
    }
    {
        const auto string_optional = std::optional<std::string>{"Totally a string"};
        const auto string_opt_value = try_serialize_value(context, {}, string_optional, &res);
        REQUIRE(string_opt_value.has_value());
        REQUIRE(*string_opt_value == boost::json::value(*string_optional));
        REQUIRE(string_opt_value->storage() == &res);
    }
    {
        const auto null_ptr = std::unique_ptr<float>{};
        REQUIRE(try_serialize_value(context, {}, null_ptr, {}) == boost::json::value());
    }
    {
        const auto double_ptr = std::make_unique<double>(128.5156);
        REQUIRE(try_serialize_value(context, {}, double_ptr, {}) == 128.5156);
    }
}

TEST_CASE("Serializing variants") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};
    {
        const auto var = std::variant<int, std::string>{"Hello"};
        const auto var_json = try_serialize_value(context, {}, var, &res);
        REQUIRE(var_json.has_value());
        REQUIRE(*var_json == "Hello");
        REQUIRE(var_json->storage() == &res);
    }
    {
        const auto var = std::variant<int, std::string>{123};
        const auto var_json = try_serialize_value(context, {}, var, &res);
        REQUIRE(var_json.has_value());
        REQUIRE(*var_json == 123);
        REQUIRE(var_json->storage() == &res);
    }
}

TEST_CASE("Serializing boost::json::value") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};

    const auto value = boost::json::value{{"first", 1}, {"second", {"2"}}};
    const auto json = try_serialize_value(context, {}, value, &res);
    REQUIRE(json.has_value());
    REQUIRE(json->is_object());
    REQUIRE(json->storage() == &res);
    const auto &json_object = json->get_object();
    REQUIRE(json_object.size() == 2);
    REQUIRE(json_object.at("first") == 1);
    REQUIRE(json_object.at("second").is_array());
    const auto &second_array = json_object.at("second").get_array();
    REQUIRE(second_array.size() == 1);
    REQUIRE(second_array[0] == "2");
    REQUIRE(second_array.storage() == &res);
}

TEST_CASE("Serializing boost::json::object") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};

    const auto value = boost::json::object{{"first", 1}, {"second", {"2"}}};
    const auto json = try_serialize_value(context, {}, value, &res);
    REQUIRE(json.has_value());
    REQUIRE(json->is_object());
    REQUIRE(json->storage() == &res);
    const auto &json_object = json->get_object();
    REQUIRE(json_object.size() == 2);
    REQUIRE(json_object.at("first") == 1);
    REQUIRE(json_object.at("second").is_array());
    const auto &second_array = json_object.at("second").get_array();
    REQUIRE(second_array.size() == 1);
    REQUIRE(second_array[0] == "2");
    REQUIRE(second_array.storage() == &res);
}

TEST_CASE("Serializing boost::json::array") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};

    const auto value = boost::json::array{1, "2", nullptr};
    const auto json = try_serialize_value(context, {}, value, &res);
    REQUIRE(json.has_value());
    REQUIRE(json->is_array());
    REQUIRE(json->storage() == &res);
    const auto &json_array = json->get_array();
    REQUIRE(json_array.size() == 3);
    REQUIRE(json_array[0] == 1);
    REQUIRE(json_array[1] == "2");
    REQUIRE(json_array[2].is_null());
}

TEST_CASE("Serializing reflex::EnumIndexedArray") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};
    {
        const auto value = reflex::EnumIndexedArray<int, test_enum>{1, 2, 3};
        const auto json = try_serialize_value(context, {}, value, &res);
        REQUIRE(json.has_value());
        REQUIRE(json->is_object());
        const auto &json_object = json->get_object();
        REQUIRE(json_object.size() == 3);
        REQUIRE(json_object.at("first") == 1);
        REQUIRE(json_object.at("second") == 2);
        REQUIRE(json_object.at("third") == 3);
        REQUIRE(json_object.storage() == &res);
        REQUIRE(json_object.at("first").storage() == &res);
    }
}

TEST_CASE("Serializing array-like types") {
    auto context = Context{};
    {
        constexpr auto empty_array = std::array<float, 0>{};
        REQUIRE(try_serialize_value(context, {}, empty_array, {}) == boost::json::value(boost::json::array_kind));
    }
    {
        auto res = boost::json::monotonic_resource{};
        constexpr auto non_empty_array = std::array{1, 2, 3, 4, 5};
        const auto array_value = try_serialize_value(context, {}, non_empty_array, &res);
        REQUIRE(array_value.has_value());
        REQUIRE(*array_value == boost::json::value{1, 2, 3, 4, 5});
        REQUIRE(array_value->storage() == &res);
        REQUIRE(std::ranges::all_of(array_value->get_array(), [&](const auto &val) { return val.storage() == &res; }));
    }
}

TEST_CASE("Serializing set-like containers") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};

    const auto string_set = std::set<std::string>{"def", "ghi", "abc"};
    const auto set_value = try_serialize_value(context, {}, string_set, &res);
    REQUIRE(set_value.has_value());
    REQUIRE(*set_value == boost::json::value{"abc", "def", "ghi"});
    REQUIRE(set_value->storage() == &res);
    REQUIRE(std::ranges::all_of(set_value->get_array(), [&](const auto &str) { return str.storage() == &res; }));
}

TEST_CASE("Serializing map-like containers") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};

    const auto string_map = std::map<std::string, std::string>{{"hello", "world"}, {"goodbye", "logic"}};
    const auto map_value = try_serialize_value(context, {}, string_map, &res);
    REQUIRE(map_value.has_value());
    REQUIRE(*map_value == boost::json::value{{"hello", "world"}, {"goodbye", "logic"}});
    REQUIRE(map_value->storage() == &res);
    REQUIRE(std::ranges::all_of(map_value->get_object(), [&](const auto &entry) { return entry.storage() == &res; }));
}

TEST_CASE("Serializing map-like containers with non-string keys") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};

    {
        const auto enum_key_map = std::map<test_enum, std::int32_t>{{test_enum::first, 1}, {test_enum::third, 3}};
        const auto map_value = try_serialize_value(context, {}, enum_key_map, &res);
        REQUIRE(map_value.has_value());
        REQUIRE(*map_value == boost::json::value{{"first", 1}, {"third", 3}});
        REQUIRE(map_value->storage() == &res);
        REQUIRE(std::ranges::all_of(map_value->get_object(),
                                    [&](const auto &entry) { return entry.storage() == &res; }));
    }
    {
        const auto invalid_enum_key_map = std::map<test_enum, std::int32_t>{{static_cast<test_enum>(42), 42}};
        REQUIRE_THAT(try_serialize_value(context, {}, invalid_enum_key_map, &res),
                     ExpectedErrorContains("\"/\"") && ExpectedErrorContains(strings::unknown_enumeration_value) &&
                             ExpectedErrorContains("42"));
    }
    {
        using tag_t = CustomStringTag<false>;
        const auto custom_key_map = std::map<tag_t, std::int32_t>{{tag_t{"hello"}, 1}, {tag_t{"world"}, 2}};
        const auto map_value = try_serialize_value(context, {}, custom_key_map, &res);
        REQUIRE(map_value.has_value());
        REQUIRE(*map_value == boost::json::value{{"hello", 1}, {"world", 2}});
        REQUIRE(map_value->storage() == &res);
        REQUIRE(std::ranges::all_of(map_value->get_object(),
                                    [&](const auto &entry) { return entry.storage() == &res; }));
    }
    {
        using tag_t = CustomStringTag<true>;
        const auto custom_key_map = std::map<tag_t, std::int32_t>{{tag_t{"hello"}, 1}, {tag_t{"world"}, 2}};
        const auto map_value = try_serialize_value(context, {}, custom_key_map, &res);
        REQUIRE(map_value.has_value());
        REQUIRE(*map_value == boost::json::value{{"hello", 1}, {"world", 2}});
        REQUIRE(map_value->storage() == &res);
        REQUIRE(std::ranges::all_of(map_value->get_object(),
                                    [&](const auto &entry) { return entry.storage() == &res; }));
    }
    {
        const auto variant_key_map = std::map<std::variant<std::string>, std::int32_t>{{"hello", 1}, {"world", 2}};
        const auto map_value = try_serialize_value(context, {}, variant_key_map, &res);
        REQUIRE(map_value.has_value());
        REQUIRE(*map_value == boost::json::value{{"hello", 1}, {"world", 2}});
        REQUIRE(map_value->storage() == &res);
        REQUIRE(std::ranges::all_of(map_value->get_object(),
                                    [&](const auto &entry) { return entry.storage() == &res; }));
    }
    {
        using bad_map_t = std::map<CustomClass, std::string>;
        REQUIRE_THAT(
                try_serialize_value(context, {}, bad_map_t{{CustomClass{1}, "one"}, {CustomClass{2}, "two"}}, &res),
                ExpectedErrorContains("\"/\"") && ExpectedErrorContains(strings::key_did_not_serialize_to_string) &&
                        ExpectedErrorContains("actual kind: int64"));
    }
}

TEST_CASE("Serializing sequence containers") {
    auto context = Context{};
    {
        const auto int_vec = std::vector{5, 2, 3, 6, 4};
        REQUIRE(try_serialize_value(context, {}, int_vec, {}) == boost::json::value{5, 2, 3, 6, 4});
    }
    {
        auto res = boost::json::monotonic_resource{};
        const auto string_list = std::list<std::string>{"one", "two", "three"};
        const auto string_list_value = try_serialize_value(context, {}, string_list, &res);
        REQUIRE(string_list_value.has_value());
        REQUIRE(*string_list_value == boost::json::value{"one", "two", "three"});
        REQUIRE(string_list_value->storage() == &res);
    }
}

TEST_CASE("Serializing enums") {
    auto context = Context{};
    {
        auto res = boost::json::monotonic_resource{};
        const auto enum_value = try_serialize_value(context, {}, test_enum::first, &res);
        REQUIRE(enum_value.has_value());
        REQUIRE(*enum_value == boost::json::value("first"));
        REQUIRE(enum_value->storage() == &res);
    }
    REQUIRE(try_serialize_value(context, {}, non_zero_enum::second, {}) == boost::json::value("second"));
}

TEST_CASE("Serializing unknown enum values returns an error") {
    auto context = Context{};
    const auto g0 = ContextGuard{context, {}};
    const auto g1 = ContextGuard{context, {.id = "first"}};
    const auto g2 = ContextGuard{context, {.id = 2uz}};

    const auto value = static_cast<non_zero_enum>(-777);
    REQUIRE_THAT(try_serialize_value(context, {.id = "option"}, value, {}),
                 ExpectedErrorContains("\"/first/2/option\"") &&
                         ExpectedErrorContains(strings::unknown_enumeration_value) && ExpectedErrorContains("-777"));
}

TEST_CASE("Serializing a renamed enumerator") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};
    const auto enum_value = try_serialize_value(context, {}, enum_with_rename::int_, &res);
    REQUIRE(enum_value.has_value());
    REQUIRE(*enum_value == boost::json::value("int"));
    REQUIRE(enum_value->storage() == &res);
}

TEST_CASE("Serializing classes") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};

    REQUIRE(try_serialize_value(context, {}, empty_struct{}, {}) == boost::json::value(boost::json::object_kind));
    {
        const auto data = custom_data_type{.count = 42,
                                           .name = "test",
                                           .flags = {true, false, true},
                                           .variant = test_enum::second};
        const auto json = try_serialize_value(context, {}, data, &res);
        REQUIRE(json.has_value());
        REQUIRE(*json == boost::json::value{{"count", 42},
                                            {"name", "test"},
                                            {"flags", {true, false, true}},
                                            {"variant", "second"}});
        REQUIRE(json->storage() == &res);
        REQUIRE(std::ranges::all_of(json->get_object(), [&](const auto &entry) { return entry.storage() == &res; }));
    }
    {
        const auto data = outer_class_with_validation{.inner{.thread_count = 10}};
        const auto json = try_serialize_value(context, {}, data, &res);
        REQUIRE(json.has_value());
        REQUIRE(*json == boost::json::value{{"inner", {{"thread_count", 10}}}});
        REQUIRE(json->storage() == &res);
        REQUIRE(std::ranges::all_of(json->get_object(), [&](const auto &entry) { return entry.storage() == &res; }));
        REQUIRE(std::ranges::all_of(json->get_object().at("inner").get_object(),
                                    [&](const auto &entry) { return entry.storage() == &res; }));
    }
}

TEST_CASE("Serializing a class with a renamed data member") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};
    const auto data = class_with_rename{._int = 123};
    const auto json = try_serialize_value(context, {}, data, &res);
    REQUIRE(json.has_value());
    REQUIRE(*json == boost::json::value{{"int", 123}});
    REQUIRE(json->storage() == &res);
}

TEST_CASE("Serializing custom types") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};

    const auto json = try_serialize_value(context, {}, CustomClass{123}, &res);
    REQUIRE(json.has_value());
    REQUIRE(*json == boost::json::value(123));
    REQUIRE(json->storage() == &res);
}

TEST_CASE("Serializing a class with an ignored data member") {
    auto context = Context{};
    auto res = boost::json::monotonic_resource{};
    const auto data = class_with_ignored_member{.field = 123, .ignored_field = 456};
    const auto json = try_serialize_value(context, {}, data, &res);
    REQUIRE(json.has_value());
    REQUIRE(*json == boost::json::value{{"field", 123}});
    REQUIRE(json->storage() == &res);
}

TEST_CASE("Serialising a config to a string") {
    const auto data = custom_data_type{.count = 42,
                                       .name = "test",
                                       .flags = {true, false, true},
                                       .variant = test_enum::second};
    const auto options = generate_stringify_json_options();
    const auto str = serialize_config_to_string(data, options);
    if (options.pretty) {
        REQUIRE(str == R"({
  "count": 42,
  "name": "test",
  "flags": [
    true,
    false,
    true
  ],
  "variant": "second"
})");
    } else {
        REQUIRE(str == R"({"count":42,"name":"test","flags":[true,false,true],"variant":"second"})");
    }
}

} // namespace reflex::config

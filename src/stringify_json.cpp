// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <reflex/config/stringify_json.hpp>

#include <array>
#include <cassert>
#include <charconv>
#include <limits>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>

namespace reflex::config {

namespace {

using namespace std::string_view_literals;

// https://stackoverflow.com/a/68475665
constexpr std::size_t max_double_fixed_length = 2 + std::numeric_limits<double>::max_exponent10 +
                                                std::numeric_limits<double>::max_digits10;

// Based on https://www.boost.org/doc/libs/1_86_0/libs/json/doc/html/json/examples.html
void pretty_json_impl(std::ostream &os,
                      const boost::json::value &value,
                      const stringify_json_options_s &options,
                      std::string &indent_str) {
    switch (value.kind()) {
        case boost::json::kind::null: [[fallthrough]];
        case boost::json::kind::bool_: [[fallthrough]];
        case boost::json::kind::int64: [[fallthrough]];
        case boost::json::kind::uint64: [[fallthrough]];
        case boost::json::kind::string: os << value; break;

        case boost::json::kind::double_: {
            const auto number = value.get_double();

            // match Boost.JSON behavior for inf/nan
            if (options.allow_infinity_and_nan) {
                if (std::isnan(number)) [[unlikely]] {
                    os << "NaN"sv;
                    break;
                }
                if (std::isinf(number)) [[unlikely]] {
                    if (number > 0) {
                        os << "Infinity"sv;
                    } else {
                        os << "-Infinity"sv;
                    }
                    break;
                }
            } else {
                if (std::isnan(number)) [[unlikely]] {
                    os << "null"sv;
                    break;
                }
                if (std::isinf(number)) [[unlikely]] {
                    if (number > 0) {
                        os << "1e99999"sv;
                    } else {
                        os << "-1e99999"sv;
                    }
                    break;
                }
            }

            std::array<char, max_double_fixed_length> buffer;
            const auto [end, ec] = std::to_chars(buffer.data(),
                                                 buffer.data() + buffer.size(),
                                                 number,
                                                 std::chars_format::fixed);
            if (ec != std::errc{}) {
                throw std::system_error{std::make_error_code(ec), "Failed to format a double as JSON"};
            }
            os << std::string_view{buffer.data(), end};
            break;
        }

        case boost::json::kind::array: {
            const auto &arr = value.get_array();
            if (arr.empty()) {
                os << "[]"sv;
                break;
            }
            os << "[\n"sv;
            indent_str.append(options.indent, ' ');
            auto it = arr.begin();
            for (;;) {
                os << indent_str;
                pretty_json_impl(os, *it, options, indent_str);
                if (++it == arr.end()) {
                    break;
                }
                os << ",\n"sv;
            }
            os << '\n';
            indent_str.resize(indent_str.size() - options.indent);
            os << indent_str << ']';
            break;
        }

        case boost::json::kind::object: {
            const auto &obj = value.get_object();
            bool encountered_value = false;
            os << '{';
            for (const auto &kv : obj) {
                const auto should_skip = kv.value().kind() == boost::json::kind::null &&
                                         options.omit_null_object_values;
                if (!should_skip) {
                    if (encountered_value) {
                        os << ",\n"sv;
                    } else {
                        os << '\n';
                        indent_str.append(options.indent, ' ');
                        encountered_value = true;
                    }
                    os << indent_str << boost::json::serialize(kv.key()) << ": "sv;
                    pretty_json_impl(os, kv.value(), options, indent_str);
                }
            }
            if (encountered_value) {
                os << '\n';
                indent_str.resize(indent_str.size() - options.indent);
                os << indent_str;
            }
            os << '}';
            break;
        }
    }
}

std::string stringify_json_pretty(const boost::json::value &value, const stringify_json_options_s &options) {
    assert(options.pretty);
    auto stream = std::stringstream{};
    stream << boost::json::serialize_options{.allow_infinity_and_nan = options.allow_infinity_and_nan};
    auto indent = std::string{};
    pretty_json_impl(stream, value, options, indent);
    return std::move(stream).str();
}

} // namespace

std::string stringify_json(const boost::json::value &value, const stringify_json_options_s options) {
    if (options.pretty) {
        return stringify_json_pretty(value, options);
    }
    return boost::json::serialize(value, {.allow_infinity_and_nan = options.allow_infinity_and_nan});
}

} // namespace reflex::config

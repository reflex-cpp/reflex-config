// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <reflex/config/serialize.hpp>

#include <fmt/ostream.h>

#include <boost/json/kind.hpp>
#include <boost/json/serialize.hpp>

#include <reflex/config/errors.hpp>
#include <reflex/config/format_error_state.hpp>

#include "detail/fmt_compat.hpp"

namespace reflex::config::detail {

std::unexpected<ErrorState> make_key_did_not_serialized_to_string(const Context &context,
                                                                  const boost::json::kind actual_kind) {
    if (context.should_suppress_error_formatting()) {
        return std::unexpected<ErrorState>{std::in_place};
    }
    return format_serialization_error(context,
                                      "key did not serialize to string (actual kind: {})",
                                      boost::json::to_string(actual_kind));
}

namespace {

std::unexpected<ErrorState> make_unknown_enum_value_impl_impl(const Context &context, const auto value) {
    if (context.should_suppress_error_formatting()) {
        return std::unexpected<ErrorState>{std::in_place};
    }
    return format_serialization_error(context, "unknown enumeration value {}", value);
}

} // namespace

std::unexpected<ErrorState> make_unknown_enum_value_impl(const Context &context, const std::intmax_t value) {
    return make_unknown_enum_value_impl_impl(context, value);
}

std::unexpected<ErrorState> make_unknown_enum_value_impl(const Context &context, const std::uintmax_t value) {
    return make_unknown_enum_value_impl_impl(context, value);
}

expected_t<boost::json::value> try_serialize_handler::handle_monostate(
        Context & /*context*/,
        const context_entry_s & /*entry*/,
        std::monostate /*value*/,
        const boost::json::storage_ptr &memory_resource) {
    return expected_t<boost::json::value>{std::in_place, nullptr, memory_resource};
}

expected_t<boost::json::value> try_serialize_handler::handle_bool(Context & /*context*/,
                                                                  const context_entry_s & /*entry*/,
                                                                  const bool value,
                                                                  const boost::json::storage_ptr &memory_resource) {
    return expected_t<boost::json::value>{std::in_place, value, memory_resource};
}

expected_t<boost::json::value> try_serialize_handler::handle_boost_json_value(
        Context & /*context*/,
        const context_entry_s & /*entry*/,
        const boost::json::value &value,
        const boost::json::storage_ptr &memory_resource) {
    return expected_t<boost::json::value>{std::in_place, value, memory_resource};
}

expected_t<boost::json::value> try_serialize_handler::handle_boost_json_object(
        Context & /*context*/,
        const context_entry_s & /*entry*/,
        const boost::json::object &value,
        const boost::json::storage_ptr &memory_resource) {
    return expected_t<boost::json::value>{std::in_place, value, memory_resource};
}

expected_t<boost::json::value> try_serialize_handler::handle_boost_json_array(
        Context & /*context*/,
        const context_entry_s & /*entry*/,
        const boost::json::array &value,
        const boost::json::storage_ptr &memory_resource) {
    return expected_t<boost::json::value>{std::in_place, value, memory_resource};
}

} // namespace reflex::config::detail

// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <reflex/config/format_error_state.hpp>

#include <boost/json/string.hpp>

#include <reflex/config/context.hpp>

#include "detail/fmt_compat.hpp"

namespace reflex::config {

std::unexpected<ErrorState> vformat_error_state(const fmt::string_view format_string,
                                                const fmt::format_args args,
                                                move_from_t,
                                                const std::span<ErrorState> nested_states) {
    auto buffer = fmt::memory_buffer{};
    fmt::vformat_to(REFLEX_CONFIG_FMT_APPENDER(buffer), format_string, args);
    return std::unexpected<ErrorState>{std::in_place,
                                       std::string_view{buffer.data(), buffer.size()},
                                       move_from,
                                       nested_states};
}

namespace {

std::unexpected<ErrorState> vformat_error_with_context(const Context &context,
                                                       const std::string_view operation_name,
                                                       const fmt::string_view format_string,
                                                       const fmt::format_args args,
                                                       move_from_t,
                                                       const std::span<ErrorState> nested_states) {
    if (context.should_suppress_error_formatting()) {
        return std::unexpected<ErrorState>{std::in_place};
    }
    auto buffer = fmt::memory_buffer{};
    fmt::format_to(REFLEX_CONFIG_FMT_APPENDER(buffer),
                   "{} error at {}: ",
                   operation_name,
                   REFLEX_CONFIG_FMT_STREAMED(context.as_json_pointer()));
    fmt::vformat_to(REFLEX_CONFIG_FMT_APPENDER(buffer), format_string, args);
    return std::unexpected<ErrorState>{std::in_place,
                                       std::string_view{buffer.data(), buffer.size()},
                                       move_from,
                                       nested_states};
}

} // namespace

std::unexpected<ErrorState> vformat_serialization_error(const Context &context,
                                                        const fmt::string_view format_string,
                                                        const fmt::format_args args,
                                                        move_from_t,
                                                        const std::span<ErrorState> nested_states) {
    return vformat_error_with_context(context, "serialization", format_string, args, move_from, nested_states);
}

std::unexpected<ErrorState> vformat_deserialization_error(const Context &context,
                                                          const fmt::string_view format_string,
                                                          const fmt::format_args args,
                                                          move_from_t,
                                                          const std::span<ErrorState> nested_states) {
    return vformat_error_with_context(context, "deserialization", format_string, args, move_from, nested_states);
}

std::unexpected<ErrorState> vformat_validation_error(const Context &context,
                                                     const fmt::string_view format_string,
                                                     const fmt::format_args args,
                                                     move_from_t,
                                                     const std::span<ErrorState> nested_states) {
    return vformat_error_with_context(context, "validation", format_string, args, move_from, nested_states);
}

} // namespace reflex::config

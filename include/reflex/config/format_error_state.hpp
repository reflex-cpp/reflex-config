// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <expected>
#include <span>

#include <fmt/format.h>

#include <reflex/config/errors.hpp>
#include <reflex/config/fwd.hpp>

namespace reflex::config {

std::unexpected<ErrorState> vformat_error_state(fmt::string_view format_string,
                                                fmt::format_args args,
                                                move_from_t,
                                                std::span<ErrorState> nested_states);

std::unexpected<ErrorState> vformat_serialization_error(const Context &context,
                                                        fmt::string_view format_string,
                                                        fmt::format_args args,
                                                        move_from_t,
                                                        std::span<ErrorState> nested_states);

std::unexpected<ErrorState> vformat_deserialization_error(const Context &context,
                                                          fmt::string_view format_string,
                                                          fmt::format_args args,
                                                          move_from_t,
                                                          std::span<ErrorState> nested_states);

std::unexpected<ErrorState> vformat_validation_error(const Context &context,
                                                     fmt::string_view format_string,
                                                     fmt::format_args args,
                                                     move_from_t,
                                                     std::span<ErrorState> nested_states);

#if FMT_VERSION >= 80000

    #if FMT_VERSION >= 100000
        #define REFLEX_CONFIG_FMT_STR_TO_SV(fmt_str) (fmt_str).get()
    #else
        #define REFLEX_CONFIG_FMT_STR_TO_SV(fmt_str) (fmt_str) // implicit cast deprecated in fmt 12.2.0
    #endif

template<typename... Args>
std::unexpected<ErrorState> format_error_state(const fmt::format_string<Args...> format_string, Args &&...args) {
    return vformat_error_state(REFLEX_CONFIG_FMT_STR_TO_SV(format_string),
                               fmt::make_format_args(args...),
                               move_from,
                               {});
}

template<typename... Args>
std::unexpected<ErrorState> format_error_state(move_from_t,
                                               const std::span<ErrorState> nested_states,
                                               const fmt::format_string<Args...> format_string,
                                               Args &&...args) {
    return vformat_error_state(REFLEX_CONFIG_FMT_STR_TO_SV(format_string),
                               fmt::make_format_args(args...),
                               move_from,
                               nested_states);
}

template<typename... Args>
std::unexpected<ErrorState> format_serialization_error(const Context &context,
                                                       const fmt::format_string<Args...> format_string,
                                                       Args &&...args) {
    return vformat_serialization_error(context,
                                       REFLEX_CONFIG_FMT_STR_TO_SV(format_string),
                                       fmt::make_format_args(args...),
                                       move_from,
                                       {});
}

template<typename... Args>
std::unexpected<ErrorState> format_serialization_error(const Context &context,
                                                       move_from_t,
                                                       const std::span<ErrorState> nested_states,
                                                       const fmt::format_string<Args...> format_string,
                                                       Args &&...args) {
    return vformat_serialization_error(context,
                                       REFLEX_CONFIG_FMT_STR_TO_SV(format_string),
                                       fmt::make_format_args(args...),
                                       move_from,
                                       nested_states);
}

template<typename... Args>
std::unexpected<ErrorState> format_deserialization_error(const Context &context,
                                                         const fmt::format_string<Args...> format_string,
                                                         Args &&...args) {
    return vformat_deserialization_error(context,
                                         REFLEX_CONFIG_FMT_STR_TO_SV(format_string),
                                         fmt::make_format_args(args...),
                                         move_from,
                                         {});
}

template<typename... Args>
std::unexpected<ErrorState> format_deserialization_error(const Context &context,
                                                         move_from_t,
                                                         const std::span<ErrorState> nested_states,
                                                         const fmt::format_string<Args...> format_string,
                                                         Args &&...args) {
    return vformat_deserialization_error(context,
                                         REFLEX_CONFIG_FMT_STR_TO_SV(format_string),
                                         fmt::make_format_args(args...),
                                         move_from,
                                         nested_states);
}

template<typename... Args>
std::unexpected<ErrorState> format_validation_error(const Context &context,
                                                    const fmt::format_string<Args...> format_string,
                                                    Args &&...args) {
    return vformat_validation_error(context,
                                    REFLEX_CONFIG_FMT_STR_TO_SV(format_string),
                                    fmt::make_format_args(args...),
                                    move_from,
                                    {});
}

template<typename... Args>
std::unexpected<ErrorState> format_validation_error(const Context &context,
                                                    move_from_t,
                                                    const std::span<ErrorState> nested_states,
                                                    const fmt::format_string<Args...> format_string,
                                                    Args &&...args) {
    return vformat_validation_error(context,
                                    REFLEX_CONFIG_FMT_STR_TO_SV(format_string),
                                    fmt::make_format_args(args...),
                                    move_from,
                                    nested_states);
}


    #undef REFLEX_CONFIG_FMT_STR_TO_SV

#else

template<typename FormatString, typename... Args>
    requires(!std::same_as<std::remove_cvref_t<FormatString>, move_from_t>)
std::unexpected<ErrorState> format_error_state(FormatString &&format_string, Args &&...args) {
    const auto vargs = fmt::make_args_checked<Args...>(format_string, args...);
    return vformat_error_state(fmt::to_string_view(format_string), vargs, move_from, {});
}

template<typename FormatString, typename... Args>
std::unexpected<ErrorState> format_error_state(move_from_t,
                                               const std::span<ErrorState> nested_states,
                                               FormatString &&format_string,
                                               Args &&...args) {
    const auto vargs = fmt::make_args_checked<Args...>(format_string, args...);
    return vformat_error_state(fmt::to_string_view(format_string), vargs, move_from, nested_states);
}

template<typename FormatString, typename... Args>
    requires(!std::same_as<std::remove_cvref_t<FormatString>, move_from_t>)
std::unexpected<ErrorState> format_serialization_error(const Context &context,
                                                       FormatString &&format_string,
                                                       Args &&...args) {
    const auto vargs = fmt::make_args_checked<Args...>(format_string, args...);
    return vformat_serialization_error(context, fmt::to_string_view(format_string), vargs, move_from, {});
}

template<typename FormatString, typename... Args>
std::unexpected<ErrorState> format_serialization_error(const Context &context,
                                                       move_from_t,
                                                       const std::span<ErrorState> nested_states,
                                                       FormatString &&format_string,
                                                       Args &&...args) {
    const auto vargs = fmt::make_args_checked<Args...>(format_string, args...);
    return vformat_serialization_error(context, fmt::to_string_view(format_string), vargs, move_from, nested_states);
}

template<typename FormatString, typename... Args>
    requires(!std::same_as<std::remove_cvref_t<FormatString>, move_from_t>)
std::unexpected<ErrorState> format_deserialization_error(const Context &context,
                                                         FormatString &&format_string,
                                                         Args &&...args) {
    const auto vargs = fmt::make_args_checked<Args...>(format_string, args...);
    return vformat_deserialization_error(context, fmt::to_string_view(format_string), vargs, move_from, {});
}

template<typename FormatString, typename... Args>
std::unexpected<ErrorState> format_deserialization_error(const Context &context,
                                                         move_from_t,
                                                         const std::span<ErrorState> nested_states,
                                                         FormatString &&format_string,
                                                         Args &&...args) {
    const auto vargs = fmt::make_args_checked<Args...>(format_string, args...);
    return vformat_deserialization_error(context, fmt::to_string_view(format_string), vargs, move_from, nested_states);
}

template<typename FormatString, typename... Args>
    requires(!std::same_as<std::remove_cvref_t<FormatString>, move_from_t>)
std::unexpected<ErrorState> format_validation_error(const Context &context,
                                                    FormatString &&format_string,
                                                    Args &&...args) {
    const auto vargs = fmt::make_args_checked<Args...>(format_string, args...);
    return vformat_validation_error(context, fmt::to_string_view(format_string), vargs, move_from, {});
}

template<typename FormatString, typename... Args>
std::unexpected<ErrorState> format_validation_error(const Context &context,
                                                    move_from_t,
                                                    const std::span<ErrorState> nested_states,
                                                    FormatString &&format_string,
                                                    Args &&...args) {
    const auto vargs = fmt::make_args_checked<Args...>(format_string, args...);
    return vformat_validation_error(context, fmt::to_string_view(format_string), vargs, move_from, nested_states);
}

#endif

} // namespace reflex::config

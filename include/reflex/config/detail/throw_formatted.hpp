// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <fmt/format.h>

namespace reflex::config::detail {

[[nodiscard]] fmt::memory_buffer vformat_null_terminated(fmt::string_view format_string, fmt::format_args args);

template<typename Exception>
[[noreturn]] void throw_vfoprmatted(const fmt::string_view format_string, const fmt::format_args args) {
    throw Exception{vformat_null_terminated(format_string, args).data()};
}

#if FMT_VERSION >= 80000
template<typename Exception, typename... Args>
[[noreturn]] void throw_formatted(const fmt::format_string<Args...> format_string, Args &&...args) {
    throw_vfoprmatted<Exception>(
    #if FMT_VERSION >= 100000
            format_string.get(),
    #else
            format_string, // implicit cast deprecated in fmt 12.2.0
    #endif
            fmt::make_format_args(args...));
}
#else
template<typename Exception, typename FormatString, typename... Args>
[[noreturn]] void throw_formatted(FormatString &&format_string, Args &&...args) {
    const auto vargs = fmt::make_args_checked<Args...>(format_string, args...);
    throw_vfoprmatted<Exception>(fmt::to_string_view(format_string), vargs);
}
#endif

} // namespace reflex::config::detail

// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <reflex/config/detail/throw_formatted.hpp>

#include "fmt_compat.hpp"

namespace reflex::config::detail {

fmt::memory_buffer vformat_null_terminated(const fmt::string_view format_string, const fmt::format_args args) {
    auto buffer = fmt::memory_buffer{};
    fmt::vformat_to(REFLEX_CONFIG_FMT_APPENDER(buffer), format_string, args);
    buffer.push_back('\0');
    return buffer;
}

} // namespace reflex::config::detail

// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <reflex/config/validators.hpp>

#include <regex>

#include <boost/json/serialize.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include <reflex/config/context.hpp>
#include <reflex/config/errors.hpp>

#include "detail/fmt_compat.hpp"

namespace reflex::config {

namespace {
constexpr std::string_view g_number_0_255 = "((([0-9])|([1-9][0-9])|(1[0-9][0-9]|2[0-4][0-9]|25[0-5])){1})";
const std::regex g_ip_regex(fmt::format(R"({0}\.{0}\.{0}\.{0})", g_number_0_255));
} // namespace

expected_t<void> ip_v4_address::validate(const Context &context, const std::string_view value) const {
    if (!std::regex_match(value.begin(), value.end(), g_ip_regex)) {
        return format_validation_error(context, "valid IPv4 address");
    }
    return {};
}

} // namespace reflex::config

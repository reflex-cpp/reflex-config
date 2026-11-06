// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <reflex/config/fixed_value.hpp>

#include <boost/json/string.hpp>

#include <reflex/config/context.hpp>
#include <reflex/config/format_error_state.hpp>

#include "detail/fmt_compat.hpp"

namespace reflex::config::detail {

std::unexpected<ErrorState> make_wrong_fixed_value(const Context &context) {
    if (context.should_suppress_error_formatting()) {
        return std::unexpected<ErrorState>{std::in_place};
    }
    return format_deserialization_error(context, "wrong fixed value");
}

} // namespace reflex::config::detail

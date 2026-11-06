// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <catch2/generators/catch_generators.hpp>

#include <reflex/config/stringify_json.hpp>

namespace reflex::config {

inline auto generate_stringify_json_options() {
    return GENERATE( // clang-format off
        stringify_json_options_s{.allow_infinity_and_nan = false, .pretty = false, .omit_null_object_values = false}, // 000
        stringify_json_options_s{.allow_infinity_and_nan = false, .pretty = false, .omit_null_object_values = true}, // 001
        stringify_json_options_s{.allow_infinity_and_nan = false, .pretty = true, .omit_null_object_values = false}, // 010
        stringify_json_options_s{.allow_infinity_and_nan = false, .pretty = true, .omit_null_object_values = true}, // 011
        stringify_json_options_s{.allow_infinity_and_nan = true, .pretty = false, .omit_null_object_values = false}, // 100
        stringify_json_options_s{.allow_infinity_and_nan = true, .pretty = false, .omit_null_object_values = true}, // 101
        stringify_json_options_s{.allow_infinity_and_nan = true, .pretty = true, .omit_null_object_values = false}, // 110
        stringify_json_options_s{.allow_infinity_and_nan = true, .pretty = true, .omit_null_object_values = true} // 111
    ); // clang-format on
}

} // namespace reflex::config

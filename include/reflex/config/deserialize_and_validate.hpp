// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <utility>

#include <boost/json/fwd.hpp>

#include <reflex/config/deserialize.hpp>
#include <reflex/config/fwd.hpp>
#include <reflex/config/validate.hpp>

namespace reflex::config {

template<typename T>
[[nodiscard]] T deserialize_and_validate_config(const boost::json::value &value, deserialization_options_s options) {
    auto result = deserialize_config<T>(value, std::move(options));
    validate_config(result);
    return result;
}

template<typename T>
[[nodiscard]] T deserialize_and_validate_config_from_string(const std::string_view string,
                                                            const boost::json::parse_options &parse_options,
                                                            deserialization_options_s options) {
    auto result = deserialize_config_from_string<T>(string, parse_options, std::move(options));
    validate_config(result);
    return result;
}

} // namespace reflex::config

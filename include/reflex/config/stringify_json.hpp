// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

#include <boost/json/fwd.hpp>

#include <reflex/config/fwd.hpp>

namespace reflex::config {

[[nodiscard]] std::string stringify_json(const boost::json::value &value, stringify_json_options_s options = {});

} // namespace reflex::config

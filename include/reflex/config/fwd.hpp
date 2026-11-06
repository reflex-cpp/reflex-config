// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <expected>
#include <functional>
#include <string>
#include <string_view>

#include <boost/json/fwd.hpp>
#include <boost/json/parse_options.hpp>
#include <boost/json/storage_ptr.hpp>

// Work around error in std::move_only_function:
// "invalid application of 'sizeof' to an incomplete type 'const boost::json::string'".
// Fixed in 12 by https://github.com/gcc-mirror/gcc/commit/9c45321cdbf48419ced5d7d442e2f033ab8f476e.
// Fixed in 13 and above by https://github.com/gcc-mirror/gcc/commit/33de0ffcf050e581eb931eb4f1c5ad9c0cdc6bf6.
#if _GLIBCXX_RELEASE == 12 && __GLIBCXX__ < 20221114 || _GLIBCXX_RELEASE > 12 && __GLIBCXX__ < 20221021
    #include <boost/json/string.hpp>
#endif

namespace reflex::config {

struct context_entry_s;
class ContextGuard;
class Context;
class DeserializationContext;

class ErrorState;
template<typename T>
using expected_t = std::expected<T, ErrorState>;

struct deserialization_options_s {
    bool allow_unknown_keys = false;

    std::move_only_function<void(const boost::json::string &where, const boost::json::string &key)> on_unknown_key =
            nullptr;
};

struct stringify_json_options_s {
    bool allow_infinity_and_nan = false;
    bool pretty = false;

    // pretty printing options, ignored otherwise:
    bool omit_null_object_values = false;
    std::size_t indent = 2;
};

template<typename T>
expected_t<boost::json::value> try_serialize_value(Context &context,
                                                   const context_entry_s &entry,
                                                   const T &value,
                                                   const boost::json::storage_ptr &memory_resource);

template<typename T>
[[nodiscard]] boost::json::value serialize_config(const T &value, boost::json::storage_ptr memory_resource = {});

template<typename T>
[[nodiscard]] std::string serialize_config_to_string(const T &value,
                                                     const stringify_json_options_s &stringify_options = {});

template<typename T>
expected_t<T> try_deserialize_value(DeserializationContext &context,
                                    const context_entry_s &entry,
                                    const boost::json::value &value);

template<typename T>
[[nodiscard]] T deserialize_config(const boost::json::value &value, deserialization_options_s options = {});

template<typename T>
[[nodiscard]] T deserialize_config_from_string(std::string_view string,
                                               const boost::json::parse_options &parse_options = {},
                                               deserialization_options_s options = {});

template<typename T>
expected_t<void> try_validate_value(Context &context, const context_entry_s &entry, const T &value);

template<typename T>
void validate_config(const T &value);

template<typename T>
[[nodiscard]] T deserialize_and_validate_config(const boost::json::value &value,
                                                deserialization_options_s options = {});

template<typename T>
[[nodiscard]] T deserialize_and_validate_config_from_string(std::string_view string,
                                                            const boost::json::parse_options &parse_options = {},
                                                            deserialization_options_s options = {});

} // namespace reflex::config

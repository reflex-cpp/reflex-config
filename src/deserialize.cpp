// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <reflex/config/deserialize.hpp>

#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <boost/json/kind.hpp>
#include <boost/json/serialize.hpp>

#include <reflex/ranges/contains.hpp>
#include <reflex/ranges/zip_view.hpp>

#include <reflex/config/errors.hpp>
#include <reflex/config/format_error_state.hpp>

#include "detail/fmt_compat.hpp"

namespace reflex::config {

DeserializationContext::DeserializationContext(deserialization_options_s options) : m_options{std::move(options)} {}

expected_t<void> DeserializationContext::on_unknown_key(const boost::json::string &key) {
    if (!m_options.allow_unknown_keys) {
        return detail::make_unknown_key(*this, key);
    }
    if (m_options.on_unknown_key) {
        const auto where = as_json_pointer();
        m_options.on_unknown_key(where, key);
    }
    return {};
}

namespace detail {

std::unexpected<ErrorState> make_wrong_kind(const Context &context,
                                            const std::span<const boost::json::kind> expected,
                                            const boost::json::kind actual) {
    if (context.should_suppress_error_formatting()) {
        return std::unexpected<ErrorState>{std::in_place};
    }
    const auto add_null = context.is_nullable() && !reflex::ranges::contains(expected, boost::json::kind::null);
    return format_deserialization_error(
            context,
            "wrong value kind (expected: {}{}, actual: {})",
            add_null ? std::string_view{"null | "} : std::string_view{},
            fmt::join(expected | std::views::transform([](const auto kind) -> std::string_view {
                          return boost::json::to_string(kind);
                      }),
                      " | "),
            boost::json::to_string(actual));
}

std::unexpected<ErrorState> make_to_number_ec(const Context &context,
                                              const boost::json::value &value,
                                              const std::error_code ec,
                                              const std::string_view type_name) {
    if (context.should_suppress_error_formatting()) {
        return std::unexpected<ErrorState>{std::in_place};
    }
    if (ec == boost::json::error::not_number) {
        return format_deserialization_error(context,
                                            "wrong value kind (expected: {}number, actual: {})",
                                            context.is_nullable() ? std::string_view{"null | "} : std::string_view{},
                                            REFLEX_CONFIG_FMT_STREAMED(value.kind()));
    }
    if (ec == boost::json::error::not_exact) {
        const auto visitor = [&](const auto actual_value) {
            return format_deserialization_error(context,
                                                "no exact conversion to {} from {} (kind: {})",
                                                type_name,
                                                actual_value,
                                                REFLEX_CONFIG_FMT_STREAMED(value.kind()));
        };
        switch (value.kind()) {
            case boost::json::kind::int64: return visitor(value.get_int64());
            case boost::json::kind::uint64: return visitor(value.get_uint64());
            case boost::json::kind::double_: return visitor(value.get_double());
            default: assert(false && "unexpected number kind"); break;
        }
        std::unreachable();
    }
    return format_deserialization_error(context, "number conversion failed ({})", ec.message());
}

std::unexpected<ErrorState> make_wrong_array_size(const Context &context,
                                                  const std::size_t expected,
                                                  const std::size_t actual) {
    if (context.should_suppress_error_formatting()) {
        return std::unexpected<ErrorState>{std::in_place};
    }
    return format_deserialization_error(context, "wrong array size (expected: {}, actual: {})", expected, actual);
}

std::unexpected<ErrorState> make_unknown_enumerator(const Context &context, const std::string_view actual) {
    if (context.should_suppress_error_formatting()) {
        return std::unexpected<ErrorState>{std::in_place};
    }
    return format_deserialization_error(context,
                                        "unknown enumerator {}",
                                        REFLEX_CONFIG_FMT_STREAMED(boost::json::serialize(actual)));
}

std::unexpected<ErrorState> make_missing_key(const Context &context, const std::string_view name) {
    if (context.should_suppress_error_formatting()) {
        return std::unexpected<ErrorState>{std::in_place};
    }
    return format_deserialization_error(context,
                                        "missing key {}",
                                        REFLEX_CONFIG_FMT_STREAMED(boost::json::serialize(name)));
}

std::unexpected<ErrorState> make_unknown_key(const Context &context, const std::string_view name) {
    if (context.should_suppress_error_formatting()) {
        return std::unexpected<ErrorState>{std::in_place};
    }
    return format_deserialization_error(context,
                                        "unknown key {}",
                                        REFLEX_CONFIG_FMT_STREAMED(boost::json::serialize(name)));
}

std::unexpected<ErrorState> make_ignored_member(const Context &context, const std::string_view name) {
    if (context.should_suppress_error_formatting()) {
        return std::unexpected<ErrorState>{std::in_place};
    }
    return format_deserialization_error(context,
                                        "ignored member {}",
                                        REFLEX_CONFIG_FMT_STREAMED(boost::json::serialize(name)));
}

std::unexpected<ErrorState> make_no_alternatives_matched(const Context &context,
                                                         move_from_t,
                                                         const std::span<ErrorState> nested_errors) {
    if (context.should_suppress_error_formatting()) {
        return std::unexpected<ErrorState>{std::in_place};
    }
    return format_deserialization_error(context, move_from, nested_errors, "no alternatives matched");
}


std::optional<std::size_t> find_enumerator(const std::span<const std::string_view> names,
                                           const std::string_view string) noexcept {
    const auto name_it = std::ranges::find(names, string);
    if (name_it == names.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::ranges::distance(names.begin(), name_it));
}

expected_t<void> find_object_member_values(DeserializationContext &context,
                                           const boost::json::object &object,
                                           const std::span<const std::string_view> names,
                                           const std::span<const bool> are_required,
                                           const std::span<const bool> are_ignored,
                                           const std::span<const boost::json::value *> values) {
    assert(are_required.size() == names.size());
    assert(are_ignored.size() == names.size());
    assert(values.size() == names.size());

    auto used_member_count = 0uz;
    for (const auto [name, is_required, is_ignored, value] :
         reflex::views::zip(names, are_required, are_ignored, values)) {
        static_assert(std::same_as<decltype(value), const boost::json::value *&>);
        const auto entry = object.find(name);
        if (entry == object.end()) {
            if (is_required && !is_ignored) {
                return make_missing_key(context, name);
            }
            value = nullptr;
        } else {
            if (is_ignored) {
                return make_ignored_member(context, name);
            }

            value = &entry->value();
            ++used_member_count;
        }
    }
    if (used_member_count != object.size()) {
        assert(used_member_count < object.size());
        if (!context.options().allow_unknown_keys || context.options().on_unknown_key) {
            for (const auto &entry : object) { // TODO: Make this lookup not quadratic (maybe use a constexpr hash-set)
                if (std::ranges::find(names, std::string_view{entry.key()}) == names.end()) {
                    if (auto ex = context.on_unknown_key(entry.key()); !ex) {
                        return std::unexpected{std::move(ex).error()};
                    }
                }
            }
        }
    }
    return {};
}


expected_t<std::monostate> try_deserialize_handler::handle_monostate(DeserializationContext &context,
                                                                     const context_entry_s &entry,
                                                                     const boost::json::value &value) {
    const auto guard = ContextGuard{context, entry};

    if (auto exp_kind = check_kind(context, value, boost::json::kind::null); !exp_kind) {
        return std::unexpected{std::move(exp_kind).error()};
    }

    return expected_t<std::monostate>{std::in_place};
}

expected_t<bool> try_deserialize_handler::handle_bool(DeserializationContext &context,
                                                      const context_entry_s &entry,
                                                      const boost::json::value &value) {
    const auto guard = ContextGuard{context, entry};

    if (auto exp_kind = check_kind(context, value, boost::json::kind::bool_); !exp_kind) {
        return std::unexpected{std::move(exp_kind).error()};
    }

    return expected_t<bool>{value.get_bool()};
}

expected_t<boost::json::value> try_deserialize_handler::handle_boost_json_value(DeserializationContext &context,
                                                                                const context_entry_s &entry,
                                                                                const boost::json::value &value) {
    const auto guard = ContextGuard{context, entry};

    return boost::json::value(value, boost::json::storage_ptr{});
}

expected_t<boost::json::object> try_deserialize_handler::handle_boost_json_object(DeserializationContext &context,
                                                                                  const context_entry_s &entry,
                                                                                  const boost::json::value &value) {
    const auto guard = ContextGuard{context, entry};

    if (auto exp_kind = check_kind(context, value, boost::json::kind::object); !exp_kind) {
        return std::unexpected{std::move(exp_kind).error()};
    }

    return expected_t<boost::json::object>{std::in_place, value.get_object(), boost::json::storage_ptr{}};
}

expected_t<boost::json::array> try_deserialize_handler::handle_boost_json_array(DeserializationContext &context,
                                                                                const context_entry_s &entry,
                                                                                const boost::json::value &value) {
    const auto guard = ContextGuard{context, entry};

    if (auto exp_kind = check_kind(context, value, boost::json::kind::array); !exp_kind) {
        return std::unexpected{std::move(exp_kind).error()};
    }

    return expected_t<boost::json::array>{std::in_place, value.get_array(), boost::json::storage_ptr{}};
}

} // namespace detail

template expected_t<std::monostate> try_deserialize_value<std::monostate>(DeserializationContext &context,
                                                                          const context_entry_s &entry,
                                                                          const boost::json::value &value);

template expected_t<bool> try_deserialize_value<bool>(DeserializationContext &context,
                                                      const context_entry_s &entry,
                                                      const boost::json::value &value);

template expected_t<std::int8_t> try_deserialize_value<std::int8_t>(DeserializationContext &context,
                                                                    const context_entry_s &entry,
                                                                    const boost::json::value &value);
template expected_t<std::int16_t> try_deserialize_value<std::int16_t>(DeserializationContext &context,
                                                                      const context_entry_s &entry,
                                                                      const boost::json::value &value);
template expected_t<std::int32_t> try_deserialize_value<std::int32_t>(DeserializationContext &context,
                                                                      const context_entry_s &entry,
                                                                      const boost::json::value &value);
template expected_t<std::int64_t> try_deserialize_value<std::int64_t>(DeserializationContext &context,
                                                                      const context_entry_s &entry,
                                                                      const boost::json::value &value);
template expected_t<std::uint8_t> try_deserialize_value<std::uint8_t>(DeserializationContext &context,
                                                                      const context_entry_s &entry,
                                                                      const boost::json::value &value);
template expected_t<std::uint16_t> try_deserialize_value<std::uint16_t>(DeserializationContext &context,
                                                                        const context_entry_s &entry,
                                                                        const boost::json::value &value);
template expected_t<std::uint32_t> try_deserialize_value<std::uint32_t>(DeserializationContext &context,
                                                                        const context_entry_s &entry,
                                                                        const boost::json::value &value);
template expected_t<std::uint64_t> try_deserialize_value<std::uint64_t>(DeserializationContext &context,
                                                                        const context_entry_s &entry,
                                                                        const boost::json::value &value);
template expected_t<float> try_deserialize_value<float>(DeserializationContext &context,
                                                        const context_entry_s &entry,
                                                        const boost::json::value &value);
template expected_t<double> try_deserialize_value<double>(DeserializationContext &context,
                                                          const context_entry_s &entry,
                                                          const boost::json::value &value);

template expected_t<std::string> try_deserialize_value<std::string>(DeserializationContext &context,
                                                                    const context_entry_s &entry,
                                                                    const boost::json::value &value);

} // namespace reflex::config

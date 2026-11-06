// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <concepts>

#include <reflex/config/context.hpp>
#include <reflex/config/errors.hpp>
#include <reflex/config/fwd.hpp>
#include <reflex/config/reflection_helpers.hpp>

namespace reflex::config {

template<auto V>
class FixedValue {
public:
    using value_type = decltype(V);
    static_assert(std::equality_comparable<value_type>);

    [[nodiscard]] explicit(false) constexpr operator value_type() const noexcept { return V; }

    [[nodiscard]] friend bool operator==(const FixedValue &, const FixedValue &) noexcept = default;

    [[nodiscard]] friend constexpr bool operator==(const FixedValue &,
                                                   const value_type &other) noexcept(noexcept(V == other)) {
        return V == other;
    }
};

namespace detail {
std::unexpected<ErrorState> make_wrong_fixed_value(const Context &context);
}

template<auto V>
struct custom_traits<FixedValue<V>> {
private:
    using value_t = FixedValue<V>::value_type;

public:
    // to_string and try_deserialize from std::string_view are not implemented because it doesn't make sense to use a
    // FixedValue as a map's key type.

    static /*expected_t<boost::json::value>*/ auto try_serialize(Context &context,
                                                                 const context_entry_s &entry,
                                                                 FixedValue<V>,
                                                                 const boost::json::storage_ptr &memory_resource) {
        if constexpr (reflex::reflected_enum<value_t>) {
            static_assert(serialized_enumerator_name(V).has_value(), "Enumerator does not have a serialized name");
        }
        return try_serialize_value(context, entry, V, memory_resource);
    }

    static expected_t<FixedValue<V>> try_deserialize(DeserializationContext &context,
                                                     const context_entry_s &entry,
                                                     const boost::json::value &value) {
        if (auto exp_v = try_deserialize_value<value_t>(context, entry, value)) {
            if (*exp_v == V) {
                return {};
            } else {
                const auto guard = ContextGuard{context, entry};
                return detail::make_wrong_fixed_value(context);
            }
        } else {
            return std::unexpected{std::move(exp_v).error()};
        }
    }

    static expected_t<void> try_validate(Context &context, const context_entry_s &entry, FixedValue<V>) {
        return try_validate_value(context, entry, V);
    }
};

} // namespace reflex::config

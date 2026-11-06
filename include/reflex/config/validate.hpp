// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <concepts>
#include <cstddef>
#include <ranges>
#include <utility>
#include <variant>

#include <reflex/meta.hpp>
#include <reflex/ranges/enumerate_view.hpp>
#include <reflex/ranges/zip_view.hpp>
#include <reflex/tuple.hpp>

#include <reflex/config/concepts.hpp>
#include <reflex/config/context.hpp>
#include <reflex/config/errors.hpp>
#include <reflex/config/fwd.hpp>
#include <reflex/config/reflection_helpers.hpp>
#include <reflex/config/validators.hpp>

namespace reflex::config {

namespace detail {

template<typename... Property>
expected_t<void> validate_with_properties(Context &context,
                                          const auto &value,
                                          const reflex::tuple<Property...> &properties) {
    auto result = expected_t<void>{};
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        std::ignore = ([&] {
            if constexpr (std::derived_from<Property, validator_base>) {
                result = reflex::get<I>(properties).validate(context, value);
                return result.has_value();
            } else {
                return true;
            }
        }() && ...); // fold over && to short-circuit on failure
    }(std::make_index_sequence<sizeof...(Property)>{});
    return result;
}

expected_t<void> validate_data_member(Context &context,
                                      const context_entry_s &entry,
                                      const auto &value,
                                      const auto &properties) {
    if (auto exp = try_validate_value(context, entry, value); !exp) {
        return exp;
    }
    {
        // we currently (potentially) enter this context twice, not the most efficient way but should be OK
        const auto guard = ContextGuard{context, entry};
        return validate_with_properties(context, value, properties);
    }
}

struct try_validate_handler {
    template<custom_type Custom>
    static expected_t<void> handle_custom(Context &context, const context_entry_s &entry, const Custom &value) {
        static_assert(requires {
            { custom_traits<Custom>::try_validate(context, entry, value) } -> std::same_as<expected_t<void>>;
        });
        return custom_traits<Custom>::try_validate(context, entry, value);
    }

    static expected_t<void> handle_monostate(const Context & /* context */,
                                             const context_entry_s & /* entry */,
                                             std::monostate /* value */) {
        return {};
    }

    static expected_t<void> handle_bool(const Context & /* context */,
                                        const context_entry_s & /* entry */,
                                        bool /* value */) {
        return {};
    }

    template<numeric Numeric>
    static expected_t<void> handle_numeric(const Context & /* context */,
                                           const context_entry_s & /* entry */,
                                           Numeric /* value */) {
        return {};
    }

    template<string_like String>
    static expected_t<void> handle_string(const Context & /* context */,
                                          const context_entry_s & /* entry */,
                                          const String & /* value */) {
        return {};
    }

    template<optional_like Optional>
    static expected_t<void> handle_optional(Context &context, const context_entry_s &entry, const Optional &optional) {
        if (optional) {
            using value_t = optional_traits<Optional>::value_type;
            return try_validate_value<value_t>(context, {.id = entry.id, .is_nullable = true}, *optional);
        } else {
            return {};
        }
    }

    template<typename Variant>
    static expected_t<void> handle_variant(Context &context, const context_entry_s &entry, const Variant &value) {
        return std::visit(
                [&]<typename Alternative>(const Alternative &alternative) {
                    return try_validate_value<Alternative>(context, entry, alternative);
                },
                value);
    }

    static expected_t<void> handle_boost_json_value(Context & /*context*/,
                                                    const context_entry_s & /*entry*/,
                                                    const boost::json::value & /*value*/) {
        return {};
    }

    static expected_t<void> handle_boost_json_object(Context & /*context*/,
                                                     const context_entry_s & /*entry*/,
                                                     const boost::json::object & /*object*/) {
        return {};
    }

    static expected_t<void> handle_boost_json_array(Context & /*context*/,
                                                    const context_entry_s & /*entry*/,
                                                    const boost::json::array & /*array*/) {
        return {};
    }

    template<typename EIArray>
    static expected_t<void> handle_enum_indexed_array(Context &context,
                                                      const context_entry_s &entry,
                                                      const EIArray &array) {
        const auto guard = ContextGuard{context, entry};

        using traits_t = enum_indexed_array_traits<EIArray>;
        using value_t = traits_t::value_type;
        using key_t = traits_t::key_type;

        constexpr auto &names = names_of_sorted_enumerators<key_t>;
        for (const auto [name, element] : reflex::views::zip(names, array)) {
            if (auto exp = try_validate_value<value_t>(context, {.id = name}, element); !exp) {
                return exp;
            }
        }
        return {};
    }

    template<array_like Array>
    static expected_t<void> handle_array(Context &context, const context_entry_s &entry, const Array &array) {
        const auto guard = ContextGuard{context, entry};

        using value_t = array_traits<Array>::value_type;
        for (const auto idx : std::views::iota(0uz, std::size_t{array_traits<Array>::extent})) {
            if (auto exp = try_validate_value<value_t>(context, {.id = idx}, array[idx]); !exp) {
                return exp;
            }
        }
        return {};
    }

    template<set_like Set>
    static expected_t<void> handle_set(Context &context, const context_entry_s &entry, const Set &set) {
        return try_validate_handler::handle_sequence(context, entry, set);
    }

    template<map_like Map>
    static expected_t<void> handle_map(Context &context, const context_entry_s &entry, const Map &map) {
        const auto guard = ContextGuard{context, entry};

        using key_t = std::remove_cv_t<typename Map::key_type>;
        using mapped_t = std::remove_cv_t<typename Map::mapped_type>;

        for (const auto &[key, value] : map) {
            if (auto key_exp = try_validate_value<key_t>(context, {}, key); !key_exp) {
                return key_exp;
            }

            const auto validate_mapped = [&](const std::string_view key_str) {
                return try_validate_value<mapped_t>(context, {.id = key_str}, value);
            };
            auto exp = [&context, &validate_mapped]<typename K>(this const auto &dispatch_validate_mapped, const K &k) {
                if constexpr (custom_type_with_to_string<K>) {
                    return validate_mapped(std::string_view{custom_traits<K>::to_string(k)});
                } else if constexpr (custom_type<K>) {
                    const auto exp_str = detail::custom_type_to_string(context, k);
                    return validate_mapped(exp_str ? std::string_view{*exp_str} : "<unknown>");
                } else if constexpr (is_variant<K>) {
                    return std::visit([&](const auto &k_alt) { return dispatch_validate_mapped(k_alt); }, k);
                } else if constexpr (string_like<K>) {
                    return validate_mapped({std::ranges::data(k), std::ranges::size(k)});
                } else if constexpr (reflex::reflected_enum<K>) {
                    return validate_mapped(serialized_enumerator_name(k).value_or("<unknown>"));
                } else {
                    static_assert(false);
                }
            }(key);
            if (!exp) {
                return exp;
            }
        }
        return {};
    }

    template<typename Sequence>
        requires sequence_container<Sequence> || set_like<Sequence> // handle_set calls handle_sequence
    static expected_t<void> handle_sequence(Context &context, const context_entry_s &entry, const Sequence &sequence) {
        const auto guard = ContextGuard{context, entry};

        for (const auto [idx, value] : sequence | reflex::views::enumerate) {
            using value_t = std::ranges::range_value_t<Sequence>;
            const auto uidx = static_cast<std::size_t>(idx);
            if (auto exp = try_validate_value<value_t>(context, {.id = uidx}, value); !exp) {
                return exp;
            }
        }
        return {};
    }

    template<reflex::reflected_enum T>
    static expected_t<void> handle_enum(Context &context, const context_entry_s &entry, const T value) {
        const auto guard = ContextGuard{context, entry};

        return validate_with_properties(context, value, reflex::meta<T>::properties);
    }

    template<reflex::reflected_class T>
    static expected_t<void> handle_class(Context &context, const context_entry_s &entry, const T &value) {
        const auto guard = ContextGuard{context, entry};

        using meta_t = reflex::meta<T>;
        static constexpr auto &meta_members = meta_t::data_members;
        constexpr auto member_count = std::tuple_size_v<decltype(meta_t::data_members)>;

        {
            auto mem_exp = expected_t<void>{};
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                std::ignore = ([&] {
                    if constexpr (is_ignored<std::tuple_element_t<I, decltype(meta_t::data_members)>>()) {
                        return true;
                    } else {
                        static constexpr auto &meta_mem = reflex::get<I>(meta_members);
                        static constexpr auto mem_name = serialized_name(meta_mem);
                        mem_exp = validate_data_member(context,
                                                       {.id = mem_name},
                                                       value.*meta_mem.ptr,
                                                       meta_mem.properties);
                        return mem_exp.has_value();
                    }
                }() && ...); // fold over && to short-circuit on failure
            }(std::make_index_sequence<member_count>{});
            if (!mem_exp) {
                return mem_exp;
            }
        }

        return validate_with_properties(context, value, reflex::meta<T>::properties);
    }
};

} // namespace detail

template<typename T>
expected_t<void> try_validate_value(Context &context, const context_entry_s &entry, const T &value) {
    static_assert(unqualified<T>, "The type for validation cannot be cv-qualified");
    return detail::handle_value<detail::try_validate_handler, T>(context, entry, value);
}

template<typename T>
void validate_value(Context &context, const context_entry_s &entry, const T &value) {
    if (auto exp = try_validate_value<T>(context, entry, value); !exp) {
        throw ValidationError{std::move(exp).error()};
    }
}

template<typename T>
void validate_config(const T &value) {
    auto context = Context{};
    validate_value(context, {}, value);
}

} // namespace reflex::config

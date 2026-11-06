// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

#include <boost/json/array.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/monotonic_resource.hpp>
#include <boost/json/storage_ptr.hpp>
#include <boost/json/value.hpp>

#include <reflex/meta.hpp>
#include <reflex/ranges/enumerate_view.hpp>
#include <reflex/ranges/zip_view.hpp>
#include <reflex/tuple.hpp>

#include <reflex/config/concepts.hpp>
#include <reflex/config/context.hpp>
#include <reflex/config/errors.hpp>
#include <reflex/config/fwd.hpp>
#include <reflex/config/reflection_helpers.hpp>
#include <reflex/config/stringify_json.hpp>
#include <reflex/config/traits.hpp>

namespace reflex::config {

namespace detail {

std::unexpected<ErrorState> make_key_did_not_serialized_to_string(const Context &context,
                                                                  boost::json::kind actual_kind);

std::unexpected<ErrorState> make_unknown_enum_value_impl(const Context &context, std::intmax_t value);
std::unexpected<ErrorState> make_unknown_enum_value_impl(const Context &context, std::uintmax_t value);

std::unexpected<ErrorState> make_unknown_enum_value(const Context &context, const std::signed_integral auto value) {
    return make_unknown_enum_value_impl(context, std::intmax_t{value});
}

std::unexpected<ErrorState> make_unknown_enum_value(const Context &context, const std::unsigned_integral auto value) {
    return make_unknown_enum_value_impl(context, std::uintmax_t{value});
}


struct try_serialize_handler {
    template<custom_type Custom>
    static expected_t<boost::json::value> handle_custom(Context &context,
                                                        const context_entry_s &entry,
                                                        const Custom &value,
                                                        const boost::json::storage_ptr &memory_resource) {
        static_assert(requires {
            {
                custom_traits<Custom>::try_serialize(context, entry, value, memory_resource)
            } -> std::same_as<expected_t<boost::json::value>>;
        });
        return custom_traits<Custom>::try_serialize(context, entry, value, memory_resource);
    }

    static expected_t<boost::json::value> handle_monostate(Context &context,
                                                           const context_entry_s &entry,
                                                           std::monostate value,
                                                           const boost::json::storage_ptr &memory_resource);

    static expected_t<boost::json::value> handle_bool(Context &context,
                                                      const context_entry_s &entry,
                                                      bool value,
                                                      const boost::json::storage_ptr &memory_resource);

    template<numeric Numeric>
    static expected_t<boost::json::value> handle_numeric(Context & /*context*/,
                                                         const context_entry_s & /*entry*/,
                                                         const Numeric &value,
                                                         const boost::json::storage_ptr &memory_resource) {
        if constexpr (std::is_floating_point_v<Numeric>) { // avoid implicit conversion warnings
            return expected_t<boost::json::value>{std::in_place, static_cast<double>(value), memory_resource};
        } else {
            static_assert(sizeof(Numeric) <= sizeof(long long),
                          "Serializing integers larger than 'long long' is not supported");
            return expected_t<boost::json::value>{std::in_place, value, memory_resource};
        }
    }

    template<string_like String>
    static expected_t<boost::json::value> handle_string(Context & /*context*/,
                                                        const context_entry_s & /*entry*/,
                                                        const String &value,
                                                        const boost::json::storage_ptr &memory_resource) {
        const auto str = std::string_view{std::ranges::data(value), std::ranges::size(value)};
        return expected_t<boost::json::value>{std::in_place, str, memory_resource};
    }

    template<optional_like Optional>
    static expected_t<boost::json::value> handle_optional(Context &context,
                                                          const context_entry_s &entry,
                                                          const Optional &value,
                                                          const boost::json::storage_ptr &memory_resource) {
        if (value) {
            return try_serialize_value<typename optional_traits<Optional>::value_type>(
                    context,
                    {.id = entry.id, .is_nullable = true},
                    *value,
                    memory_resource);
        }
        return expected_t<boost::json::value>{std::in_place, nullptr, memory_resource};
    }

    template<typename T>
    static expected_t<boost::json::value> handle_variant(Context &context,
                                                         const context_entry_s &entry,
                                                         const T &value,
                                                         const boost::json::storage_ptr &memory_resource) {
        return std::visit(
                [&]<typename Alternative>(const Alternative &alternative) {
                    return try_serialize_value<Alternative>(context, entry, alternative, memory_resource);
                },
                value);
    }

    static expected_t<boost::json::value> handle_boost_json_value(Context &context,
                                                                  const context_entry_s &entry,
                                                                  const boost::json::value &value,
                                                                  const boost::json::storage_ptr &memory_resource);

    static expected_t<boost::json::value> handle_boost_json_object(Context &context,
                                                                   const context_entry_s &entry,
                                                                   const boost::json::object &value,
                                                                   const boost::json::storage_ptr &memory_resource);

    static expected_t<boost::json::value> handle_boost_json_array(Context &context,
                                                                  const context_entry_s &entry,
                                                                  const boost::json::array &value,
                                                                  const boost::json::storage_ptr &memory_resource);

    template<typename EIArray>
    static expected_t<boost::json::value> handle_enum_indexed_array(Context &context,
                                                                    const context_entry_s &entry,
                                                                    const EIArray &value,
                                                                    const boost::json::storage_ptr &memory_resource) {
        const auto guard = ContextGuard{context, entry};

        using traits_t = enum_indexed_array_traits<EIArray>;
        using value_t = traits_t::value_type;
        using key_t = traits_t::key_type;
        constexpr const auto &names = names_of_sorted_enumerators<key_t>;

        auto result = expected_t<boost::json::value>{std::in_place, boost::json::object_kind, memory_resource};
        auto &object = result->get_object();
        object.reserve(value.size());
        for (auto &&[name, element] : reflex::views::zip(names, value)) {
            if (auto exp = try_serialize_value<value_t>(context, {.id = name}, element, memory_resource)) {
                object.emplace(name, *std::move(exp));
            } else {
                result = std::unexpected{std::move(exp).error()};
                break;
            }
        }
        return result;
    }

    template<array_like Array>
    static expected_t<boost::json::value> handle_array(Context &context,
                                                       const context_entry_s &entry,
                                                       const Array &value,
                                                       const boost::json::storage_ptr &memory_resource) {
        const auto guard = ContextGuard{context, entry};

        using value_t = array_traits<Array>::value_type;
        constexpr auto extent = array_traits<Array>::extent;

        auto result = expected_t<boost::json::value>{std::in_place, boost::json::array_kind, memory_resource};
        auto &array = result->get_array();
        array.reserve(extent);
        for (const auto idx : std::views::iota(0uz, extent)) {
            if (auto exp = try_serialize_value<value_t>(context, {.id = idx}, value[idx], memory_resource)) {
                array.push_back(*std::move(exp));
            } else {
                result = std::unexpected{std::move(exp).error()};
                break;
            }
        }
        return result;
    }

    template<set_like Set>
    static expected_t<boost::json::value> handle_set(Context &context,
                                                     const context_entry_s &entry,
                                                     const Set &value,
                                                     const boost::json::storage_ptr &memory_resource) {
        return try_serialize_handler::handle_sequence(context, entry, value, memory_resource);
    }

    template<map_like Map>
    static expected_t<boost::json::value> handle_map(Context &context,
                                                     const context_entry_s &entry,
                                                     const Map &value,
                                                     const boost::json::storage_ptr &memory_resource) {
        const auto guard = ContextGuard{context, entry};

        using key_t = std::remove_cv_t<typename Map::key_type>;
        using mapped_t = std::remove_cv_t<typename Map::mapped_type>;

        auto result = expected_t<boost::json::value>{std::in_place, boost::json::object_kind, memory_resource};
        auto &object = result->get_object();
        if constexpr (std::ranges::sized_range<const Map &>) {
            object.reserve(std::ranges::size(value));
        }
        for (auto &&[key, v] : value) {
            const auto insert_kv = [&](const std::string_view key_str) {
                if (auto exp_v = try_serialize_value<mapped_t>(context, {.id = key_str}, v, memory_resource)) {
                    object.emplace(key_str, *std::move(exp_v));
                    return true;
                } else {
                    result = std::unexpected{std::move(exp_v).error()};
                    return false;
                }
            };
            const auto succeeded = [&]<typename K>(this const auto &dispatch_serialize_key, const K &k) {
                if constexpr (custom_type_with_to_string<K>) { // Avoid allocation and string copies when possible.
                    return insert_kv(std::string_view{custom_traits<K>::to_string(k)});
                } else if constexpr (custom_type<K>) {
                    if (auto exp_k = try_serialize_value(context, {}, k, memory_resource)) {
                        if (exp_k->is_string()) {
                            return insert_kv(exp_k->get_string());
                        } else {
                            const auto k_guard = ContextGuard{context, {}};
                            result = make_key_did_not_serialized_to_string(context, exp_k->kind());
                            return false;
                        }
                    } else {
                        result = std::unexpected{std::move(exp_k).error()};
                        return false;
                    }
                } else if constexpr (is_variant<K>) {
                    return std::visit([&](const auto &alt) { return dispatch_serialize_key(alt); }, k);
                } else if constexpr (string_like<K>) {
                    return insert_kv(std::string_view{std::ranges::data(k), std::ranges::size(k)});
                } else if constexpr (reflex::reflected_enum<K>) {
                    if (const auto name = serialized_enumerator_name(k)) {
                        return insert_kv(*name);
                    } else {
                        const auto k_guard = ContextGuard{context, {}};
                        result = make_unknown_enum_value(context, std::to_underlying(k));
                        return false;
                    }
                } else {
                    static_assert(false, "Unsupported type for map-like key");
                }
            }.template operator()<key_t>(key);
            if (!succeeded) {
                assert(!result.has_value());
                break;
            }
        }
        return result;
    }

    template<typename Sequence>
        requires sequence_container<Sequence> || set_like<Sequence> // handle_set calls handle_sequence
    static expected_t<boost::json::value> handle_sequence(Context &context,
                                                          const context_entry_s &entry,
                                                          const Sequence &value,
                                                          const boost::json::storage_ptr &memory_resource) {
        const auto guard = ContextGuard{context, entry};

        auto result = expected_t<boost::json::value>{std::in_place, boost::json::array_kind, memory_resource};
        auto &array = result->get_array();
        if constexpr (std::ranges::sized_range<const Sequence &>) {
            array.reserve(std::ranges::size(value));
        }
        for (auto &&[idx, elem] : ranges::views::enumerate(value)) {
            using value_t = std::remove_cv_t<std::ranges::range_value_t<Sequence>>;
            const auto uidx = static_cast<std::size_t>(idx);
            if (auto exp = try_serialize_value<value_t>(context, {.id = uidx}, elem, memory_resource)) {
                array.emplace_back(*std::move(exp));
            } else {
                result = std::unexpected{std::move(exp).error()};
                break;
            }
        }
        return result;
    }

    template<reflex::reflected_enum E>
    static expected_t<boost::json::value> handle_enum(Context &context,
                                                      const context_entry_s &entry,
                                                      const E value,
                                                      const boost::json::storage_ptr &memory_resource) {
        if (const auto name = serialized_enumerator_name(value)) {
            return expected_t<boost::json::value>{std::in_place, *name, memory_resource};
        } else {
            const auto guard = ContextGuard{context, entry};
            return make_unknown_enum_value(context, std::to_underlying(value));
        }
    }

    template<reflex::reflected_class Class>
    static expected_t<boost::json::value> handle_class(Context &context,
                                                       const context_entry_s &entry,
                                                       const Class &value,
                                                       const boost::json::storage_ptr &memory_resource) {
        const auto guard = ContextGuard{context, entry};

        using meta_t = reflex::meta<Class>;
        static constexpr auto &meta_members = reflex::meta<Class>::data_members;
        constexpr auto member_count = std::tuple_size_v<decltype(meta_t::data_members)>;

        auto result = expected_t<boost::json::value>{std::in_place, boost::json::object_kind, memory_resource};
        auto &object = result->get_object();
        object.reserve(member_count);
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            std::ignore = ([&] {
                if constexpr (is_ignored<std::tuple_element_t<I, std::remove_cvref_t<decltype(meta_members)>>>()) {
                    return true;
                } else {
                    static constexpr auto &meta_mem = reflex::get<I>(meta_members);
                    static constexpr auto name = serialized_name(meta_mem);
                    if (auto exp_mem = try_serialize_value(context,
                                                           {.id = name},
                                                           value.*(meta_mem.ptr),
                                                           memory_resource)) {
                        object.emplace(name, *std::move(exp_mem));
                        return true;
                    } else {
                        result = std::unexpected{std::move(exp_mem).error()};
                        return false;
                    }
                }
            }() && ...); // fold over && to short-circuit on failure
        }(std::make_index_sequence<member_count>{});
        return result;
    }
};

} // namespace detail

template<typename T>
expected_t<boost::json::value> try_serialize_value(Context &context,
                                                   const context_entry_s &entry,
                                                   const T &value,
                                                   const boost::json::storage_ptr &memory_resource) {
    return detail::handle_value<detail::try_serialize_handler, T>(context, entry, value, memory_resource);
}

template<typename T>
[[nodiscard]] boost::json::value serialize_value(Context &context,
                                                 const context_entry_s &entry,
                                                 const T &value,
                                                 const boost::json::storage_ptr &memory_resource) {
    auto exp = try_serialize_value(context, entry, value, memory_resource);
    if (!exp) {
        throw SerializationError{std::move(exp).error()};
    }
    return *std::move(exp);
}

template<typename T>
[[nodiscard]] boost::json::value serialize_config(const T &value, const boost::json::storage_ptr memory_resource) {
    auto context = Context{};
    return serialize_value(context, {}, value, memory_resource);
}

template<typename T>
[[nodiscard]] std::string serialize_config_to_string(const T &value,
                                                     const stringify_json_options_s &stringify_options) {
    auto memory_resource = boost::json::monotonic_resource{};
    return stringify_json(serialize_config(value, &memory_resource), stringify_options);
}

} // namespace reflex::config

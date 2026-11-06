// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

#include <boost/json/array.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/monotonic_resource.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/string.hpp>
#include <boost/json/value.hpp>

#include <reflex/meta.hpp>
#include <reflex/ranges/enumerate_view.hpp>
#include <reflex/ranges/zip_view.hpp>
#include <reflex/tuple.hpp>
#include <reflex/type_name.hpp>

#include <reflex/config/concepts.hpp>
#include <reflex/config/context.hpp>
#include <reflex/config/errors.hpp>
#include <reflex/config/fwd.hpp>
#include <reflex/config/reflection_helpers.hpp>
#include <reflex/config/traits.hpp>

namespace reflex::config {

namespace detail {

std::unexpected<ErrorState> make_wrong_kind(const Context &context,
                                            std::span<const boost::json::kind> expected,
                                            boost::json::kind actual);
std::unexpected<ErrorState> make_to_number_ec(const Context &context,
                                              const boost::json::value &value,
                                              std::error_code ec,
                                              std::string_view type_name);
std::unexpected<ErrorState> make_wrong_array_size(const Context &context, std::size_t expected, std::size_t actual);
std::unexpected<ErrorState> make_unknown_enumerator(const Context &context, std::string_view actual);
std::unexpected<ErrorState> make_missing_key(const Context &context, std::string_view name);
std::unexpected<ErrorState> make_unknown_key(const Context &context, std::string_view name);
std::unexpected<ErrorState> make_ignored_member(const Context &context, std::string_view name);
std::unexpected<ErrorState> make_no_alternatives_matched(const Context &context,
                                                         move_from_t,
                                                         std::span<ErrorState> nested_errors);

template<std::invocable Fn>
class InitWrapper {
public:
    constexpr explicit InitWrapper(Fn &&fn) noexcept(std::is_nothrow_move_constructible_v<Fn>) : m_fn{std::move(fn)} {}

    constexpr explicit(false) operator std::invoke_result_t<Fn>() && noexcept(std::is_nothrow_invocable_v<Fn>) {
        return std::invoke(std::move(m_fn));
    }

private:
    Fn m_fn;
};

template<typename Variant, typename JsonType, typename Deserializer>
expected_t<Variant> deserialize_variant_impl(DeserializationContext &context,
                                             const context_entry_s &entry,
                                             const JsonType &json,
                                             Deserializer deserializer) {
    constexpr auto alt_count = std::variant_size_v<Variant>;
    static_assert(alt_count != 0, "Variants with no alternatives are not supported");

    auto result = std::optional<Variant>{};
    auto alt_errors = std::array<ErrorState, alt_count>{};

    const auto do_deserialize = [&] {
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            std::ignore = ([&] {
                using alt_t = std::remove_cv_t<std::variant_alternative_t<I, Variant>>;
                if (auto ex_alt = deserializer.template operator()<alt_t>(context, entry, json)) {
                    result.emplace(std::in_place_index<I>, *std::move(ex_alt));
                    return true;
                } else {
                    alt_errors[I] = std::move(ex_alt).error();
                    return false;
                }
            }() || ...); // fold over || to short-circuit on success
        }(std::make_index_sequence<alt_count>{});
    };

    {
        const auto suppress_error_formatting = ErrorFormattingSuppressionGuard{context};
        do_deserialize();
    }
    if (!result.has_value()) {
        do_deserialize();
        assert(!result.has_value());

        const auto guard = ContextGuard{context, entry};
        return make_no_alternatives_matched(context, move_from, alt_errors);
    }
    return *std::move(result);
}

[[nodiscard]] std::optional<std::size_t> find_enumerator(std::span<const std::string_view> names,
                                                         std::string_view string) noexcept;

template<typename E>
expected_t<E> deserialize_enum(const DeserializationContext &context, const std::string_view string) {
    constexpr const auto &info = enum_info<E>;
    const auto enumerator_index = find_enumerator(info.names, string);
    if (!enumerator_index) {
        return make_unknown_enumerator(context, string);
    }
    return info.values[*enumerator_index];
}

template<typename Key>
expected_t<Key> deserialize_map_key(DeserializationContext &context, const std::string_view key) {
    static_assert(unqualified<Key>);
    if constexpr (custom_type_with_try_deserialize_string<Key>) {
        return custom_traits<Key>::try_deserialize_string(context, context_entry_s{}, key);
    } else if constexpr (custom_type<Key>) {
        const auto key_value = boost::json::value{key};
        return try_deserialize_value<Key>(context, context_entry_s{}, key_value);
    } else if constexpr (is_variant<Key>) {
        return deserialize_variant_impl<Key>(
                context,
                {},
                key,
                []<typename K>(DeserializationContext &c, const context_entry_s &, const std::string_view str) {
                    return deserialize_map_key<K>(c, str);
                });
    } else if constexpr (string_like<Key>) {
        static_cast<void>(context);
        return Key(key.begin(), key.end());
    } else if constexpr (reflex::reflected_enum<Key>) {
        const auto guard = ContextGuard{context, {}};
        return deserialize_enum<Key>(context, key);
    } else {
        static_assert(false, "Unsupported map-like key type");
    }
}

template<std::size_t MemberCount>
struct class_info {
    std::array<std::string_view, MemberCount> names;
    std::array<bool, MemberCount> are_required;
    std::array<bool, MemberCount> are_ignored;
};

expected_t<void> find_object_member_values(DeserializationContext &context,
                                           const boost::json::object &object,
                                           std::span<const std::string_view> names,
                                           std::span<const bool> are_required,
                                           std::span<const bool> are_ignored,
                                           std::span<const boost::json::value *> values);

template<typename T, T (*Initializer)(), bool Ignored>
expected_t<T> construct_class_member(DeserializationContext &context,
                                     const boost::json::value *const value,
                                     const context_entry_s &entry) {
    static_assert(unqualified<T>);
    if constexpr (Ignored) {
        if constexpr (Initializer == nullptr) {
            return expected_t<T>{std::in_place};
        } else {
            return expected_t<T>{std::in_place, InitWrapper{[&] -> T { return Initializer(); }}};
        }
    } else {
        if constexpr (Initializer == nullptr) {
            assert(value);
        }
        if (value) {
            return try_deserialize_value<T>(context, entry, *value);
        }
        // The extra constexpr-if is here to silence an MSVC warning about calling a null function pointer.
        if constexpr (Initializer != nullptr) {
            return expected_t<T>{std::in_place, InitWrapper{[&] -> T { return Initializer(); }}};
        } else {
            std::unreachable();
        }
    }
}

} // namespace detail

inline expected_t<void> check_kind(const Context &context,
                                   const boost::json::value &value,
                                   const boost::json::kind expected) {
    if (value.kind() != expected) {
        return detail::make_wrong_kind(context, {&expected, 1}, value.kind());
    }
    return {};
}

namespace detail {

struct try_deserialize_handler {
    template<custom_type Custom>
    static expected_t<Custom> handle_custom(DeserializationContext &context,
                                            const context_entry_s &entry,
                                            const boost::json::value &value) {
        static_assert(requires {
            { custom_traits<Custom>::try_deserialize(context, entry, value) } -> std::same_as<expected_t<Custom>>;
        });
        return custom_traits<Custom>::try_deserialize(context, entry, value);
    }

    static expected_t<std::monostate> handle_monostate(DeserializationContext &context,
                                                       const context_entry_s &entry,
                                                       const boost::json::value &value);

    static expected_t<bool> handle_bool(DeserializationContext &context,
                                        const context_entry_s &entry,
                                        const boost::json::value &value);

    template<numeric Numeric>
    static expected_t<Numeric> handle_numeric(DeserializationContext &context,
                                              const context_entry_s &entry,
                                              const boost::json::value &value) {
        if constexpr (std::is_integral_v<Numeric>) {
            // boost::json::value::to_number casts std::numeric_limits<T>::max() to std::uint64_t to check overflow.
            // Doing that for larger Ts would itself cause truncation.
            static_assert(sizeof(Numeric) <= sizeof(std::uint64_t),
                          "Deserializing integers larger than 64 bits is not supported");
        }

        const auto guard = ContextGuard{context, entry};

        auto ec = std::error_code{};
        auto number = value.to_number<Numeric>(ec);
        if (ec) {
            return make_to_number_ec(context, value, ec, reflex::type_name<Numeric>);
        }
        return std::move(number);
    }

    template<string_like String>
    static expected_t<String> handle_string(DeserializationContext &context,
                                            const context_entry_s &entry,
                                            const boost::json::value &value) {
        const auto guard = ContextGuard{context, entry};

        if (auto exp_kind = check_kind(context, value, boost::json::kind::string); !exp_kind) {
            return std::unexpected{std::move(exp_kind).error()};
        }

        const auto &string = value.get_string();
        return expected_t<String>{std::in_place, string.begin(), string.end()};
    }

    template<optional_like Optional>
    static expected_t<Optional> handle_optional(DeserializationContext &context,
                                                const context_entry_s &entry,
                                                const boost::json::value &value) {
        using traits_t = optional_traits<Optional>;
        using value_t = traits_t::value_type;

        if (value.is_null()) {
            return expected_t<Optional>{std::in_place};
        }

        if (auto exp_value = try_deserialize_value<value_t>(context, {.id = entry.id, .is_nullable = true}, value)) {
            return expected_t<Optional>{std::in_place, InitWrapper{[&] -> Optional {
                                            return traits_t::construct(*std::move(exp_value));
                                        }}};
        } else {
            return std::unexpected{std::move(exp_value).error()};
        }
    }

    template<typename Variant>
    static expected_t<Variant> handle_variant(DeserializationContext &context,
                                              const context_entry_s &entry,
                                              const boost::json::value &value) {
        return deserialize_variant_impl<Variant>(
                context,
                entry,
                value,
                []<typename A>(DeserializationContext &c, const context_entry_s &e, const boost::json::value &v) {
                    return try_deserialize_value<A>(c, e, v);
                });
    }

    static expected_t<boost::json::value> handle_boost_json_value(DeserializationContext &context,
                                                                  const context_entry_s &entry,
                                                                  const boost::json::value &value);

    static expected_t<boost::json::object> handle_boost_json_object(DeserializationContext &context,
                                                                    const context_entry_s &entry,
                                                                    const boost::json::value &value);

    static expected_t<boost::json::array> handle_boost_json_array(DeserializationContext &context,
                                                                  const context_entry_s &entry,
                                                                  const boost::json::value &value);

    template<typename EIArray>
    static expected_t<EIArray> handle_enum_indexed_array(DeserializationContext &context,
                                                         const context_entry_s &entry,
                                                         const boost::json::value &value) {
        const auto guard = ContextGuard{context, entry};

        using traits_t = enum_indexed_array_traits<EIArray>;
        using value_t = traits_t::value_type;
        using key_t = traits_t::key_type;

        constexpr const auto &names = names_of_sorted_enumerators<key_t>;
        using elem_values_t = std::array<const boost::json::value *, names.size()>;

        if (auto exp_kind = check_kind(context, value, boost::json::kind::object); !exp_kind) {
            return std::unexpected{std::move(exp_kind).error()};
        }

        elem_values_t elem_values;
        {
            const auto &object = value.get_object();
            std::array<bool, names.size()> are_required;
            are_required.fill(true);
            std::array<bool, names.size()> are_ignored{};
            if (auto exp_found =
                        find_object_member_values(context, object, names, are_required, are_ignored, elem_values);
                !exp_found) {
                return std::unexpected{std::move(exp_found).error()};
            }
        }

        auto array_of_optional = std::array<std::optional<value_t>, names.size()>{};
        for (auto &&[opt, name, elem_v] : reflex::views::zip(array_of_optional, names, elem_values)) {
            if (auto exp = try_deserialize_value<value_t>(context, {.id = name}, *elem_v)) {
                opt.emplace(*std::move(exp));
            } else {
                return std::unexpected{std::move(exp).error()};
            }
        }

        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return expected_t<EIArray>{std::in_place,
                                       InitWrapper{[&] -> EIArray { return {*std::move(array_of_optional[I])...}; }}};
        }(std::make_index_sequence<names.size()>{});
    }

    template<array_like Array>
    static expected_t<Array> handle_array(DeserializationContext &context,
                                          const context_entry_s &entry,
                                          const boost::json::value &value) {
        const auto guard = ContextGuard{context, entry};

        using value_t = array_traits<Array>::value_type;
        constexpr auto extent = std::size_t{array_traits<Array>::extent};

        if (auto exp_kind = check_kind(context, value, boost::json::kind::array); !exp_kind) {
            return std::unexpected{std::move(exp_kind).error()};
        }

        const auto &array = value.get_array();
        if (array.size() != extent) {
            return make_wrong_array_size(context, extent, array.size());
        }

        auto array_of_optional = std::array<std::optional<value_t>, extent>{};
        for (const auto i : std::views::iota(0uz, extent)) {
            if (auto exp = try_deserialize_value<value_t>(context, {.id = i}, array[i])) {
                array_of_optional[i].emplace(*std::move(exp));
            } else {
                return std::unexpected{std::move(exp).error()};
            }
        }

        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return expected_t<Array>{std::in_place,
                                     InitWrapper{[&] -> Array { return Array{*std::move(array_of_optional[I])...}; }}};
        }(std::make_index_sequence<extent>{});
    }

    template<set_like Set>
    static expected_t<Set> handle_set(DeserializationContext &context,
                                      const context_entry_s &entry,
                                      const boost::json::value &value) {
        const auto guard = ContextGuard{context, entry};

        using key_t = std::remove_cv_t<typename Set::key_type>;

        if (auto exp_kind = check_kind(context, value, boost::json::kind::array); !exp_kind) {
            return std::unexpected{std::move(exp_kind).error()};
        }

        const auto &array = value.get_array();
        auto result = expected_t<Set>{std::in_place}; // Container is default-constructible
        if constexpr (requires(Set &set) { set.reserve(std::size_t{}); }) {
            result->reserve(array.size()); // UnorderedAssociativeContainer may provide reserve()
        }
        for (const auto &[idx, elem_v] : array | reflex::views::enumerate) {
            if (auto exp = try_deserialize_value<key_t>(context, {.id = static_cast<std::size_t>(idx)}, elem_v)) {
                // Since we already have a key_t constructed, there is no difference between 'emplace' and 'insert'.
                result->insert(*std::move(exp)); // should work for any set-like [Unordered]AssociativeContainer
            } else {
                result = std::unexpected{std::move(exp).error()};
                break;
            }
        }
        return result;
    }

    template<map_like Map>
    static expected_t<Map> handle_map(DeserializationContext &context,
                                      const context_entry_s &entry,
                                      const boost::json::value &value) {
        const auto guard = ContextGuard{context, entry};

        using key_t = std::remove_cv_t<typename Map::key_type>;
        using mapped_t = std::remove_cv_t<typename Map::mapped_type>;

        if (auto exp_kind = check_kind(context, value, boost::json::kind::object); !exp_kind) {
            return std::unexpected{std::move(exp_kind).error()};
        }

        const auto &object = value.get_object();
        auto result = expected_t<Map>{std::in_place}; // Container is default-constructible
        if constexpr (requires(Map &map, std::size_t cap) { map.reserve(cap); }) {
            result->reserve(object.size()); // UnorderedAssociativeContainer may provide reserve()
        }
        for (const auto &key_value : object) {
            auto exp_key = deserialize_map_key<key_t>(context, key_value.key());
            if (!exp_key) {
                result = std::unexpected{std::move(exp_key).error()};
                break;
            }
            auto exp_mapped = try_deserialize_value<mapped_t>(context, {.id = key_value.key()}, key_value.value());
            if (!exp_mapped) {
                result = std::unexpected{std::move(exp_mapped).error()};
                break;
            }
            if constexpr (requires(Map &map, key_t &&k, mapped_t &&m) {
                              map.try_emplace(std::move(k), std::move(m));
                          }) {
                // 'try_emplace' is the best option for std::flat_map or similar, where Key and Mapped are stored
                // separately. It can also avoid constructing a std::pair when the key already exists.
                result->try_emplace(*std::move(exp_key), *std::move(exp_mapped));
            } else if constexpr (requires(Map &map, key_t &&k, mapped_t &&m) {
                                     map.emplace(std::move(k), std::move(m));
                                 }) {
                // Even if 'try_emplace' isn't available, 'emplace' can still avoid creating a temporary std::pair and
                // moving it for some containers.
                result->emplace(*std::move(exp_key), *std::move(exp_mapped));
            } else {
                // Fall back to insert(value_type(...)) if all else fails. This will probably only happen for really old
                // (pre C++11) containers.
                result->insert(typename Map::value_type(*std::move(exp_key), *std::move(exp_mapped)));
            }
        }
        return result;
    }

    template<sequence_container Sequence>
    static expected_t<Sequence> handle_sequence(DeserializationContext &context,
                                                const context_entry_s &entry,
                                                const boost::json::value &value) {
        const auto guard = ContextGuard{context, entry};

        if (auto exp_kind = check_kind(context, value, boost::json::kind::array); !exp_kind) {
            return std::unexpected{std::move(exp_kind).error()};
        }

        const auto &array = value.get_array();
        auto result = expected_t<Sequence>{std::in_place}; // Container is default-constructible
        if constexpr (requires(Sequence &seq) { seq.reserve(std::size_t{}); }) {
            result->reserve(array.size());
        }
        for (const auto &[idx, elem_v] : array | reflex::views::enumerate) {
            using value_t = std::remove_cv_t<typename Sequence::value_type>;
            if (auto exp = try_deserialize_value<value_t>(context, {.id = static_cast<std::size_t>(idx)}, elem_v)) {
                // Since we already have a value_t constructed, there is no difference between 'emplace_back' and
                // 'push_back'.
                result->push_back(*std::move(exp)); // 'push_back' is what std::back_insert_iterator uses
            } else {
                result = std::unexpected{std::move(exp).error()};
                break;
            }
        }
        return result;
    }

    template<reflex::reflected_enum E>
    static expected_t<E> handle_enum(DeserializationContext &context,
                                     const context_entry_s &entry,
                                     const boost::json::value &value) {
        const auto guard = ContextGuard{context, entry};

        if (auto exp_kind = check_kind(context, value, boost::json::kind::string); !exp_kind) {
            return std::unexpected{std::move(exp_kind).error()};
        }

        const auto &string = value.get_string();
        return deserialize_enum<E>(context, string);
    }

    template<reflex::reflected_class Class>
    static expected_t<Class> handle_class(DeserializationContext &context,
                                          const context_entry_s &entry,
                                          const boost::json::value &value) {
        const auto guard = ContextGuard{context, entry};

        static_assert(std::is_aggregate_v<Class>, "Deserialization is only supported for aggregate classes");
        using meta_t = reflex::meta<Class>;
        constexpr auto member_count = std::tuple_size_v<decltype(meta_t::data_members)>;
        static constexpr auto info = reflex::apply(
                []<typename... MetaMember>(const MetaMember &...data_member) {
                    return class_info<sizeof...(data_member)>{{serialized_name(data_member)...},
                                                              {data_member.initializer == nullptr...},
                                                              {is_ignored<MetaMember>()...}};
                },
                meta_t::data_members);
        using mem_values_t = std::array<const boost::json::value *, member_count>;
        using optional_mem_tuple_t = decltype([]<std::size_t... I>(std::index_sequence<I...>) {
            return std::type_identity<reflex::tuple<std::optional<
                    std::remove_cv_t<typename std::tuple_element_t<I, decltype(meta_t::data_members)>::type>>...>>{};
        }(std::make_index_sequence<member_count>{}))::type;

        if (auto exp_kind = check_kind(context, value, boost::json::kind::object); !exp_kind) {
            return std::unexpected{std::move(exp_kind).error()};
        }

        const auto &object = value.get_object();
        mem_values_t mem_values;
        if (auto exp_found = find_object_member_values(context,
                                                       object,
                                                       info.names,
                                                       info.are_required,
                                                       info.are_ignored,
                                                       mem_values);
            !exp_found) {
            return std::unexpected{std::move(exp_found).error()};
        }

        auto optional_members = optional_mem_tuple_t{};
        auto error = ErrorState{};
        const auto succeeded = [&]<std::size_t... I>(std::index_sequence<I...>) {
            return ([&] {
                using member_t = std::tuple_element_t<I, decltype(meta_t::data_members)>::type;
                auto exp =
                        construct_class_member<std::remove_cv_t<member_t>,
                                               reflex::get<I>(meta_t::data_members).initializer,
                                               is_ignored<std::tuple_element_t<I, decltype(meta_t::data_members)>>()>(
                                context,
                                mem_values[I],
                                {.id = info.names[I]});
                if (exp) {
                    reflex::get<I>(optional_members).emplace(*std::move(exp));
                    return true;
                } else {
                    error = std::move(exp).error();
                    return false;
                }
            }() && ...); // fold over && to short-circuit on failure
        }(std::make_index_sequence<member_count>{});
        if (succeeded) {
            return [&]<std::size_t... I>(std::index_sequence<I...>) {
                // Using InitWrapper because parenthesised aggregate initialization doesn't provide brace elision for
                // base classes.
                return expected_t<Class>{std::in_place, InitWrapper{[&] -> Class {
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-braces" // suggest braces around initialization of subobject (base)
#endif
                                             return Class{*reflex::get<I>(std::move(optional_members))...};
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
                                         }}};
            }(std::make_index_sequence<member_count>{});
        } else {
            return std::unexpected{std::move(error)};
        }
    }
};

} // namespace detail

template<typename T>
expected_t<T> try_deserialize_value(DeserializationContext &context,
                                    const context_entry_s &entry,
                                    const boost::json::value &value) {
    static_assert(unqualified<T>, "The type for deserialization cannot be cv-qualified");
    return detail::handle_value<detail::try_deserialize_handler, T>(context, entry, value);
}

template<typename T>
[[nodiscard]] T deserialize_value(DeserializationContext &context,
                                  const context_entry_s &entry,
                                  const boost::json::value &value) {
    auto result = try_deserialize_value<T>(context, entry, value);
    if (result) {
        return *std::move(result);
    }
    throw DeserializationError{std::move(result).error()};
}

template<typename T>
[[nodiscard]] T deserialize_config(const boost::json::value &value, deserialization_options_s options) {
    auto context = DeserializationContext{std::move(options)};
    return deserialize_value<T>(context, {}, value);
}

template<typename T>
[[nodiscard]] T deserialize_config_from_string(const std::string_view string,
                                               const boost::json::parse_options &parse_options,
                                               deserialization_options_s options) {
    auto memory_resource = boost::json::monotonic_resource{};
    const auto json = boost::json::parse(string, &memory_resource, parse_options);
    return deserialize_config<T>(json, std::move(options));
}

// parse_options to use for human editable JSON (comments and trailing commas) with precise number parsing
inline constexpr boost::json::parse_options extended_precise_parse_options = {
        .numbers = boost::json::number_precision::precise,
        .allow_comments = true,
        .allow_trailing_commas = true};

extern template expected_t<std::monostate> try_deserialize_value<std::monostate>(DeserializationContext &context,
                                                                                 const context_entry_s &entry,
                                                                                 const boost::json::value &value);

extern template expected_t<bool> try_deserialize_value<bool>(DeserializationContext &context,
                                                             const context_entry_s &entry,
                                                             const boost::json::value &value);

extern template expected_t<std::int8_t> try_deserialize_value<std::int8_t>(DeserializationContext &context,
                                                                           const context_entry_s &entry,
                                                                           const boost::json::value &value);
extern template expected_t<std::int16_t> try_deserialize_value<std::int16_t>(DeserializationContext &context,
                                                                             const context_entry_s &entry,
                                                                             const boost::json::value &value);
extern template expected_t<std::int32_t> try_deserialize_value<std::int32_t>(DeserializationContext &context,
                                                                             const context_entry_s &entry,
                                                                             const boost::json::value &value);
extern template expected_t<std::int64_t> try_deserialize_value<std::int64_t>(DeserializationContext &context,
                                                                             const context_entry_s &entry,
                                                                             const boost::json::value &value);
extern template expected_t<std::uint8_t> try_deserialize_value<std::uint8_t>(DeserializationContext &context,
                                                                             const context_entry_s &entry,
                                                                             const boost::json::value &value);
extern template expected_t<std::uint16_t> try_deserialize_value<std::uint16_t>(DeserializationContext &context,
                                                                               const context_entry_s &entry,
                                                                               const boost::json::value &value);
extern template expected_t<std::uint32_t> try_deserialize_value<std::uint32_t>(DeserializationContext &context,
                                                                               const context_entry_s &entry,
                                                                               const boost::json::value &value);
extern template expected_t<std::uint64_t> try_deserialize_value<std::uint64_t>(DeserializationContext &context,
                                                                               const context_entry_s &entry,
                                                                               const boost::json::value &value);
extern template expected_t<float> try_deserialize_value<float>(DeserializationContext &context,
                                                               const context_entry_s &entry,
                                                               const boost::json::value &value);
extern template expected_t<double> try_deserialize_value<double>(DeserializationContext &context,
                                                                 const context_entry_s &entry,
                                                                 const boost::json::value &value);

extern template expected_t<std::string> try_deserialize_value<std::string>(DeserializationContext &context,
                                                                           const context_entry_s &entry,
                                                                           const boost::json::value &value);

} // namespace reflex::config
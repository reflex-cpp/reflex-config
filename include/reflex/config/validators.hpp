// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cassert>
#include <concepts>
#include <functional>
#include <optional>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include <boost/json/fwd.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/value.hpp>

#include <reflex/detail/config.hpp>
#include <reflex/ranges/enumerate_view.hpp>

#include <reflex/config/concepts.hpp>
#include <reflex/config/context.hpp>
#include <reflex/config/errors.hpp>
#include <reflex/config/format_error_state.hpp>
#include <reflex/config/reflection_helpers.hpp>

namespace reflex::config {

class Context;

class validator_base {};

namespace detail {

template<custom_type T>
std::optional<boost::json::string> custom_type_to_string(Context &context, const T &value) {
    static_assert(requires {
        {
            custom_traits<T>::try_serialize(context, context_entry_s{}, value, boost::json::storage_ptr{})
        } -> std::same_as<expected_t<boost::json::value>>;
    });
    if (auto exp = custom_traits<T>::try_serialize(context, context_entry_s{}, value, boost::json::storage_ptr{});
        exp.has_value() && exp->is_string()) {
        return std::move(*exp).get_string();
    } else {
        return std::nullopt;
    }
}

} // namespace detail

template<typename Validator>
class check_values : public validator_base {
public:
    constexpr explicit check_values(Validator validator) noexcept : m_validator{std::move(validator)} {}

    template<typename T>
    expected_t<void> validate(Context &context, const T &actual) const {
        if constexpr (custom_type<T> || std::same_as<T, std::monostate> || std::same_as<T, bool> || numeric<T> ||
                      string_like<T> || reflex::reflected_enum<T>) {
            // These types can't be decomposed (they could be potentially decomposable, if customization defined object
            // graph visitation).
            return m_validator.validate(context, actual);
        } else if constexpr (optional_like<T>) {
            if (actual) {
                return m_validator.validate(context, *actual);
            } else {
                return {};
            }
        } else if constexpr (is_variant<T>) {
            return std::visit([&](const auto &value) { return m_validator.validate(context, value); }, actual);
        } else if constexpr (std::same_as<T, boost::json::value>) {
            // Treat boost::json::value as a variant. null is treated as "empty".
            switch (actual.kind()) {
                case boost::json::kind::null: return {};
                case boost::json::kind::bool_: return m_validator.validate(context, actual.get_bool());
                case boost::json::kind::int64: return m_validator.validate(context, actual.get_int64());
                case boost::json::kind::uint64: return m_validator.validate(context, actual.get_uint64());
                case boost::json::kind::double_: return m_validator.validate(context, actual.get_double());
                case boost::json::kind::string: return m_validator.validate(context, actual.get_string());
                case boost::json::kind::array: return m_validator.validate(context, actual.get_array());
                case boost::json::kind::object: return m_validator.validate(context, actual.get_object());
            }
        } else if constexpr (std::same_as<T, boost::json::object>) {
            for (const auto &entry : actual) {
                const auto g = ContextGuard{context, {.id = entry.key()}};
                if (auto exp = m_validator.validate(context, entry.value()); !exp) {
                    return exp;
                }
            }
            return {};
        } else if constexpr (is_enum_indexed_array_v<T>) {
            for (const auto [e, value] : actual.enumerate()) {
                const auto g = ContextGuard{context, {.id = serialized_enumerator_name(e).value_or("<unknown>")}};
                if (auto exp = m_validator.validate(context, value); !exp) {
                    return exp;
                }
            }
            return {};
        } else if constexpr (std::same_as<T, boost::json::array> || array_like<T> || sequence_container<T> ||
                             set_like<T>) {
            for (const auto [index, value] : reflex::views::enumerate(actual)) {
                const auto g = ContextGuard{context, {.id = static_cast<std::size_t>(index)}};
                if (auto exp = m_validator.validate(context, value); !exp) {
                    return exp;
                }
            }
            return {};
        } else if constexpr (map_like<T>) {
            for (const auto &[key, value] : actual) {
                const auto do_validate = [&](const std::string_view key_str) {
                    const auto g = ContextGuard{context, {.id = key_str}};
                    return m_validator.validate(context, value);
                };
                auto exp = [&context, &do_validate]<typename K>(this const auto &dispatch_do_validate, const K &k) {
                    if constexpr (custom_type_with_to_string<K>) {
                        return do_validate(std::string_view{custom_traits<K>::to_string(k)});
                    } else if constexpr (custom_type<K>) {
                        const auto exp_str = detail::custom_type_to_string(context, k);
                        return do_validate(exp_str ? std::string_view{*exp_str} : "<unknown>");
                    } else if constexpr (is_variant<K>) {
                        return std::visit([&](const auto &k_alt) { return dispatch_do_validate(k_alt); }, k);
                    } else if constexpr (string_like<K>) {
                        return do_validate({std::ranges::data(k), std::ranges::size(k)});
                    } else if constexpr (reflex::reflected_enum<K>) {
                        return do_validate(serialized_enumerator_name(k).value_or("<unknown>"));
                    } else {
                        static_assert(false, "Unsupported map-like key type");
                    }
                }(key);
                if (!exp) {
                    return exp;
                }
            }
            return {};
        } else if constexpr (reflex::reflected_class<T>) {
            using meta_t = reflex::meta<T>;
            static constexpr auto &meta_members = meta_t::data_members;
            constexpr auto member_count = std::tuple_size_v<decltype(meta_t::data_members)>;
            auto result = expected_t<void>{};
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                std::ignore = ([&] {
                    if constexpr (is_ignored<std::tuple_element_t<I, decltype(meta_t::data_members)>>()) {
                        return true;
                    } else {
                        static constexpr auto &meta_mem = reflex::get<I>(meta_members);
                        static constexpr auto mem_name = serialized_name(meta_mem);
                        const auto g = ContextGuard{context, {.id = mem_name}};
                        result = m_validator.validate(context, actual.*meta_mem.ptr);
                        return result.has_value();
                    }
                }() && ...); // fold over && to short-circuit on failure
            }(std::make_index_sequence<member_count>{});
            return result;
        } else {
            static_assert(false, "Unhandled type category");
        }
    }

private:
    Validator m_validator;
};

// The validation function must return config::format_validation_error(context, format_string, args...) on error.
template<typename ValidateFn>
class custom_check : public validator_base {
public:
    constexpr custom_check(ValidateFn &&validate_fn) noexcept(std::is_nothrow_move_constructible_v<ValidateFn>)
        : m_validate_fn{std::move(validate_fn)} {}

    template<typename T>
    expected_t<void> validate(const Context &context, const T &actual) const {
        static_assert(std::invocable<const ValidateFn &, const Context &, const T &>);
        static_assert(
                std::same_as<std::invoke_result_t<const ValidateFn &, const Context &, const T &>, expected_t<void>>);
        return std::invoke(m_validate_fn, context, actual);
    }

private:
    REFLEX_NO_UNIQUE_ADDRESS ValidateFn m_validate_fn;
};

// The validation function must return false on error.
template<typename ValidateFn>
class check_that : public validator_base {
public:
    constexpr check_that(ValidateFn validate_fn,
                         const std::string_view message) noexcept(std::is_nothrow_move_constructible_v<ValidateFn>)
        : m_validate_fn{std::move(validate_fn)},
          m_message{message} {}

    template<typename T>
    expected_t<void> validate(const Context &context, const T &actual) const {
        static_assert(std::invocable<const ValidateFn &, const T &>);
        static_assert(std::same_as<std::invoke_result_t<const ValidateFn &, const T &>, bool>);
        if (!std::invoke(m_validate_fn, actual)) {
            return format_validation_error(context, "{}", m_message);
        }
        return {};
    }

private:
    REFLEX_NO_UNIQUE_ADDRESS ValidateFn m_validate_fn;
    std::string_view m_message;
};

#ifdef __clang__ // clang-format off
    #define REFLEX_CONFIG_DIAG_PUSH_DISABLE_DOUBLE_PROMOTION                                                           \
        _Pragma("clang diagnostic push")                                                                               \
        _Pragma("clang diagnostic ignored \"-Wdouble-promotion\"") // implicit conversion increases floating-point precision
    #define REFLEX_CONFIG_DIAG_POP _Pragma("clang diagnostic pop")
#else // clang-format on
    #define REFLEX_CONFIG_DIAG_PUSH_DISABLE_DOUBLE_PROMOTION
    #define REFLEX_CONFIG_DIAG_POP
#endif

template<numeric T>
class min : public validator_base {
public:
    constexpr explicit min(const T value) noexcept : m_value{value} {}

    template<typename Number>
    expected_t<void> validate(const Context &context, const Number &actual) const {
        static_assert(same_numeric_category_as<Number, T>,
                      "Number must have the same numeric category as the provided value");
        REFLEX_CONFIG_DIAG_PUSH_DISABLE_DOUBLE_PROMOTION
        if (actual < m_value) {
            REFLEX_CONFIG_DIAG_POP
            return format_validation_error(context, "min {}", m_value);
        }
        return {};
    }

private:
    T m_value;
};

template<numeric T>
class max : public validator_base {
public:
    constexpr explicit max(const T value) noexcept : m_value{value} {}

    template<typename Number>
    expected_t<void> validate(const Context &context, const Number &actual) const {
        static_assert(same_numeric_category_as<Number, T>,
                      "Number must have the same numeric category as the provided value");
        REFLEX_CONFIG_DIAG_PUSH_DISABLE_DOUBLE_PROMOTION
        if (actual > m_value) {
            REFLEX_CONFIG_DIAG_POP
            return format_validation_error(context, "max {}", m_value);
        }
        return {};
    }

private:
    T m_value;
};

template<numeric T>
class less_than : public validator_base {
public:
    constexpr explicit less_than(const T value) noexcept : m_value{value} {}

    template<typename Number>
    expected_t<void> validate(const Context &context, const Number &actual) const {
        static_assert(same_numeric_category_as<Number, T>,
                      "Number must have the same numeric category as the provided value");
        REFLEX_CONFIG_DIAG_PUSH_DISABLE_DOUBLE_PROMOTION
        if (actual >= m_value) {
            REFLEX_CONFIG_DIAG_POP
            return format_validation_error(context, "less than {}", m_value);
        }
        return {};
    }

private:
    T m_value;
};

template<numeric T>
class greater_than : public validator_base {
public:
    constexpr explicit greater_than(const T value) noexcept : m_value{value} {}

    template<typename Number>
    expected_t<void> validate(const Context &context, const Number &actual) const {
        static_assert(same_numeric_category_as<Number, T>,
                      "Number must have the same numeric category as the provided value");
        REFLEX_CONFIG_DIAG_PUSH_DISABLE_DOUBLE_PROMOTION
        if (actual <= m_value) {
            REFLEX_CONFIG_DIAG_POP
            return format_validation_error(context, "greater than {}", m_value);
        }
        return {};
    }

private:
    T m_value;
};

template<numeric T>
class in_closed_range : public validator_base {
public:
    constexpr explicit in_closed_range(const T min_value, const T max_value) noexcept
        : m_min_value{min_value},
          m_max_value{max_value} {}

    template<typename Number>
    expected_t<void> validate(const Context &context, const Number &actual) const {
        static_assert(same_numeric_category_as<Number, T>,
                      "Number must have the same numeric category as the provided value");
        REFLEX_CONFIG_DIAG_PUSH_DISABLE_DOUBLE_PROMOTION
        if (actual < m_min_value || actual > m_max_value) {
            REFLEX_CONFIG_DIAG_POP
            return format_validation_error(context, "in range [{}, {}]", m_min_value, m_max_value);
        }
        return {};
    }

private:
    T m_min_value;
    T m_max_value;
};

#undef REFLEX_CONFIG_DIAG_POP
#undef REFLEX_CONFIG_DIAG_PUSH_DISABLE_DOUBLE_PROMOTION

class power_of_two : public validator_base {
public:
    constexpr explicit power_of_two() noexcept = default;

    template<std::integral Number>
    expected_t<void> validate(const Context &context, const Number &value) const {
        if (!is_power_of_2(value)) {
            return format_validation_error(context, "power of two");
        }
        return {};
    }

private:
    static constexpr bool is_power_of_2(const std::integral auto value) noexcept {
        return value > 0 && !(value & (value - 1));
    }
};

class ip_v4_address : public validator_base {
public:
    constexpr explicit ip_v4_address() noexcept = default;

    expected_t<void> validate(const Context &context, std::string_view value) const;
};

} // namespace reflex::config

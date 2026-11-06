// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <expected>
#include <string>

#include <catch2/catch_tostring.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <reflex/config/errors.hpp>

template<>
struct Catch::StringMaker<reflex::config::ErrorState> {
    static std::string convert(const reflex::config::ErrorState &err) {
        return fmt::format("ErrorState{{.message = \"{}\", .nested = [{}]}}",
                           err.message(),
                           fmt::join(err.nested() | std::views::transform(convert), ", "));
    }
};

template<typename T, typename E>
struct Catch::StringMaker<std::expected<T, E>> {
    static std::string convert(const std::expected<T, E> &ex) {
        if (ex.has_value()) {
            if constexpr (std::is_void_v<T>) {
                return "value {void}";
            } else {
                return fmt::format("value {{{}}}", StringMaker<T>::convert(*ex));
            }
        } else {
            return fmt::format("error {{{}}}", StringMaker<E>::convert(ex.error()));
        }
    }
};

namespace reflex::config {

namespace strings {

inline constexpr auto serialization_error = "serialization error";
inline constexpr auto deserialization_error = "deserialization error";
inline constexpr auto validation_error = "validation error";

inline constexpr auto wrong_value_kind = "wrong value kind";
inline constexpr auto no_exact_conversion = "no exact conversion";
inline constexpr auto wrong_array_size = "wrong array size";
inline constexpr auto unknown_enumerator = "unknown enumerator";
inline constexpr auto missing_key = "missing key";
inline constexpr auto unknown_key = "unknown key";
inline constexpr auto ignored_member = "ignored member";
inline constexpr auto no_alternatives_matched = "no alternatives matched";
inline constexpr auto wrong_fixed_value = "wrong fixed value";

inline constexpr auto key_did_not_serialize_to_string = "key did not serialize to string";
inline constexpr auto unknown_enumeration_value = "unknown enumeration value";

} // namespace strings

class ExpectedErrorContains final : public Catch::Matchers::MatcherGenericBase {
public:
    explicit ExpectedErrorContains(std::string_view substr) : m_substr{substr} {}

    template<typename T>
    bool match(const expected_t<T> &arg) const noexcept {
        return !arg.has_value() && arg.error().message().contains(m_substr);
    }

private:
    std::string describe() const override {
        return fmt::format("expected_t holds an error which contains \"{}\"", m_substr);
    }

    std::string m_substr;
};

} // namespace reflex::config

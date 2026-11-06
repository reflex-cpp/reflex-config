// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

#include <reflex/config/fwd.hpp>

namespace reflex::config {

struct move_from_t {
    explicit constexpr move_from_t() = default;
};

inline constexpr move_from_t move_from{};

namespace detail {

class RefCountedString {
    struct header_s {
        std::atomic<std::size_t> ref_count{1};
        std::size_t size;
    };

public:
    RefCountedString() = default;

    explicit RefCountedString(std::string_view str);

    RefCountedString(const RefCountedString &other) noexcept : m_chars{other.m_chars} {
        if (auto *const header = get_header()) {
            header->ref_count.fetch_add(1, std::memory_order::relaxed);
        }
    }

    RefCountedString(RefCountedString &&other) noexcept : m_chars{std::exchange(other.m_chars, nullptr)} {}

    ~RefCountedString();

    RefCountedString &operator=(const RefCountedString &other) noexcept {
        RefCountedString(other).swap(*this);
        return *this;
    }

    RefCountedString &operator=(RefCountedString &&other) noexcept {
        RefCountedString(std::move(other)).swap(*this);
        return *this;
    }

    void swap(RefCountedString &other) noexcept { std::swap(m_chars, other.m_chars); }

    friend void swap(RefCountedString &lhs, RefCountedString &rhs) noexcept { lhs.swap(rhs); }

    [[nodiscard]] std::size_t size() const noexcept {
        if (const auto *const header = get_header()) {
            return header->size;
        }
        return 0;
    }

    [[nodiscard]] const char *data() const noexcept { return m_chars; }

    [[nodiscard]] const char *c_str() const noexcept { return m_chars ? m_chars : ""; }

    [[nodiscard]] explicit(false) operator std::string_view() const noexcept {
        return std::string_view{data(), size()};
    }

private:
    char *m_chars = nullptr;

    [[nodiscard]] header_s *get_header() const noexcept {
        return m_chars ? std::launder(reinterpret_cast<header_s *>(m_chars - sizeof(header_s))) : nullptr;
    }
};

class NestedErrorStates {
    struct header_s {
        std::atomic<std::size_t> ref_count{1};
        std::size_t size;
    };

public:
    NestedErrorStates() = default;

    explicit NestedErrorStates(move_from_t, std::span<ErrorState> states);

    NestedErrorStates(const NestedErrorStates &other) noexcept : m_states_raw{other.m_states_raw} {
        if (auto *const header = get_header()) {
            header->ref_count.fetch_add(1, std::memory_order::relaxed);
        }
    }

    NestedErrorStates(NestedErrorStates &&other) noexcept : m_states_raw{std::exchange(other.m_states_raw, nullptr)} {}

    ~NestedErrorStates();

    NestedErrorStates &operator=(const NestedErrorStates &other) noexcept {
        NestedErrorStates(other).swap(*this);
        return *this;
    }

    NestedErrorStates &operator=(NestedErrorStates &&other) noexcept {
        NestedErrorStates(std::move(other)).swap(*this);
        return *this;
    }

    void swap(NestedErrorStates &other) noexcept { std::swap(m_states_raw, other.m_states_raw); }

    friend void swap(NestedErrorStates &lhs, NestedErrorStates &rhs) noexcept { lhs.swap(rhs); }

    [[nodiscard]] std::span<const ErrorState> states() const noexcept;

private:
    std::byte *m_states_raw = nullptr;

    [[nodiscard]] header_s *get_header() const noexcept {
        return m_states_raw ? std::launder(reinterpret_cast<header_s *>(m_states_raw - sizeof(header_s))) : nullptr;
    }
};

} // namespace detail

class [[nodiscard]] ErrorState {
public:
    ErrorState() = default;

    explicit ErrorState(std::string_view message);

    explicit ErrorState(std::string_view message, move_from_t move_from, std::span<ErrorState> states);

    [[nodiscard]] std::string_view message() const noexcept { return m_message; }
    [[nodiscard]] const char *what() const noexcept { return m_message.c_str(); }

    [[nodiscard]] std::span<const ErrorState> nested() const noexcept { return m_nested.states(); }

private:
    detail::RefCountedString m_message{};
    detail::NestedErrorStates m_nested{};
};

inline std::span<const ErrorState> detail::NestedErrorStates::states() const noexcept {
    if (const auto *const header = get_header()) {
        return {std::launder(reinterpret_cast<const ErrorState *>(m_states_raw)), header->size};
    }
    return {};
}

class Error : public std::exception {
public:
    explicit Error(ErrorState state) noexcept;

    [[nodiscard]] const char *what() const noexcept override { return m_state.what(); }

    [[nodiscard]] const ErrorState &state() const noexcept { return m_state; }

private:
    ErrorState m_state;
};

class DeserializationError : public Error {
public:
    using Error::Error;
};

class SerializationError : public Error {
public:
    using Error::Error;
};

class ValidationError : public Error {
public:
    using Error::Error;
};

} // namespace reflex::config

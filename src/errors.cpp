// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <reflex/config/errors.hpp>

#include <cstring>
#include <memory>
#include <new>
#include <type_traits>

namespace reflex::config {

namespace detail {

RefCountedString::RefCountedString(const std::string_view str) {
    if (!str.empty()) {
        const auto total_bytes = sizeof(header_s) + str.size() + 1;
        static_assert(alignof(header_s) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
        auto *const memory = static_cast<char *>(::operator new(total_bytes));
        m_chars = memory + sizeof(header_s);
        std::memcpy(m_chars, str.data(), str.size());
        m_chars[str.size()] = '\0';
        ::new (memory) header_s{.size = str.size()};
    }
}

RefCountedString::~RefCountedString() {
    if (auto *const header = get_header()) {
        if (header->ref_count.fetch_sub(1, std::memory_order::acq_rel) == 1) {
            const auto total_bytes = sizeof(header_s) + header->size + 1;
            static_assert(std::is_trivially_destructible_v<header_s>);
            ::operator delete(m_chars - sizeof(header_s), total_bytes);
        }
    }
}

NestedErrorStates::NestedErrorStates(move_from_t, const std::span<ErrorState> states) {
    if (!states.empty()) {
        static_assert(alignof(header_s) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
        static_assert(alignof(header_s) == alignof(ErrorState));
        const auto total_bytes = sizeof(header_s) + sizeof(ErrorState) * states.size();
        auto *const memory = static_cast<std::byte *>(::operator new(total_bytes));
        m_states_raw = memory + sizeof(header_s);
        static_assert(std::is_nothrow_move_constructible_v<ErrorState>);
        std::ranges::uninitialized_move(states.begin(),
                                        states.end(),
                                        reinterpret_cast<ErrorState *>(m_states_raw),
                                        std::unreachable_sentinel);
        ::new (memory) header_s{.size = states.size()};
    }
}

NestedErrorStates::~NestedErrorStates() {
    if (auto *const header = get_header()) {
        if (header->ref_count.fetch_sub(1, std::memory_order::acq_rel) == 1) {
            std::ranges::destroy_n(std::launder(reinterpret_cast<ErrorState *>(m_states_raw)),
                                   static_cast<std::ptrdiff_t>(header->size));
            const auto total_bytes = sizeof(header_s) + sizeof(ErrorState) * header->size;
            static_assert(std::is_trivially_destructible_v<header_s>);
            ::operator delete(m_states_raw - sizeof(header_s), total_bytes);
        }
    }
}

} // namespace detail

ErrorState::ErrorState(const std::string_view message) : ErrorState(message, move_from, {}) {}

ErrorState::ErrorState(const std::string_view message,
                       const move_from_t /*move_from*/,
                       const std::span<ErrorState> nested_states)
    : m_message(message),
      m_nested(move_from, nested_states) {}

Error::Error(ErrorState state) noexcept : m_state(std::move(state)) {}

} // namespace reflex::config

// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <reflex/config/context.hpp>

#include <iterator>
#include <ranges>
#include <string>

#include <fmt/compile.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <boost/json/string.hpp>

#include <reflex/ranges/to.hpp>

namespace {

template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

// escape a key as described in https://datatracker.ietf.org/doc/html/rfc6901#section-3
[[nodiscard]] std::string escape_json_pointer_key(const std::string_view key) {
    return key | std::views::transform([](const char &ch) -> std::string_view {
               if (ch == '~') {
                   [[unlikely]] return "~0";
               }
               if (ch == '/') {
                   [[unlikely]] return "~1";
               }
               // `ch` is a reference into the original string, so we don't return a dangling std::string_view
               return {&ch, 1};
           }) |
           std::views::join | reflex::ranges::to<std::string>();
}

} // namespace

template<>
struct fmt::formatter<reflex::config::context_entry_s, char> {
    constexpr auto parse(auto &context) const {
        auto it = context.begin();
        if (it != context.end() && *it != '}') {
            throw format_error{"invalid format specifier"};
        }
        return it;
    }

    constexpr auto format(const reflex::config::context_entry_s &entry, auto &context) const
            -> decltype(context.out()) {
        return std::visit(overloaded{[&](const std::string_view key) {
                                         const auto escaped = escape_json_pointer_key(key);
                                         return fmt::format_to(context.out(), FMT_COMPILE("{}"), escaped);
                                     },
                                     [&](const std::size_t index) {
                                         return fmt::format_to(context.out(), FMT_COMPILE("{}"), index);
                                     }},
                          entry.id);
    }
};

namespace reflex::config {

bool Context::push(const context_entry_s &entry) {
    if (m_suppress_error_formatting) {
        return false;
    }
    m_path.push_back(entry);
    return true;
}

void Context::pop() noexcept { m_path.pop_back(); }


ErrorFormattingSuppressionGuard::ErrorFormattingSuppressionGuard(Context &context) noexcept
    : m_context{context.m_suppress_error_formatting ? nullptr : &context} {
    if (m_context) {
        m_context->m_suppress_error_formatting = true;
    }
}

ErrorFormattingSuppressionGuard::~ErrorFormattingSuppressionGuard() {
    if (m_context) {
        m_context->m_suppress_error_formatting = false;
    }
}

ContextGuard::ContextGuard(Context &context, const context_entry_s &key)
    : m_context{context.push(key) ? &context : nullptr} {}

ContextGuard::~ContextGuard() {
    if (m_context) {
        m_context->pop();
    }
}

boost::json::string Context::as_json_pointer() const {
    auto result = boost::json::string{};
    fmt::format_to(std::back_inserter(result), FMT_COMPILE("{}"), fmt::join(m_path, "/"));
    return result;
}

// DeserializationContext member functions are defined in deserialize.cpp

} // namespace reflex::config

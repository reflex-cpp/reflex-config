// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <string_view>
#include <variant>
#include <vector>

#include <boost/json/fwd.hpp>

#include <reflex/config/fwd.hpp>

namespace reflex::config {

struct context_entry_s {
    std::variant<std::string_view, std::size_t> id{};
    bool is_nullable = false;
};

class [[nodiscard]] ErrorFormattingSuppressionGuard {
public:
    explicit ErrorFormattingSuppressionGuard(Context &context) noexcept;
    ~ErrorFormattingSuppressionGuard();

private:
    Context *m_context; // non-null if wasn't already suppressed
};

class [[nodiscard]] ContextGuard {
public:
    explicit ContextGuard(Context &context, const context_entry_s &key);
    ~ContextGuard();

private:
    Context *m_context;
};

class Context {
    friend class ErrorFormattingSuppressionGuard;
    friend class ContextGuard;

public:
    [[nodiscard]] bool should_suppress_error_formatting() const noexcept { return m_suppress_error_formatting; }
    [[nodiscard]] bool is_nullable() const noexcept { return !m_path.empty() && m_path.back().is_nullable; }

    // returning a JSON String so that formatting it yields an escaped string
    [[nodiscard]] boost::json::string as_json_pointer() const;

private:
    bool m_suppress_error_formatting = false;
    std::vector<context_entry_s> m_path;

    [[nodiscard]] bool push(const context_entry_s &entry); // returns whether the entry was pushed
    void pop() noexcept;
};

class DeserializationContext : public Context {
public:
    explicit DeserializationContext(deserialization_options_s options = {});

    [[nodiscard]] const deserialization_options_s &options() const noexcept { return m_options; }

    expected_t<void> on_unknown_key(const boost::json::string &key);

private:
    deserialization_options_s m_options;
};


} // namespace reflex::config

// Part of the Reflex project.
// Copyright © Reflex Project Contributors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <fmt/format.h>
#include <fmt/ostream.h>

#if FMT_VERSION >= 80000
    #define REFLEX_CONFIG_FMT_APPENDER(c) fmt::appender((c))
#else
    #define REFLEX_CONFIG_FMT_APPENDER(c) (c)
#endif

#if FMT_VERSION >= 90000
    #define REFLEX_CONFIG_FMT_STREAMED(v) fmt::streamed((v))
#else
    #define REFLEX_CONFIG_FMT_STREAMED(v) (v)
#endif

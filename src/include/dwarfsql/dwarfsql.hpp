// Copyright (c) 2024-2026 Elias Bachaalany
// SPDX-License-Identifier: LicenseRef-Human-Origin-Source-1.0
//
// This file is licensed under the Human-Origin Source License v1.0.
// See LICENSE.

#pragma once

/**
 * dwarfsql - SQL interface to DWARF debug information
 *
 * Main public header. Include this to use the dwarfsql library.
 */

#include "dwarf_session.hpp"
#include "dwarf_tables.hpp"

namespace dwarfsql {

constexpr const char* VERSION = "0.0.3";
constexpr const char* COPYRIGHT = "Copyright (c) 2024-2026 Elias Bachaalany";
constexpr int DEFAULT_PORT = 17199;

} // namespace dwarfsql

// Copyright (c) 2024-2026 Elias Bachaalany
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

/**
 * dwarfsql - SQL interface to DWARF debug information
 *
 * Main public header. Include this to use the dwarfsql library.
 */

#include "dwarf_session.hpp"
#include "dwarf_tables.hpp"

namespace dwarfsql {

constexpr const char* VERSION = "0.0.1";
constexpr int DEFAULT_PORT = 17199;

} // namespace dwarfsql

#pragma once

/**
 * dwarfsql - SQL interface to DWARF debug information
 *
 * Main public header. Include this to use the dwarfsql library.
 */

#include "dwarf_session.hpp"
#include "dwarf_tables.hpp"

namespace dwarfsql {

constexpr const char* VERSION = "0.1.0";
constexpr int DEFAULT_PORT = 17199;

} // namespace dwarfsql

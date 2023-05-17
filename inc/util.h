/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UTIL_H
#define UTIL_H

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "msl/bits.h"
#include "msl/lru_table.h"

namespace champsim
{
using msl::bitmask;
using msl::lg2;
using msl::lru_table;
using msl::splice_bits;

} // namespace champsim

#endif

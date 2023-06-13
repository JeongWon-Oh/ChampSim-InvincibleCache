#    Copyright 2023 The ChampSim Contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

def get_constants_file(env, pmem):
    yield from (
        '#ifndef CHAMPSIM_CONSTANTS_H',
        '#define CHAMPSIM_CONSTANTS_H',
        '#include <cstdlib>',
        '#include "util/bits.h"',
        f'constexpr unsigned BLOCK_SIZE = {env["block_size"]};'
        f'constexpr unsigned PAGE_SIZE = {env["page_size"]};'
        f'constexpr uint64_t STAT_PRINTING_PERIOD = {env["heartbeat_frequency"]};'
        f'constexpr std::size_t NUM_CPUS = {env["num_cores"]};'
        'constexpr auto LOG2_BLOCK_SIZE = champsim::lg2(BLOCK_SIZE);',
        'constexpr auto LOG2_PAGE_SIZE = champsim::lg2(PAGE_SIZE);',

        f'constexpr uint64_t DRAM_IO_FREQ = {pmem["io_freq"]};'
        f'constexpr std::size_t DRAM_CHANNELS = {pmem["channels"]};'
        f'constexpr std::size_t DRAM_RANKS = {pmem["ranks"]};'
        f'constexpr std::size_t DRAM_BANKS = {pmem["banks"]};'
        f'constexpr std::size_t DRAM_ROWS = {pmem["rows"]};'
        f'constexpr std::size_t DRAM_COLUMNS = {pmem["columns"]};'
        f'constexpr std::size_t DRAM_CHANNEL_WIDTH = {pmem["channel_width"]};'
        f'constexpr std::size_t DRAM_WQ_SIZE = {pmem["wq_size"]};'
        f'constexpr std::size_t DRAM_RQ_SIZE = {pmem["rq_size"]};'
        '#endif')


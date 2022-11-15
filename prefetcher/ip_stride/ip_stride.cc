#include <algorithm>
#include <array>
#include <map>

#include "cache.h"

#include "msl/lru_table.h"

namespace
{
struct tracker
{
  struct tracker_entry {
    uint64_t ip = 0;              // the IP we're tracking
    uint64_t last_cl_addr = 0;    // the last address accessed by this IP
    int64_t last_stride = 0;      // the stride between the last two addresses accessed by this IP

    auto index() const { return ip; }
    auto tag() const { return ip; }
  };

  struct lookahead_entry {
    uint64_t address = 0;
    int64_t stride = 0;
    int degree = 0; // degree remaining
  };

  constexpr static std::size_t TRACKER_SETS = 256;
  constexpr static std::size_t TRACKER_WAYS = 4;
  constexpr static int PREFETCH_DEGREE = 3;

  lookahead_entry lookahead{};

  champsim::msl::lru_table<tracker_entry> table{TRACKER_SETS, TRACKER_WAYS};
};

std::map<CACHE*, tracker> trackers;
} // namespace

void CACHE::prefetcher_initialize() { std::cout << NAME << " IP-based stride prefetcher" << std::endl; }

void CACHE::prefetcher_cycle_operate()
{
  // If a lookahead is active
  if (auto [old_pf_address, stride, degree] = ::trackers[this].lookahead; degree > 0) {
    auto pf_address = old_pf_address + (stride << LOG2_BLOCK_SIZE);

    // If the next step would exceed the degree or run off the page, stop
    if (virtual_prefetch || (pf_address >> LOG2_PAGE_SIZE) == (old_pf_address >> LOG2_PAGE_SIZE)) {
      // check the MSHR occupancy to decide if we're going to prefetch to this
      // level or not
      bool success = prefetch_line(pf_address, (get_occupancy(0, pf_address) < (get_size(0, pf_address) / 2)), 0);
      if (success)
        ::trackers[this].lookahead = {pf_address, stride, degree - 1};
      // If we fail, try again next cycle
    } else {
      ::trackers[this].lookahead = {};
    }
  }
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;
  int64_t stride = 0;

  // find the current ip within the set
  auto found = ::trackers[this].table.check_hit({ip, cl_addr, stride});

  // if we found a matching entry
  if (found.has_value()) {
    // calculate the stride between the current address and the last address
    // no need to check for overflow since these values are downshifted
    stride = (int64_t)cl_addr - (int64_t)found->last_cl_addr;

    // Initialize prefetch state unless we somehow saw the same address twice in
    // a row or if this is the first time we've seen this stride
    if (stride != 0 && stride == found->last_stride)
      ::trackers[this].lookahead = {cl_addr, stride, ::tracker::PREFETCH_DEGREE};
  }

  // update tracking set
  ::trackers[this].table.fill({ip, cl_addr, stride});

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_final_stats() {}

#include <algorithm>
#include <array>
#include <map>
#include <optional>

#include "cache.h"
#include "msl/lru_table.h"

#define NUM_IP_TABLE_L2_ENTRIES 1024
#define NUM_IP_INDEX_BITS 10
#define NUM_IP_TAG_BITS 6
#define S_TYPE 1                                            // global stream (GS)
#define CS_TYPE 2                                           // constant stride (CS)
#define CPLX_TYPE 3                                         // complex stride (CPLX)
#define NL_TYPE 4                                           // next line (NL)

namespace
{

class IP_TRACKER {
  public:
    uint64_t ip_tag;
    uint16_t ip_valid;
    uint32_t pref_type;                                     // prefetch class type
    int stride;                                             // last stride sent by metadata

    IP_TRACKER () {
        ip_tag = 0;
        ip_valid = 0;
        pref_type = 0;
        stride = 0;
    };
};

uint32_t spec_nl_l2[NUM_CPUS] = {0};
IP_TRACKER trackers[NUM_CPUS][NUM_IP_TABLE_L2_ENTRIES];

int decode_stride(uint32_t metadata){
    int stride=0;
    if(metadata & 0b1000000)
        stride = -1*(metadata & 0b111111);
    else
        stride = metadata & 0b111111;

    return stride;
}
} // namespace

void CACHE::prefetcher_initialize() {}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;
    int prefetch_degree = 0;
    int64_t stride = decode_stride(metadata_in);
    uint32_t pref_type = metadata_in & 0xF00;
    uint16_t ip_tag = (ip >> NUM_IP_INDEX_BITS) & ((1 << NUM_IP_TAG_BITS)-1);

if(NUM_CPUS == 1){
   if (MSHR.size() < (1*MSHR_SIZE)/2)
    prefetch_degree = 4;
   else 
    prefetch_degree = 3;  
} else {                                    // tightening the degree for multi-core
    prefetch_degree = 2;
}

// calculate the index bit
int index = ip & ((1 << NUM_IP_INDEX_BITS)-1);
    if(trackers[cpu][index].ip_tag != ip_tag){              // new/conflict IP
        if(trackers[cpu][index].ip_valid == 0){             // if valid bit is zero, update with latest IP info
        trackers[cpu][index].ip_tag = ip_tag;
        trackers[cpu][index].pref_type = pref_type;
        trackers[cpu][index].stride = stride;
    } else {
        trackers[cpu][index].ip_valid = 0;                  // otherwise, reset valid bit and leave the previous IP as it is
    }

        // issue a next line prefetch upon encountering new IP
        uint64_t pf_address = ((addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE;
        prefetch_line(ip, addr, pf_address, FILL_L2, 0);
        return metadata_in;
    }
    else {                                                  // if same IP encountered, set valid bit
        trackers[cpu][index].ip_valid = 1;
    }

// update the IP table upon receiving metadata from prefetch
if(type == (uint8_t)access_type::PREFETCH){
    trackers[cpu][index].pref_type = pref_type;
    trackers[cpu][index].stride = stride;
    spec_nl_l2[cpu] = metadata_in & 0x1000;
}

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}

#include "ptw.h"

#include "champsim.h"
#include "champsim_constants.h"
#include "util.h"

PageTableWalker::PageTableWalker(std::string v1, uint32_t cpu, unsigned fill_level, std::vector<champsim::simple_lru_table<uint64_t>>&& _pscl, uint32_t v10,
                                 uint32_t v11, uint32_t v12, uint32_t v13, unsigned latency, MemoryRequestConsumer* ll, VirtualMemory& _vmem)
    : champsim::operable(1), MemoryRequestConsumer(fill_level), MemoryRequestProducer(ll), NAME(v1), cpu(cpu), MSHR_SIZE(v11), MAX_READ(v12),
      MAX_FILL(v13), RQ{v10, latency}, pscl{_pscl}, vmem(_vmem), CR3_addr(_vmem.get_pte_pa(cpu, 0, std::size(pscl) + 1).first)
{
}

void PageTableWalker::handle_read()
{
  int reads_this_cycle = MAX_READ;

  while (reads_this_cycle > 0 && RQ.has_ready() && std::size(MSHR) != MSHR_SIZE) {
    PACKET& handle_pkt = RQ.front();

    auto walk_base = CR3_addr;
    auto walk_init_level = std::size(pscl);
    for (auto cache = std::begin(pscl); cache != std::end(pscl); ++cache) {
      if (auto check_addr = cache->check_hit(handle_pkt.address); check_addr.has_value()) {
        walk_base = check_addr.value();
        walk_init_level = std::distance(cache, std::end(pscl)) - 1;
      }
    }
    auto walk_offset = vmem.get_offset(handle_pkt.address, walk_init_level + 1) * PTE_BYTES;

    if constexpr (champsim::debug_print) {
      std::cout << "[" << NAME << "] " << __func__ << " instr_id: " << handle_pkt.instr_id;
      std::cout << " address: " << std::hex << walk_base;
      std::cout << " v_address: " << handle_pkt.v_address << std::dec;
      std::cout << " pt_page offset: " << walk_offset / PTE_BYTES;
      std::cout << " translation_level: " << +walk_init_level << std::endl;
    }

    PACKET packet = handle_pkt;
    packet.v_address = handle_pkt.address;
    packet.init_translation_level = walk_init_level;
    packet.cycle_enqueued = current_cycle;

    bool success = step_translation(splice_bits(walk_base, walk_offset, LOG2_PAGE_SIZE), walk_init_level, packet);
    if (!success)
      return;

    RQ.pop_front();
    reads_this_cycle--;
  }
}

void PageTableWalker::handle_fill()
{
  int fill_this_cycle = MAX_FILL;

  while (fill_this_cycle > 0 && !std::empty(MSHR) && MSHR.front().event_cycle <= current_cycle) {
    auto fill_mshr = MSHR.front();

    // Return the translated physical address to STLB. Does not contain last 12 bits
    uint64_t penalty;
    if (fill_mshr.translation_level == 0)
      std::tie(fill_mshr.data, penalty) = vmem.va_to_pa(cpu, fill_mshr.v_address);
    else
      std::tie(fill_mshr.data, penalty) = vmem.get_pte_pa(cpu, fill_mshr.v_address, fill_mshr.translation_level);
    fill_mshr.event_cycle = current_cycle + (warmup ? 0 : penalty);

    if constexpr (champsim::debug_print) {
      std::cout << "[" << NAME << "] " << __func__ << " instr_id: " << fill_mshr.instr_id;
      std::cout << " address: " << std::hex << fill_mshr.address;
      std::cout << " v_address: " << fill_mshr.v_address;
      std::cout << " data: " << fill_mshr.data << std::dec;
      std::cout << " pt_page offset: " << ((fill_mshr.data & bitmask(LOG2_PAGE_SIZE)) >> lg2(PTE_BYTES));
      std::cout << " translation_level: " << +fill_mshr.translation_level;
      std::cout << " event: " << fill_mshr.event_cycle << " current: " << current_cycle << std::endl;
    }

    if (fill_mshr.event_cycle <= current_cycle) {
      if (fill_mshr.translation_level == 0) {
        fill_mshr.address = fill_mshr.v_address;

        for (auto ret : fill_mshr.to_return)
          ret->return_data(fill_mshr);

        total_miss_latency += current_cycle - fill_mshr.cycle_enqueued;
      } else {
        const auto pscl_idx = std::size(pscl) - fill_mshr.translation_level;
        pscl.at(pscl_idx).fill_cache(fill_mshr.v_address, fill_mshr.data);

        auto success = step_translation(fill_mshr.data, fill_mshr.translation_level - 1, fill_mshr);
        if (!success)
          return;
      }

      MSHR.pop_front();
    } else {
      MSHR.sort(ord_event_cycle<PACKET>{});
    }

    fill_this_cycle--;
  }
}

bool PageTableWalker::step_translation(uint64_t addr, uint8_t transl_level, const PACKET& source)
{
  auto fwd_pkt = source;
  fwd_pkt.address = addr;
  fwd_pkt.fill_level = lower_level->fill_level; // This packet will be sent from L1 to PTW.
  fwd_pkt.cpu = cpu;
  fwd_pkt.type = TRANSLATION;
  fwd_pkt.to_return = {this};
  fwd_pkt.translation_level = transl_level;

  bool success = lower_level->add_rq(fwd_pkt);
  if (success) {
    fwd_pkt.to_return = source.to_return; // Set the return for MSHR packet same as read packet.
    fwd_pkt.type = source.type;
    fwd_pkt.event_cycle = std::numeric_limits<uint64_t>::max();
    MSHR.push_back(fwd_pkt);

    return true;
  }

  return false;
}

void PageTableWalker::operate()
{
  handle_fill();
  handle_read();
  RQ.operate();
}

bool PageTableWalker::add_rq(const PACKET& packet)
{
  assert(packet.address != 0);

  // check for duplicates in the read queue
  auto found_rq = std::find_if(RQ.begin(), RQ.end(), eq_addr<PACKET>(packet.address, LOG2_PAGE_SIZE));
  assert(found_rq == RQ.end()); // Duplicate request should not be sent.

  // check occupancy
  if (RQ.full()) {
    return false; // cannot handle this request
  }

  // if there is no duplicate, add it to RQ
  RQ.push_back(packet);

  return true;
}

void PageTableWalker::return_data(const PACKET& packet)
{
  for (auto& mshr_entry : MSHR) {
    if (eq_addr<PACKET>{packet.address, LOG2_BLOCK_SIZE}(mshr_entry)) {
      mshr_entry.event_cycle = current_cycle;

      if constexpr (champsim::debug_print) {
        std::cout << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << mshr_entry.instr_id;
        std::cout << " address: " << std::hex << mshr_entry.address;
        std::cout << " v_address: " << mshr_entry.v_address;
        std::cout << " data: " << mshr_entry.data << std::dec;
        std::cout << " translation_level: " << +mshr_entry.translation_level;
        std::cout << " occupancy: " << get_occupancy(0, mshr_entry.address);
        std::cout << " event: " << mshr_entry.event_cycle << " current: " << current_cycle << std::endl;
      }
    }
  }

  MSHR.sort(ord_event_cycle<PACKET>());
}

uint32_t PageTableWalker::get_occupancy(uint8_t queue_type, uint64_t address)
{
  if (queue_type == 0)
    return std::count_if(MSHR.begin(), MSHR.end(), is_valid<PACKET>());
  else if (queue_type == 1)
    return RQ.occupancy();
  return 0;
}

uint32_t PageTableWalker::get_size(uint8_t queue_type, uint64_t address)
{
  if (queue_type == 0)
    return MSHR_SIZE;
  else if (queue_type == 1)
    return RQ.size();
  return 0;
}

void PageTableWalker::print_deadlock()
{
  if (!std::empty(MSHR)) {
    std::cout << NAME << " MSHR Entry" << std::endl;
    std::size_t j = 0;
    for (PACKET entry : MSHR) {
      std::cout << "[" << NAME << " MSHR] entry: " << j++ << " instr_id: " << entry.instr_id;
      std::cout << " address: " << std::hex << entry.address << " v_address: " << entry.v_address << std::dec << " type: " << +entry.type;
      std::cout << " translation_level: " << +entry.translation_level;
      std::cout << " fill_level: " << +entry.fill_level << " event_cycle: " << entry.event_cycle << std::endl;
    }
  } else {
    std::cout << NAME << " MSHR empty" << std::endl;
  }
}

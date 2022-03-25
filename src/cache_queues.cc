#include "cache.h"

#include "champsim.h"
#include "instruction.h"
#include "util.h"

extern uint8_t warmup_complete[NUM_CPUS];

void CACHE::NonTranslatingQueues::operate()
{
  check_collision();
}

void CACHE::TranslatingQueues::operate()
{
  NonTranslatingQueues::operate();
  issue_translation();
  detect_misses();
}

void CACHE::NonTranslatingQueues::check_collision()
{
  std::size_t write_shamt = match_offset_bits ? 0 : OFFSET_BITS;
  std::size_t read_shamt = OFFSET_BITS;

  // Check WQ for duplicates, merging if they are found
  for (auto wq_it = std::find_if(std::begin(WQ), std::end(WQ), std::not_fn(&PACKET::forward_checked)); wq_it != std::end(WQ);) {
    if (auto found = std::find_if(std::begin(WQ), wq_it, eq_addr<PACKET>(wq_it->address, match_offset_bits ? 0 : OFFSET_BITS)); found != wq_it) {
      // Merge with earlier write
      WQ_MERGED++;
      wq_it = WQ.erase(wq_it);
    } else {
      wq_it->forward_checked = true;
      ++wq_it;
    }
  }

  // Check RQ for forwarding from WQ (return if found), then for duplicates (merge if found)
  for (auto rq_it = std::find_if(std::begin(RQ), std::end(RQ), std::not_fn(&PACKET::forward_checked)); rq_it != std::end(RQ);) {
    if (auto found_wq = std::find_if(std::begin(WQ), std::end(WQ), eq_addr<PACKET>(rq_it->address, write_shamt)); found_wq != std::end(WQ)) {
      // Forward from earlier write
      rq_it->data = found_wq->data;
      for (auto ret : rq_it->to_return)
        ret->return_data(*rq_it);

      WQ_FORWARD++;
      rq_it = RQ.erase(rq_it);
    } else if (auto found_rq = std::find_if(std::begin(RQ), rq_it, eq_addr<PACKET>(rq_it->address, read_shamt)); found_rq != rq_it) {
      // Merge with earlier read
      auto instr_copy = std::move(found_rq->instr_depend_on_me);
      auto ret_copy = std::move(found_rq->to_return);

      std::set_union(std::begin(instr_copy), std::end(instr_copy), std::begin(rq_it->instr_depend_on_me), std::end(rq_it->instr_depend_on_me),
                     std::back_inserter(found_rq->instr_depend_on_me), [](ooo_model_instr& x, ooo_model_instr& y) { return x.instr_id < y.instr_id; });
      std::set_union(std::begin(ret_copy), std::end(ret_copy), std::begin(rq_it->to_return), std::end(rq_it->to_return),
                     std::back_inserter(found_rq->to_return));

      RQ_MERGED++;
      rq_it = RQ.erase(rq_it);
    } else {
      rq_it->forward_checked = true;
      ++rq_it;
    }
  }

  // Check PQ for forwarding from WQ (return if found), then for duplicates (merge if found)
  for (auto pq_it = std::find_if(std::begin(PQ), std::end(PQ), std::not_fn(&PACKET::forward_checked)); pq_it != std::end(PQ);) {
    if (auto found_wq = std::find_if(std::begin(WQ), std::end(WQ), eq_addr<PACKET>(pq_it->address, write_shamt)); found_wq != std::end(WQ)) {
      // Forward from earlier write
      pq_it->data = found_wq->data;
      for (auto ret : pq_it->to_return)
        ret->return_data(*pq_it);

      WQ_FORWARD++;
      pq_it = PQ.erase(pq_it);
    } else if (auto found = std::find_if(std::begin(PQ), pq_it, eq_addr<PACKET>(pq_it->address, read_shamt)); found != pq_it) {
      // Merge with earlier prefetch
      auto ret_copy = std::move(found->to_return);
      std::set_union(std::begin(ret_copy), std::end(ret_copy), std::begin(pq_it->to_return), std::end(pq_it->to_return), std::back_inserter(found->to_return));

      PQ_MERGED++;
      pq_it = PQ.erase(pq_it);
    } else {
      pq_it->forward_checked = true;
      ++pq_it;
    }
  }
}

void CACHE::TranslatingQueues::issue_translation()
{
  do_issue_translation(WQ);
  do_issue_translation(RQ);
  do_issue_translation(PQ);
}

template <typename R>
void CACHE::TranslatingQueues::do_issue_translation(R &queue)
{
  for (auto &q_entry : queue) {
    if (!q_entry.translate_issued && q_entry.address == q_entry.v_address) {
      auto fwd_pkt = q_entry;
      fwd_pkt.type = LOAD;
      fwd_pkt.to_return = {this};
      auto success = lower_level->add_rq(fwd_pkt);
      if (success) {
        if constexpr (champsim::debug_print) {
          std::cout << "[TRANSLATE] " << __func__ << " instr_id: " << q_entry.instr_id;
          std::cout << " address: "  << std::hex << q_entry.address << " v_address: " << q_entry.v_address << std::dec;
          std::cout << " type: " << +q_entry.type << " occupancy: " << std::size(queue) << std::endl;
        }

        q_entry.translate_issued = true;
        q_entry.address = 0;
      }
    }
  }
}

void CACHE::TranslatingQueues::detect_misses()
{
  do_detect_misses(WQ);
  do_detect_misses(RQ);
  do_detect_misses(PQ);
}

template <typename R>
void CACHE::TranslatingQueues::do_detect_misses(R &queue)
{
  // Find entries that would be ready except that they have not finished translation, move them to the back of the queue
  auto q_it = std::find_if_not(std::begin(queue), std::end(queue), [this](auto x){ return x.event_cycle < this->current_cycle && x.address == 0; });
  std::for_each(std::begin(queue), q_it, [](auto &x){ x.event_cycle = std::numeric_limits<uint64_t>::max(); });
  std::rotate(std::begin(queue), q_it, std::end(queue));
}

bool CACHE::NonTranslatingQueues::add_rq(const PACKET &packet)
{
  assert(packet.address != 0);
  RQ_ACCESS++;

  // check occupancy
  if (std::size(RQ) >= RQ_SIZE) {
    RQ_FULL++;

    if constexpr (champsim::debug_print) {
      std::cout << " FULL" << std::endl;
    }

    return false; // cannot handle this request
  }

  // Insert the packet ahead of the translation misses
  auto ins_loc = std::find_if(std::begin(RQ), std::end(RQ), [](auto x){ return x.event_cycle == std::numeric_limits<uint64_t>::max(); });
  auto fwd_pkt = packet;
  fwd_pkt.forward_checked = false;
  fwd_pkt.translate_issued = false;
  fwd_pkt.event_cycle = current_cycle + warmup_complete[packet.cpu] ? HIT_LATENCY : 0;
  RQ.insert(ins_loc, fwd_pkt);

  if constexpr (champsim::debug_print) {
    std::cout << " ADDED" << std::endl;
  }

  RQ_TO_CACHE++;
  return true;
}

bool CACHE::NonTranslatingQueues::add_wq(const PACKET &packet)
{
  WQ_ACCESS++;

  // Check for room in the queue
  if (std::size(WQ) >= WQ_SIZE) {
    if constexpr (champsim::debug_print) {
      std::cout << " FULL" << std::endl;
    }

    ++WQ_FULL;
    return false;
  }

  // Insert the packet ahead of the translation misses
  auto ins_loc = std::find_if(std::begin(WQ), std::end(WQ), [](auto x){ return x.event_cycle == std::numeric_limits<uint64_t>::max(); });
  auto fwd_pkt = packet;
  fwd_pkt.forward_checked = false;
  fwd_pkt.translate_issued = false;
  fwd_pkt.event_cycle = current_cycle + warmup_complete[packet.cpu] ? HIT_LATENCY : 0;
  WQ.insert(ins_loc, fwd_pkt);

  if constexpr (champsim::debug_print) {
    std::cout << " ADDED" << std::endl;
  }

  WQ_TO_CACHE++;
  WQ_ACCESS++;

  return true;
}

bool CACHE::NonTranslatingQueues::add_pq(const PACKET &packet)
{
  assert(packet.address != 0);
  PQ_ACCESS++;

  // check occupancy
  if (std::size(PQ) >= PQ_SIZE) {

    if constexpr (champsim::debug_print) {
      std::cout << " FULL" << std::endl;
    }

    PQ_FULL++;
    return false; // cannot handle this request
  }

  // Insert the packet ahead of the translation misses
  auto ins_loc = std::find_if(std::begin(PQ), std::end(PQ), [](auto x){ return x.event_cycle == std::numeric_limits<uint64_t>::max(); });
  auto fwd_pkt = packet;
  fwd_pkt.forward_checked = false;
  fwd_pkt.translate_issued = false;
  fwd_pkt.event_cycle = current_cycle + warmup_complete[packet.cpu] ? HIT_LATENCY : 0;
  PQ.insert(ins_loc, fwd_pkt);

  if constexpr (champsim::debug_print) {
    std::cout << " ADDED" << std::endl;
  }

  PQ_TO_CACHE++;
  return true;
}

bool CACHE::NonTranslatingQueues::wq_has_ready() const
{
  return WQ.front().event_cycle <= current_cycle;
}

bool CACHE::NonTranslatingQueues::rq_has_ready() const
{
  return RQ.front().event_cycle <= current_cycle;
}

bool CACHE::NonTranslatingQueues::pq_has_ready() const
{
  return PQ.front().event_cycle <= current_cycle;
}

bool CACHE::TranslatingQueues::wq_has_ready() const
{
  return NonTranslatingQueues::wq_has_ready() && WQ.front().address != 0 && WQ.front().address != WQ.front().v_address;
}

bool CACHE::TranslatingQueues::rq_has_ready() const
{
  return NonTranslatingQueues::rq_has_ready() && RQ.front().address != 0 && RQ.front().address != RQ.front().v_address;
}

bool CACHE::TranslatingQueues::pq_has_ready() const
{
  return NonTranslatingQueues::pq_has_ready() && PQ.front().address != 0 && PQ.front().address != PQ.front().v_address;
}

void CACHE::TranslatingQueues::return_data(const PACKET &packet)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[TRANSLATE] " << __func__ << " instr_id: " << packet.instr_id;
    std::cout << " address: " << std::hex << packet.address;
    std::cout << " data: " << packet.data << std::dec;
    std::cout << " event: " << packet.event_cycle << " current: " << current_cycle << std::endl;
  }

  // Find all packets that match the page of the returned packet
  for (auto &wq_entry : WQ) {
    if ((wq_entry.v_address >> LOG2_PAGE_SIZE) == (packet.v_address >> LOG2_PAGE_SIZE)) {
      wq_entry.address = splice_bits(packet.data, wq_entry.v_address, LOG2_PAGE_SIZE); // translated address
      wq_entry.event_cycle = std::min(wq_entry.event_cycle, current_cycle + (warmup_complete[wq_entry.cpu] ? HIT_LATENCY : 0));
    }
  }

  for (auto &rq_entry : RQ) {
    if ((rq_entry.v_address >> LOG2_PAGE_SIZE) == (packet.v_address >> LOG2_PAGE_SIZE)) {
      rq_entry.address = splice_bits(packet.data, rq_entry.v_address, LOG2_PAGE_SIZE); // translated address
      rq_entry.event_cycle = std::min(rq_entry.event_cycle, current_cycle + (warmup_complete[rq_entry.cpu] ? HIT_LATENCY : 0));
    }
  }

  for (auto &pq_entry : PQ) {
    if ((pq_entry.v_address >> LOG2_PAGE_SIZE) == (packet.v_address >> LOG2_PAGE_SIZE)) {
      pq_entry.address = splice_bits(packet.data, pq_entry.v_address, LOG2_PAGE_SIZE); // translated address
      pq_entry.event_cycle = std::min(pq_entry.event_cycle, current_cycle + (warmup_complete[pq_entry.cpu] ? HIT_LATENCY : 0));
    }
  }
}

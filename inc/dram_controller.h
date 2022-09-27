#ifndef DRAM_H
#define DRAM_H

#include <array>
#include <cmath>
#include <limits>

#include "champsim_constants.h"
#include "memory_class.h"
#include "operable.h"
#include "util.h"

struct dram_stats {
  std::string name;
  uint64_t dbus_cycle_congested = 0, dbus_count_congested = 0;

  unsigned WQ_ROW_BUFFER_HIT = 0, WQ_ROW_BUFFER_MISS = 0, RQ_ROW_BUFFER_HIT = 0, RQ_ROW_BUFFER_MISS = 0, WQ_FULL = 0;
};

struct DRAM_CHANNEL {
  using queue_type = std::vector<PACKET>;
  queue_type WQ{DRAM_WQ_SIZE}, RQ{DRAM_RQ_SIZE};

  struct BANK_REQUEST {
    bool valid = false, row_buffer_hit = false;

    std::size_t open_row = std::numeric_limits<uint32_t>::max();

    uint64_t event_cycle = 0;

    queue_type::iterator pkt;
  };

  using request_array_type = std::array<BANK_REQUEST, DRAM_RANKS * DRAM_BANKS>;
  request_array_type bank_request = {};
  request_array_type::iterator active_request = std::end(bank_request);

  bool write_mode = false;
  uint64_t dbus_cycle_available = 0;

  using stats_type = dram_stats;
  std::vector<stats_type> roi_stats, sim_stats;

  void check_collision();
};

class MEMORY_CONTROLLER : public champsim::operable, public MemoryRequestConsumer
{
  // Latencies
  const uint64_t tRP, tRCD, tCAS, DRAM_DBUS_TURN_AROUND_TIME, DRAM_DBUS_RETURN_TIME = std::ceil(1.0 * BLOCK_SIZE / DRAM_CHANNEL_WIDTH);

  // these values control when to send out a burst of writes
  constexpr static std::size_t DRAM_WRITE_HIGH_WM = ((DRAM_WQ_SIZE * 7) >> 3);         // 7/8th
  constexpr static std::size_t DRAM_WRITE_LOW_WM = ((DRAM_WQ_SIZE * 6) >> 3);          // 6/8th
  constexpr static std::size_t MIN_DRAM_WRITES_PER_SWITCH = ((DRAM_WQ_SIZE * 1) >> 2); // 1/4

public:
  std::array<DRAM_CHANNEL, DRAM_CHANNELS> channels;

  MEMORY_CONTROLLER(double freq_scale, int io_freq, double t_rp, double t_rcd, double t_cas, double turnaround);

  void initialize() override;
  void operate() override;
  void begin_phase() override;
  void end_phase(unsigned cpu) override;

  bool add_rq(const PACKET& packet) override;
  bool add_wq(const PACKET& packet) override;
  bool add_pq(const PACKET& packet) override;

  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override;
  uint32_t get_size(uint8_t queue_type, uint64_t address) override;

  std::size_t size() const;

  uint32_t dram_get_channel(uint64_t address);
  uint32_t dram_get_rank(uint64_t address);
  uint32_t dram_get_bank(uint64_t address);
  uint32_t dram_get_row(uint64_t address);
  uint32_t dram_get_column(uint64_t address);
};

#endif

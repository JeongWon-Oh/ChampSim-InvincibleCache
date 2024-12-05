#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"
#include <stdio.h>

SCENARIO("Invincible Cache Test on Inclusive LLC") {
  GIVEN("Three cache levels with LLC filled by cpu 0") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    constexpr uint64_t fill_latency = 2;
    constexpr uint64_t RANDOM_EVICTION_FREQ = 1000;
    do_nothing_MRC mock_ll{miss_latency};
    
    to_rq_MRP mock_ul_1;
    to_rq_MRP mock_ul_2;

    champsim::channel l1_to_l2_1{};
    champsim::channel l1_to_l2_2{};
    champsim::channel l2_to_llc_1{};
    champsim::channel l2_to_llc_2{};

    CACHE cpu0_l1d{CACHE::Builder{champsim::defaults::default_l1d}
      .name("cpu0-l1d")
      .upper_levels({&mock_ul_1.queues})
      .lower_level(&l1_to_l2_1)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .sets(1)
      .ways(1)
    };

    CACHE cpu1_l1d{CACHE::Builder{champsim::defaults::default_l1d}
      .name("cpu1-l1d")
      .upper_levels({&mock_ul_2.queues})
      .lower_level(&l1_to_l2_2)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .sets(1)
      .ways(1)
    };

    CACHE cpu0_l2{CACHE::Builder{champsim::defaults::default_l2c}
      .name("cpu0-l2")
      .upper_levels({&l1_to_l2_1})
      .lower_level(&l2_to_llc_1)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .sets(1)
      .ways(1)
    };

    CACHE cpu1_l2{CACHE::Builder{champsim::defaults::default_l2c}
      .name("cpu1-l2")
      .upper_levels({&l1_to_l2_2})
      .lower_level(&l2_to_llc_2)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .sets(1)
      .ways(1)
    };

    CACHE LLC{CACHE::Builder{champsim::defaults::default_llc}
      .name("LLC")
      .upper_levels({&l2_to_llc_1, &l2_to_llc_2})
      .lower_level(&mock_ll.queues)
      .sets(1)
      .ways(2)
    };

    std::array<champsim::operable*, 8> elements{{&cpu0_l1d, &cpu1_l1d, &cpu0_l2, &cpu1_l2, &LLC, &mock_ll, &mock_ul_1, &mock_ul_2}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Create a test packet
    static uint64_t id = 1;
    decltype(mock_ul_1)::request_type seed;
    seed.address = 0xdeadbeef;
    seed.cpu = 0;
    seed.instr_id = id++;
    // Issue it to the uut
    auto seed_result = mock_ul_1.issue(seed);
    THEN("This issue is received") {
      REQUIRE(seed_result);
    }

    // Run the uut for long enough to fulfill the request
    for (uint64_t i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    decltype(mock_ul_1)::request_type seed2;
    seed2.address = 0xcafecafe;
    seed2.cpu = 0;
    seed2.instr_id = id++;

    auto seed_result2 = mock_ul_1.issue(seed2);
    THEN("This issue is received") {
      REQUIRE(seed_result2);
    }

    for (uint64_t i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    THEN("LLC is invincible") {
      REQUIRE(LLC.inv_table[0]);
    }

    WHEN("cpu1 trys to access LLC") {
      decltype(mock_ul_2)::request_type test;
      test.address = 0xcafecafe;
      test.cpu = 1;
      test.instr_id = id++;

      auto test_result = mock_ul_2.issue(test);
      THEN("This issue is received") {
        REQUIRE(test_result);
      }

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("cpu1 is blocked") {
        REQUIRE(LLC.block.front().cpu != 1);
        REQUIRE(LLC.block.back().cpu != 1);
      }

      AND_WHEN("cpu0 trys to access LLC") {
        decltype(mock_ul_1)::request_type test2;
        test2.address = 0xbabebabe;
        test2.cpu = 0;
        test2.instr_id = id++;

        auto test2_result = mock_ul_1.issue(test2);
        THEN("This issue is received") {
          REQUIRE(test2_result);
        }

        for (uint64_t i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();
        
        THEN("assert in cartel access") {
          REQUIRE(cpu0_l1d.block.front().address == test2.address);
        }

        AND_WHEN("after random eviction frequency") {
          for (uint64_t i = 0; i < 1000; ++i)
            for (auto elem : elements)
              elem->_operate();
          THEN("Invincible state is freed") {
            REQUIRE(!LLC.inv_table[0]);
          }
        }
      }
    }
  }
}

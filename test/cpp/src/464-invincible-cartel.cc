#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"

#include <iostream>

TEST_CASE("Cartel test") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll{miss_latency};
    to_rq_MRP mock_ul_seed;
    to_rq_MRP mock_ul_test;

    champsim::channel seed_to_llc{};
    champsim::channel test_to_llc{};

    CACHE cpu0_l1{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("cpu0_l1")
      .upper_levels({&mock_ul_seed.queues})
      .lower_level(&seed_to_llc)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .sets(1)
      .ways(1)
    };
    cpu0_l1.cpu = 0;

    CACHE cpu1_l1{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("cpu1_l1")
      .upper_levels({&mock_ul_test.queues})
      .lower_level(&test_to_llc)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .sets(1)
      .ways(1)
    };
    cpu1_l1.cpu = 1;

    CACHE llc{champsim::cache_builder{champsim::defaults::default_llc}
      .name("LLC")
      .upper_levels({&seed_to_llc, &test_to_llc})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .sets(1)
      .ways(2)
    };

    std::array<champsim::operable*, 6> elements{{&cpu0_l1, &cpu1_l1, &llc, &mock_ll, &mock_ul_seed, &mock_ul_test}};
    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    std::cout << elements.size() << std::endl;
    
    for (int i = 0; i < 10; ++i) {
      elements.at(0)->_operate();
      elements.at(1)->_operate();
      elements.at(2)->_operate();
      elements.at(3)->_operate();
      elements.at(4)->_operate();
      elements.at(5)->_operate();
    }

    static uint64_t id = 1;
    decltype(mock_ul_seed)::request_type seed;
    seed.address = 0xa0a0a0a0;
    seed.cpu = 0;
    seed.instr_id = id++;
    seed.type = access_type::LOAD;
    seed.clusivity = champsim::inclusivity::exclusive;

    auto seed_result = mock_ul_seed.issue(seed);
    
    for (int i = 0; i < 100; ++i) {
      elements.at(0)->_operate();
      elements.at(1)->_operate();
      elements.at(2)->_operate();
      elements.at(3)->_operate();
      elements.at(4)->_operate();
      elements.at(5)->_operate();
    }

    std::cout << "break 1"<< std::endl;

    decltype(mock_ul_seed)::request_type seed2;
    seed2.address = 0xbabebabe;
    seed2.cpu = 0;
    seed2.instr_id = id++;
    seed2.type = access_type::LOAD;
    seed2.clusivity = champsim::inclusivity::exclusive;

    auto seed2_result = mock_ul_seed.issue(seed2);

    for (int i = 0; i < 100; ++i) {
      elements.at(0)->_operate();
      elements.at(1)->_operate();
      elements.at(2)->_operate();
      elements.at(3)->_operate();
      elements.at(4)->_operate();
      elements.at(5)->_operate();
    }

    std::cout << "break 2"<< std::endl;

    //AND_WHEN("Try to read from invincible set") {
      decltype(mock_ul_test)::request_type reader;
      reader.address = 0xcafecafe;
      reader.cpu = 1;
      reader.instr_id = id++;
      reader.type = access_type::LOAD;
      reader.clusivity = champsim::inclusivity::exclusive;

      auto reader_result = mock_ul_test.issue(reader);

      for (int i = 0; i < 100; ++i) {
        elements.at(0)->_operate(); std::cout << "flag 0" << std::endl;
        elements.at(1)->_operate(); std::cout << "flag 1" << std::endl;
        elements.at(2)->_operate(); std::cout << "flag 2" << std::endl;
        elements.at(3)->_operate(); std::cout << "flag 3" << std::endl;
        elements.at(4)->_operate(); std::cout << "flag 4" << std::endl;
        elements.at(5)->_operate(); std::cout << "flag 5" << std::endl;
      }

      std::cout << "break 3" << std::endl;

    decltype(mock_ul_test)::request_type reader2;
    reader2.address = 0xdeadbeef;
    reader2.cpu = 1;
    reader2.instr_id = id++;
    reader2.type = access_type::LOAD;
    reader2.clusivity = champsim::inclusivity::exclusive;

    auto reader2_result = mock_ul_test.issue(reader2);

    for (int i = 0; i < 100; ++i) {
      elements.at(0)->_operate();
      elements.at(1)->_operate();
      elements.at(2)->_operate();
      elements.at(3)->_operate();
      elements.at(4)->_operate();
      elements.at(5)->_operate();
    }

    std::cout << "break 4" << std::endl;
}


#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"

#include <iostream>

TEST_CASE("Reading invincible LLC set") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll{miss_latency};
    to_wq_MRP mock_ul_seed;
    to_wq_MRP mock_ul_test;

    champsim::channel seed_to_llc{};
    champsim::channel test_to_llc{};
    champsim::channel seed_to_test{};

    CACHE seed_upper{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("463-seed-upper")
      .upper_levels({&mock_ul_seed.queues, &mock_ul_test.queues})
      .lower_level(&seed_to_test)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .sets(1)
      .ways(1)
    };
    seed_upper.cpu = 0;

    CACHE test_upper{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("463-test-upper")
      .upper_levels({&seed_to_test})
      .lower_level(&test_to_llc)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .sets(1)
      .ways(1)
    };
    test_upper.cpu = 0;

    CACHE llc{champsim::cache_builder{champsim::defaults::default_llc}
      .name("LLC")
      .upper_levels({&test_to_llc})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .sets(1)
      .ways(2)
    };

    std::array<champsim::operable*, 6> elements{{&seed_upper, &test_upper, &llc, &mock_ll, &mock_ul_seed, &mock_ul_test}};
    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }
    std::cout << elements.size() << std::endl;
    for (int i = 0; i < 10; ++i) {
      //std::cout << "flag 1" << std::endl;
      elements.at(0)->_operate();
      elements.at(1)->_operate();
      elements.at(2)->_operate();
      elements.at(3)->_operate();
      elements.at(4)->_operate();
      elements.at(5)->_operate();
    }

    // Create a test packet
    static uint64_t id = 1;
    decltype(mock_ul_seed)::request_type seed;
    seed.address = 0xa0a0a0a0;
    seed.cpu = 0;
    seed.instr_id = id++;
    seed.type = access_type::WRITE;
    seed.clusivity = champsim::inclusivity::inclusive;

    // Issue it to the uut
    auto seed_result = mock_ul_seed.issue(seed);

    // Run the uut for long enough to fulfill the request
    for (int i = 0; i < 100; ++i) {
      //std::cout << "flag 1" << std::endl;
      elements.at(0)->_operate();
      elements.at(1)->_operate();
      elements.at(2)->_operate();
      elements.at(3)->_operate();
      elements.at(4)->_operate();
      elements.at(5)->_operate();
    }

    THEN("This issue is received") {
      REQUIRE(seed_result);
    }

    auto print_result1 = llc.is_invincible(0xdeadbeef) ? "true" : "false";
    std::cout << "1: " << print_result1 << std::endl;

    //WHEN("second access") {

    decltype(mock_ul_seed)::request_type seed2;
    seed2.address = 0xbabebabe;
    seed2.cpu = 0;
    seed2.instr_id = id++;
    seed2.type = access_type::WRITE;
    seed2.clusivity = champsim::inclusivity::inclusive;

    auto seed2_result = mock_ul_seed.issue(seed2);

    for (int i = 0; i < 100; ++i) {
      //std::cout << "flag 2" << std::endl;
      elements.at(0)->_operate();
      elements.at(1)->_operate();
      elements.at(2)->_operate();
      elements.at(3)->_operate();
      elements.at(4)->_operate();
      elements.at(5)->_operate();
    }

    THEN("This issue is received") {
      REQUIRE(seed2_result);
    }

    auto print_result2 = llc.is_invincible(0xdeadbeef) ? "true" : "false";
    std::cout << "2: " << print_result2 << std::endl;

    //AND_WHEN("Try to read from invincible set") {
      decltype(mock_ul_test)::request_type reader;
      reader.address = 0xa0a0a0a0;
      reader.cpu = 0;
      reader.instr_id = id++;
      reader.type = access_type::WRITE;
      reader.clusivity = champsim::inclusivity::inclusive;

      // Issue it to the uut
      auto reader_result = mock_ul_test.issue(reader);
      //THEN("This issue is received") {
      //  REQUIRE(reader_result);
      //}

      for (int i = 0; i < 100; ++i) {
        //std::cout << "flag3 " << i << std::endl;
        elements.at(0)->_operate();
        elements.at(1)->_operate();
        elements.at(2)->_operate();
        elements.at(3)->_operate();
        elements.at(4)->_operate();
        elements.at(5)->_operate();
      }

      // for (uint64_t i = 0; i < 10; ++i)
      //   for (auto elem : elements)
      //     elem->_operate();

      auto print_result3 = llc.is_invincible(0xdeadbeef) ? "true" : "false";
      std::cout << "3: " << reader_result << std::endl;

   //     AND_WHEN("second access to invincible set") {
    decltype(mock_ul_test)::request_type reader2;
    reader2.address = 0xdeadbeef;
    reader2.cpu = 0;
    reader2.instr_id = id++;
    reader2.type = access_type::WRITE;
    reader2.clusivity = champsim::inclusivity::inclusive;

    // Issue it to the uut
    auto reader2_result = mock_ul_test.issue(reader2);
    //THEN("This issue is received") {
    //  REQUIRE(reader2_result);
    //}

    for (int i = 0; i < 100; ++i) {
      //std::cout << "flag4 " << i << std::endl;
      elements.at(0)->_operate();
      elements.at(1)->_operate();
      elements.at(2)->_operate();
      elements.at(3)->_operate();
      elements.at(4)->_operate();
      elements.at(5)->_operate();
    }

          // for (uint64_t i = 0; i < 10; ++i)
          //   for (auto elem : elements)
          //     elem->_operate();

          auto print_result4 = llc.is_invincible(0xdeadbeef) ? "true" : "false";
          std::cout << "4: " << reader2_result << std::endl;

          

      //  }
      //}
    //}
}


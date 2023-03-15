#include <catch.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <algorithm>

#include "mocks.hpp"
#include "ooo_cpu.h"

namespace {
template<typename T>
struct AllEqualMatcher : Catch::Matchers::MatcherGenericBase {
    AllEqualMatcher(const T& val):
        val_{ val }
    {}

    template<typename Other>
    bool match(const Other& other) const {
        return other == val_;
    }

    std::string describe() const override {
        return "All match {" + Catch::to_string(val_.first) + ", " + Catch::to_string(val_.second) + "}";
    }

private:
    const T& val_;
};
}

TEST_CASE("The basic_btb does not fill not-taken branches") {
    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{0, 1.0, {32, 8, {2}, {2}}, 64, 32, 32, 352, 128, 72, 2, 2, 2, 128, 1, 2, 2, 1, 1, 1, 1, 0, 0, &mock_L1I, 1, &mock_L1D, 1, O3_CPU::bbranchDbimodal, O3_CPU::tbtbDbasic_btb};

    std::array<uint64_t, 8> seed_ip{{0x110000, 0x111000, 0x112000, 0x113000, 0x114000, 0x115000, 0x116000, 0x117000}};
    uint64_t test_ip = 0x118000;

    uut.initialize();

    // Fill the BTB ways
    const uint64_t fake_target = 0x66b5f0;
    for (auto ip : seed_ip)
      uut.impl_update_btb(ip, fake_target, true, BRANCH_CONDITIONAL);

    // Check that all of the addresses are in the BTB
    std::vector<std::pair<uint64_t, uint8_t>> seed_check_result{};
    std::transform(std::cbegin(seed_ip), std::cend(seed_ip), std::back_inserter(seed_check_result), [&](auto ip){ return uut.impl_btb_prediction(ip); });
    REQUIRE_THAT(seed_check_result, Catch::Matchers::AllMatch(::AllEqualMatcher{std::pair{fake_target, (uint8_t)true}}));

    // Attempt to fill with not-taken
    uut.impl_update_btb(test_ip, 0, false, BRANCH_CONDITIONAL);

    // The first seeded IP is still present
    auto [seed_predicted_target, seed_always_taken] = uut.impl_btb_prediction(seed_ip.front());
    REQUIRE_FALSE(seed_predicted_target == 0); // Branch target should be known
    REQUIRE(seed_always_taken);

    // The block is not filled
    auto [predicted_target, always_taken] = uut.impl_btb_prediction(test_ip);
    REQUIRE(predicted_target == 0); // Branch target should not be known
}


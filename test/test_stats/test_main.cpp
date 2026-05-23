#include <unity.h>
#include "stats.h"

// ── helpers ──────────────────────────────────────────────────────────────────

static void reset_state() {
  memset(&_stats, 0, sizeof(_stats));
  _dirty              = false;
  _lastBridgeTokens   = 0;
  _tokensSynced       = false;
  _levelUpPending     = false;
  _lastNapEndMs       = 0;
  _energyAtNap        = 3;   // boot default
  _mock_millis        = 0;
}

// Push N velocity samples without going through NVS.
static void push_velocity(uint16_t* vals, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    _stats.velocity[_stats.velIdx] = vals[i];
    _stats.velIdx = (_stats.velIdx + 1) % 8;
    if (_stats.velCount < 8) _stats.velCount++;
  }
}

// ── statsMedianVelocity ───────────────────────────────────────────────────────

void test_median_empty() {
  // No samples → 0
  TEST_ASSERT_EQUAL_UINT16(0, statsMedianVelocity());
}

void test_median_single() {
  uint16_t v[] = { 42 };
  push_velocity(v, 1);
  // n=1: tmp[0] = 42
  TEST_ASSERT_EQUAL_UINT16(42, statsMedianVelocity());
}

void test_median_odd_count() {
  uint16_t v[] = { 30, 10, 50 };
  push_velocity(v, 3);
  // sorted: [10, 30, 50], n/2 = 1 → 30
  TEST_ASSERT_EQUAL_UINT16(30, statsMedianVelocity());
}

void test_median_even_count_upper() {
  uint16_t v[] = { 10, 20 };
  push_velocity(v, 2);
  // sorted: [10, 20], n/2 = 1 → upper median = 20
  TEST_ASSERT_EQUAL_UINT16(20, statsMedianVelocity());
}

void test_median_full_buffer() {
  uint16_t v[] = { 80, 20, 60, 40, 100, 10, 70, 30 };
  push_velocity(v, 8);
  // sorted: [10,20,30,40,60,70,80,100], n/2 = 4 → 60
  TEST_ASSERT_EQUAL_UINT16(60, statsMedianVelocity());
}

void test_median_ring_wraps() {
  // Fill buffer then add two more; oldest two should be evicted.
  uint16_t first[] = { 10, 20, 30, 40, 50, 60, 70, 80 };
  push_velocity(first, 8);
  uint16_t extra[] = { 5, 3 };
  push_velocity(extra, 2);
  // Ring now holds: [3, 5, 30, 40, 50, 60, 70, 80]
  // sorted: [3,5,30,40,50,60,70,80], n/2=4 → 50
  TEST_ASSERT_EQUAL_UINT16(50, statsMedianVelocity());
}

// ── statsMoodTier ─────────────────────────────────────────────────────────────

void test_mood_no_data() {
  // vel=0 → tier 2 (neutral), no decisions → no penalty
  TEST_ASSERT_EQUAL_UINT8(2, statsMoodTier());
}

void test_mood_fast_responses() {
  uint16_t v[] = { 5, 8, 3 };
  push_velocity(v, 3);
  // median=5, vel<15 → tier 4; 0 denials → no penalty
  TEST_ASSERT_EQUAL_UINT8(4, statsMoodTier());
}

void test_mood_slow_responses() {
  uint16_t v[] = { 200, 300, 150 };
  push_velocity(v, 3);
  // median=200, vel>=120 → tier 0
  TEST_ASSERT_EQUAL_UINT8(0, statsMoodTier());
}

void test_mood_denial_penalty_heavy() {
  uint16_t v[] = { 5, 5, 5 };
  push_velocity(v, 3);
  _stats.approvals = 3;
  _stats.denials   = 5;   // d > a → -2
  // base tier 4, -2 = 2
  TEST_ASSERT_EQUAL_UINT8(2, statsMoodTier());
}

void test_mood_denial_penalty_light() {
  uint16_t v[] = { 5, 5, 5 };
  push_velocity(v, 3);
  _stats.approvals = 4;
  _stats.denials   = 2;   // d*2=4 == a, not > a → no -1 (d*2 > a is false)
  TEST_ASSERT_EQUAL_UINT8(4, statsMoodTier());
}

void test_mood_denial_penalty_moderate() {
  uint16_t v[] = { 5, 5, 5 };
  push_velocity(v, 3);
  _stats.approvals = 4;
  _stats.denials   = 3;   // d*2=6 > a=4 → -1; d=3 not > a=4 → not -2
  // base 4 - 1 = 3
  TEST_ASSERT_EQUAL_UINT8(3, statsMoodTier());
}

void test_mood_denial_threshold_not_met() {
  // Fewer than 3 total decisions → no penalty applied regardless of ratio
  _stats.approvals = 0;
  _stats.denials   = 2;
  TEST_ASSERT_EQUAL_UINT8(2, statsMoodTier());  // neutral, no penalty
}

void test_mood_tier_floor_zero() {
  uint16_t v[] = { 200, 200, 200 };
  push_velocity(v, 3);
  _stats.approvals = 1;
  _stats.denials   = 10;  // d > a → -2; base 0, 0-2 clamped to 0
  TEST_ASSERT_EQUAL_UINT8(0, statsMoodTier());
}

// ── statsEnergyTier ───────────────────────────────────────────────────────────

void test_energy_boot_default() {
  // _energyAtNap=3, no time elapsed
  TEST_ASSERT_EQUAL_UINT8(3, statsEnergyTier());
}

void test_energy_after_wake() {
  statsOnWake();
  // _energyAtNap=5, millis=0, hoursSince=0, e=5
  TEST_ASSERT_EQUAL_UINT8(5, statsEnergyTier());
}

void test_energy_drains_per_2h() {
  statsOnWake();
  _mock_millis = 2UL * 3600000UL;  // 2 hours later
  TEST_ASSERT_EQUAL_UINT8(4, statsEnergyTier());

  _mock_millis = 4UL * 3600000UL;
  TEST_ASSERT_EQUAL_UINT8(3, statsEnergyTier());

  _mock_millis = 10UL * 3600000UL;
  TEST_ASSERT_EQUAL_UINT8(0, statsEnergyTier());
}

void test_energy_floor_zero_no_usb() {
  // Long time since nap, no USB — should reach 0
  _mock_millis = 20UL * 3600000UL;
  TEST_ASSERT_EQUAL_UINT8(0, statsEnergyTier());
}

// ── statsFedProgress ─────────────────────────────────────────────────────────

void test_fed_zero_tokens() {
  TEST_ASSERT_EQUAL_UINT8(0, statsFedProgress());
}

void test_fed_one_pip() {
  _stats.tokens = 5000;
  TEST_ASSERT_EQUAL_UINT8(1, statsFedProgress());
}

void test_fed_max_before_levelup() {
  _stats.tokens = 49999;
  TEST_ASSERT_EQUAL_UINT8(9, statsFedProgress());
}

void test_fed_resets_on_levelup() {
  _stats.tokens = 50000;   // exactly one level → 0 progress into next
  TEST_ASSERT_EQUAL_UINT8(0, statsFedProgress());
}

void test_fed_second_level_progress() {
  _stats.tokens = 55000;   // 5K into level 2
  TEST_ASSERT_EQUAL_UINT8(1, statsFedProgress());
}

// ── statsOnBridgeTokens delta logic ──────────────────────────────────────────

void test_bridge_tokens_first_packet_latched() {
  // First packet should not credit tokens — it establishes the baseline.
  statsOnBridgeTokens(10000);
  TEST_ASSERT_EQUAL_UINT32(0, _stats.tokens);
}

void test_bridge_tokens_delta_credited() {
  statsOnBridgeTokens(10000);   // baseline
  statsOnBridgeTokens(15000);   // +5000
  TEST_ASSERT_EQUAL_UINT32(5000, _stats.tokens);
}

void test_bridge_tokens_restart_resyncs() {
  statsOnBridgeTokens(10000);
  statsOnBridgeTokens(15000);   // +5000 credited
  statsOnBridgeTokens(3000);    // drop = bridge restart, resync, no credit
  TEST_ASSERT_EQUAL_UINT32(5000, _stats.tokens);
  statsOnBridgeTokens(8000);    // +5000 from new baseline of 3000
  TEST_ASSERT_EQUAL_UINT32(10000, _stats.tokens);
}

void test_bridge_tokens_level_up() {
  statsOnBridgeTokens(0);         // baseline
  statsOnBridgeTokens(60000);     // +60K → crosses level 1
  TEST_ASSERT_EQUAL_UINT8(1, _stats.level);
  TEST_ASSERT_TRUE(statsPollLevelUp());
  TEST_ASSERT_FALSE(statsPollLevelUp());  // flag clears after poll
}

// ── runner ───────────────────────────────────────────────────────────────────

void setUp()    { reset_state(); }
void tearDown() {}

int main(int argc, char** argv) {
  UNITY_BEGIN();

  RUN_TEST(test_median_empty);
  RUN_TEST(test_median_single);
  RUN_TEST(test_median_odd_count);
  RUN_TEST(test_median_even_count_upper);
  RUN_TEST(test_median_full_buffer);
  RUN_TEST(test_median_ring_wraps);

  RUN_TEST(test_mood_no_data);
  RUN_TEST(test_mood_fast_responses);
  RUN_TEST(test_mood_slow_responses);
  RUN_TEST(test_mood_denial_penalty_heavy);
  RUN_TEST(test_mood_denial_penalty_light);
  RUN_TEST(test_mood_denial_penalty_moderate);
  RUN_TEST(test_mood_denial_threshold_not_met);
  RUN_TEST(test_mood_tier_floor_zero);

  RUN_TEST(test_energy_boot_default);
  RUN_TEST(test_energy_after_wake);
  RUN_TEST(test_energy_drains_per_2h);
  RUN_TEST(test_energy_floor_zero_no_usb);

  RUN_TEST(test_fed_zero_tokens);
  RUN_TEST(test_fed_one_pip);
  RUN_TEST(test_fed_max_before_levelup);
  RUN_TEST(test_fed_resets_on_levelup);
  RUN_TEST(test_fed_second_level_progress);

  RUN_TEST(test_bridge_tokens_first_packet_latched);
  RUN_TEST(test_bridge_tokens_delta_credited);
  RUN_TEST(test_bridge_tokens_restart_resyncs);
  RUN_TEST(test_bridge_tokens_level_up);

  return UNITY_END();
}

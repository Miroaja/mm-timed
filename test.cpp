#include "timed.hpp"
#include <cstdint>
#include <print>
using namespace mm::timed;
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

using namespace std::chrono;

using duration_t = std::chrono::duration<int64_t, std::nano>;

// -------------------------
// helpers
// -------------------------
bool approx(duration_t value, duration_t target, duration_t tolerance) {
  return (value > target - tolerance) && (value < target + tolerance);
}

auto print_stats(const std::vector<duration_t> &samples, duration_t expected) {
  if (samples.empty())
    return duration_t::zero();

  auto min = *std::min_element(samples.begin(), samples.end());
  auto max = *std::max_element(samples.begin(), samples.end());
  auto sum = std::accumulate(samples.begin(), samples.end(), duration_t(0ms));
  auto avg = sum / samples.size();

  std::cout << "  samples: " << samples.size() << "\n";
  std::cout << "  min: " << min << std::endl;
  std::cout << "  avg: " << avg << std::endl;
  std::cout << "  max: " << max << std::endl;
  std::cout << "  expected: " << expected << std::endl;

  return duration_t(avg);
}

// -------------------------
// TEST 1: single blocking
// -------------------------
void test_single_blocking() {
  std::cout << "\n[TEST] single blocking\n";

  every<high_resolution_clock, duration_t> e(duration_t(50ms));

  for (int i = 0; i < 5; ++i) {
    auto start = high_resolution_clock::now();

    e.run([&](auto dt) {
      auto dt_ms = duration_cast<duration_t>(dt);
      std::cout << "  run " << i << " dt=" << dt_ms << std::endl;
      assert(dt_ms >= duration_t(45ms));
    });

    auto elapsed =
        duration_cast<duration_t>(high_resolution_clock::now() - start);
    std::cout << "  elapsed=" << elapsed << std::endl;
  }
}

// -------------------------
// TEST 2: single non-blocking
// -------------------------
void test_single_non_blocking() {
  std::cout << "\n[TEST] single non-blocking\n";

  every<high_resolution_clock, duration_t> e(duration_t(30ms));

  int hits = 0;
  auto start = high_resolution_clock::now();

  while (hits < 5) {
    if (e.run(
            [&](auto dt) {
              auto dt_ms = duration_cast<duration_t>(dt);
              std::cout << "  hit dt=" << dt_ms << std::endl;
              ++hits;
            },
            no_sync)) {
    }
  }

  auto elapsed =
      duration_cast<duration_t>(high_resolution_clock::now() - start);
  std::cout << "  total elapsed=" << elapsed << std::endl;

  assert(hits == 5);
}

// -------------------------
// TEST 3: long blocking loop
// -------------------------
void test_loop_blocking_long() {
  std::cout << "\n[TEST] loop blocking (long)\n";

  every<high_resolution_clock, duration_t> e(duration_t(20ms));
  std::vector<duration_t> deltas;

  int count = 0;
  e.run(
      [&](auto dt, bool running_short) {
        auto dt_ms = duration_cast<duration_t>(dt);
        deltas.push_back(dt_ms);
        ++count;

        if (running_short) {
          std::println("uh oh");
        }
        if (count % 20 == 0) {
          std::cout << "  iteration " << count << "\n";
        }

        return count < 200;
      },
      loop);

  auto avg = print_stats(deltas, duration_t(20ms));

  assert(approx(avg, duration_t(20ms), duration_t(1ms)));
}

// -------------------------
// TEST 4: loop no_sync (long)
// -------------------------
void test_loop_no_sync_long() {
  std::cout << "\n[TEST] loop no_sync (long)\n";

  every<high_resolution_clock, duration_t> e(duration_t(10ms));
  int count = 0;

  auto start = high_resolution_clock::now();

  e.run(
      [&](auto dt, bool) {
        auto dt_ms = duration_cast<duration_t>(dt);
        ++count;

        if (count % 50 == 0) {
          std::cout << "  iteration " << count << " dt=" << dt_ms.count()
                    << " ms\n";
        }

        return count < 300;
      },
      loop, no_sync);

  auto elapsed =
      duration_cast<duration_t>(high_resolution_clock::now() - start);

  std::cout << "  total elapsed=" << elapsed.count() << " ms\n";

  assert(count == 300);
}

// -------------------------
// TEST 5: run_over extended
// -------------------------
void test_run_over_long() {
  std::cout << "\n[TEST] run_over (long)\n";

  every<high_resolution_clock, duration_t> e(duration_t(15ms));
  std::vector<duration_t> deltas;

  e.run_over(
      [&](auto dt, bool) {
        auto dt_ms = duration_cast<duration_t>(dt);
        deltas.push_back(dt_ms);
      },
      duration_t(500ms));

  print_stats(deltas, duration_t(15ms));

  assert(deltas.size() > 10);
}

// -------------------------
// TEST 6: drift stress
// -------------------------
void test_drift_stress() {
  std::cout << "\n[TEST] drift stress\n";

  every<high_resolution_clock, duration_t> e(duration_t(10ms));
  std::vector<high_resolution_clock::time_point> timestamps;
  std::vector<duration_t> deltas;

  e.run(
      [&](auto dt, bool) {
        deltas.push_back(dt);
        timestamps.push_back(high_resolution_clock::now());
        return timestamps.size() < 1000;
      },
      loop);

  std::vector<duration_t> diffs;

  for (size_t i = 1; i < timestamps.size(); ++i) {
    diffs.push_back(
        duration_cast<duration_t>(timestamps[i] - timestamps[i - 1]));
  }

  print_stats(diffs, duration_t(10ms));
  print_stats(deltas, duration_t(10ms));

  int spikes = 0;
  for (auto d : diffs) {
    if (d > duration_t(30ms))
      ++spikes;
  }

  std::cout << "  jitter spikes (>30ms): " << spikes << "\n";
}

// -------------------------
// TEST 7: slow callback stress
// -------------------------
void test_slow_callback_stress() {
  std::cout << "\n[TEST] slow callback stress\n";

  every<high_resolution_clock, duration_t> e(duration_t(10ms));

  int count = 0;

  e.run(
      [&](auto dt, bool) {
        auto dt_ms = duration_cast<duration_t>(dt);
        ++count;

        std::cout << "  dt=" << dt_ms.count() << " ms\n";

        std::this_thread::sleep_for(duration_t(25ms)); // intentionally slow

        return count < 20;
      },
      loop);

  assert(count == 20);
}

// -------------------------
// TEST 8: reset behavior
// -------------------------
void test_reset_behavior() {
  std::cout << "\n[TEST] reset behavior\n";

  every<high_resolution_clock, duration_t> e(duration_t(50ms));

  std::this_thread::sleep_for(duration_t(60ms));
  e.reset();

  bool triggered = e.run([](auto) {}, no_sync);

  std::cout << "  triggered after reset: " << triggered << "\n";
  assert(!triggered);
}

// -------------------------
// main
// -------------------------
int main() {
  test_single_blocking();
  test_single_non_blocking();
  test_loop_blocking_long();
  test_loop_no_sync_long();
  test_run_over_long();
  test_drift_stress();
  test_slow_callback_stress();
  test_reset_behavior();

  std::cout << "\nAll extended tests passed!\n";
}

#pragma once
#include <chrono>
#include <thread>

namespace mm {
namespace timed {
struct no_sync_t {};
inline constexpr no_sync_t no_sync;
struct loop_t {};
inline constexpr loop_t loop;

template <typename Clock = std::chrono::steady_clock,
          typename Duration = std::chrono::duration<float>>
  requires std::chrono::is_clock_v<Clock>
struct every {
  using clock_t = Clock;
  using duration_t = Duration;
  using time_point_t = typename clock_t::time_point;

  explicit every(duration_t step) noexcept
      : _step(step), _last(clock_t::now()) {}

  void reset() noexcept { _last = clock_t::now(); }

  template <typename F>
    requires requires(F &&f, duration_t dt) {
      { f(dt, true) } -> std::same_as<bool>;
    }
  void run(F &&f, loop_t) {
    bool running_short = false;

    auto dt = _step;
    while (true) {
      auto start = clock_t::now();

      if (!f(dt, running_short)) {
        return;
      }

      auto next = start + _step;
      _last = clock_t::now();
      running_short = next < _last;

      std::this_thread::sleep_until(next);
      dt = std::chrono::duration_cast<duration_t>(clock_t::now() - start);
    }
  }

  template <typename F>
    requires requires(F &&f, duration_t dt) {
      { f(dt, true) } -> std::same_as<bool>;
    }
  void run(F &&f, loop_t, no_sync_t) {
    bool running_short = false;
    while (true) {
      auto now = clock_t::now();

      if (now - _last >= _step) {
        auto dt = now - _last;

        if (!f(std::chrono::duration_cast<duration_t>(dt), running_short)) {
          return;
        }

        auto next = _last + _step;
        _last = clock_t::now();
        running_short = next < _last;
      } else {
        std::this_thread::yield();
      }
    }
  }

  template <typename F>
    requires requires(F &&f, duration_t dt) {
      { f(dt) } -> std::same_as<void>;
    }
  void run(F &&f) {
    auto next =
        _last + std::chrono::duration_cast<typename clock_t::duration>(_step);
    auto now = clock_t::now();

    std::this_thread::sleep_until(next);

    now = clock_t::now();
    auto dt = now - _last;

    f(std::chrono::duration_cast<duration_t>(dt));
    _last = next;
  }

  template <typename F>
    requires requires(F &&f, duration_t dt) {
      { f(dt) } -> std::same_as<void>;
    }
  bool run(F &&f, no_sync_t) {
    auto now = clock_t::now();

    if (now - _last >= _step) {
      auto dt = now - _last;
      f(std::chrono::duration_cast<duration_t>(dt));
      _last = clock_t::now();
      return true;
    }
    return false;
  }

  template <typename F>
    requires requires(F &&f, duration_t dt) {
      { f(dt, true) } -> std::same_as<void>;
    }
  void run_over(F &&f, duration_t over) {
    auto start = clock_t::now();
    auto end = start + over;
    _last = start;
    bool running_short = false;

    auto dt = _step;
    while (clock_t::now() < end) {
      auto start = clock_t::now();
      auto next = start + _step;

      f(dt, running_short);
      _last = clock_t::now();
      running_short = next < _last;

      std::this_thread::sleep_until(next);
      dt = std::chrono::duration_cast<duration_t>(clock_t::now() - start);
    }
  }

private:
  duration_t _step;
  time_point_t _last;
};

template <typename Clock = std::chrono::steady_clock,
          typename Duration = std::chrono::duration<float>>
  requires std::chrono::is_clock_v<Clock>
struct over {};
// FEATURE: implement "over time" timers
} // namespace timed
} // namespace mm

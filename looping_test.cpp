#include "timed.hpp"
#include <chrono>
#include <print>

using namespace mm::timed;
using namespace std::chrono_literals;
int main() {
  every looper(16ms);
  every<std::chrono::high_resolution_clock, std::chrono::nanoseconds> timer(
      1ms);

  looper.run(
      [&timer](decltype(looper)::duration_t dt, bool running_short) {
        // Record the start of this loop iteration
        static auto last_time = std::chrono::high_resolution_clock::now();
        auto loop_start_time = std::chrono::high_resolution_clock::now();

        auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                              loop_start_time - last_time)
                              .count();
        std::println("Elapsed since last frame start: {} ns (dt: {})",
                     elapsed_ns, dt);

        last_time = loop_start_time;

        if (running_short) {
          std::println("running_short (outer)");
        }
        timer.run_over(
            [](decltype(timer)::duration_t, bool running_short) {
              if (running_short) {
                std::println("running_short (inner)");
              }
              // std::this_thread::sleep_for(10000ns);
            },
            16ms);
        return true;
      },
      loop);
}

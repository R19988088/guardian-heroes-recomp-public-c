#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

namespace gh::thread {

// Yield the current host thread and publish prior writes to other threads.
// This covers the small scheduler-yield boundary used by GPU fence polling.
void YieldCurrentThread() noexcept;

// Project-owned critical region used by generated guest code. The runtime
// adapter binds this facade to ReXGlue's singleton before runtime setup; the
// standalone implementation remains available for native-only tests.
class GlobalCriticalRegion {
 public:
  static std::recursive_mutex& mutex();
  static std::unique_lock<std::recursive_mutex> AcquireDirect();

  // Pass nullptr to restore the project-owned fallback mutex (used by tests).
  // Binding is performed once during application setup, before guest threads
  // are created.
  static void BindRuntimeMutex(std::recursive_mutex* runtime_mutex);

  static void Enter();
  static void Leave();
  static std::uint64_t Check();
  static std::int32_t depth();
};

}  // namespace gh::thread

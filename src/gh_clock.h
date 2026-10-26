#pragma once

#include <cstdint>

namespace gh::clock {

// Project-owned monotonic and guest time source. Guest timestamps use the
// Xbox FILETIME epoch (100 ns ticks since 1601-01-01 UTC).
uint64_t HostTickFrequency() noexcept;
uint64_t HostTickCount() noexcept;
uint64_t HostSystemTime() noexcept;

uint64_t GuestTickFrequency() noexcept;
void SetGuestTickFrequency(uint64_t frequency) noexcept;
using GuestTickFrequencySource = uint64_t (*)();
void SetGuestTickFrequencySource(GuestTickFrequencySource source) noexcept;
using GuestTickCountSource = uint64_t (*)();
// Runtime adapters may bind the authoritative guest clock. Passing nullptr
// restores the standalone project clock used by native-only builds.
void SetGuestTickCountSource(GuestTickCountSource source) noexcept;
uint64_t GuestTickCount() noexcept;
uint64_t GuestSystemTime() noexcept;
void SetGuestSystemTime(uint64_t file_time) noexcept;
uint32_t GuestUptimeMillis() noexcept;

struct GuestState {
  uint64_t frequency;
  uint64_t system_time_base;
  uint64_t system_time_anchor;
  uint64_t uptime_anchor;
};
// The runtime freezes the authoritative tick source before using these.
GuestState CaptureGuestState() noexcept;
void RestoreGuestState(const GuestState& state) noexcept;

}  // namespace gh::clock

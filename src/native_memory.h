#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

// Owns the address translation and access rules used by project native code.
// The optional size is the mapped host extent starting at base, including any
// platform-specific physical-memory padding.
class GuestMemory {
 public:
  struct MmioHandler {
    void* context = nullptr;
    uint32_t (*load32)(void* context, uint32_t address) = nullptr;
    void (*store32)(void* context, uint32_t address, uint32_t value) = nullptr;
  };

  static void SetGlobalMmioHandler(MmioHandler* handler) noexcept {
    global_mmio_handler_.store(handler, std::memory_order_release);
  }

  static MmioHandler* GlobalMmioHandler() noexcept {
    return global_mmio_handler_.load(std::memory_order_acquire);
  }

  static GuestMemory WithGlobalMmio(uint8_t* base) noexcept {
    return GuestMemory(base, 0, GlobalMmioHandler());
  }

  explicit GuestMemory(uint8_t* base, size_t mapped_size = 0,
                       MmioHandler* mmio = nullptr) noexcept
      : base_(base), mapped_size_(mapped_size), mmio_(mmio) {}

  static constexpr uint32_t kMmioBegin = 0x7F000000u;
  static constexpr uint32_t kMmioEnd = 0x80000000u;

  static constexpr bool IsMmioAddress(uint32_t address) noexcept {
    return address >= kMmioBegin && address < kMmioEnd;
  }

  static constexpr uint32_t HostOffset(uint32_t address) noexcept {
#if defined(_WIN32) || (defined(__APPLE__) && defined(__aarch64__))
    return address >= 0xE0000000u ? 0x1000u : 0u;
#else
    (void)address;
    return 0;
#endif
  }

  // Returns the mapped host address. Raw spans are used only after the caller
  // has validated a guest range, matching the existing native bulk-copy paths.
  uint8_t* Address(uint32_t address) const {
    return Address(address, 1);
  }

  // Returns a pointer for a contiguous raw span after validating the complete
  // guest range. Scalar accesses can continue to use the single-byte form;
  // bulk callers must validate the full copy before touching host memory.
  uint8_t* Address(uint32_t address, size_t size) const {
    CheckRange(address, size);
    return base_ + uint64_t(address) + HostOffset(address);
  }

  bool Contains(uint32_t address, size_t size) const noexcept {
    const uint64_t guest_end = uint64_t(address) + size;
    if (guest_end > (uint64_t{1} << 32)) return false;
    const uint64_t host_begin = uint64_t(address) + HostOffset(address);
    const uint64_t host_end = host_begin + size;
    if (host_end < host_begin) return false;
    return !mapped_size_ || host_end <= mapped_size_;
  }

  uint8_t Read8(uint32_t address) const {
    CheckRange(address, 1);
    if (IsMmioAddress(address) && mmio_ && mmio_->load32)
      return static_cast<uint8_t>(LoadMmio(address));
    return *reinterpret_cast<volatile const uint8_t*>(Address(address));
  }

  uint8_t ReadRam8(uint32_t address) const {
    if (mapped_size_) CheckRange(address, 1);
    return *reinterpret_cast<volatile const uint8_t*>(RamAddress(address));
  }

  uint16_t ReadRam16(uint32_t address) const {
    if (mapped_size_) CheckRange(address, 2);
    return __builtin_bswap16(*reinterpret_cast<volatile const uint16_t*>(RamAddress(address)));
  }

  uint32_t ReadRam32(uint32_t address) const {
    if (mapped_size_) CheckRange(address, 4);
    return __builtin_bswap32(*reinterpret_cast<volatile const uint32_t*>(RamAddress(address)));
  }

  uint64_t ReadRam64(uint32_t address) const {
    if (mapped_size_) CheckRange(address, 8);
    return __builtin_bswap64(*reinterpret_cast<volatile const uint64_t*>(RamAddress(address)));
  }

  uint16_t Read16(uint32_t address) const {
    CheckRange(address, 2);
    if (IsMmioAddress(address) && mmio_ && mmio_->load32)
      return static_cast<uint16_t>(LoadMmio(address));
    uint16_t value = 0;
    for (unsigned i = 0; i < 2; ++i) value = uint16_t((value << 8) | ReadRamByte(address + i));
    return value;
  }

  uint32_t Read32(uint32_t address) const {
    CheckRange(address, 4);
    if (IsMmioAddress(address) && mmio_ && mmio_->load32) return LoadMmio(address);
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value = (value << 8) | ReadRamByte(address + i);
    return value;
  }

  uint64_t Read64(uint32_t address) const {
    CheckRange(address, 8);
    if (IsMmioAddress(address) && mmio_ && mmio_->load32)
      return (uint64_t(LoadMmio(address)) << 32) | LoadMmio(address + 4);
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value = (value << 8) | ReadRamByte(address + i);
    return value;
  }

  void Write8(uint32_t address, uint8_t value) const {
    CheckRange(address, 1);
    if (IsMmioAddress(address) && mmio_ && mmio_->store32) {
      StoreMmio(address, value);
      return;
    }
    WriteRamByte(address, value);
  }

  void WriteRam8(uint32_t address, uint8_t value) const {
    if (mapped_size_) CheckRange(address, 1);
    *reinterpret_cast<volatile uint8_t*>(RamAddress(address)) = value;
  }

  void WriteRam16(uint32_t address, uint16_t value) const {
    if (mapped_size_) CheckRange(address, 2);
    *reinterpret_cast<volatile uint16_t*>(RamAddress(address)) = __builtin_bswap16(value);
  }

  void WriteRam32(uint32_t address, uint32_t value) const {
    if (mapped_size_) CheckRange(address, 4);
    *reinterpret_cast<volatile uint32_t*>(RamAddress(address)) = __builtin_bswap32(value);
  }

  void WriteRam64(uint32_t address, uint64_t value) const {
    if (mapped_size_) CheckRange(address, 8);
    *reinterpret_cast<volatile uint64_t*>(RamAddress(address)) = __builtin_bswap64(value);
  }

  // The generated MMIO byte/halfword store macros historically forwarded the
  // full source register to CheckStore; retain that behavior at the adapter.
  void WriteMmio8(uint32_t address, uint32_t value) const {
    if (IsMmioAddress(address) && mmio_ && mmio_->store32) {
      StoreMmio(address, value);
      return;
    }
    WriteRam8(address, static_cast<uint8_t>(value));
  }

  void WriteMmio16(uint32_t address, uint32_t value) const {
    if (IsMmioAddress(address) && mmio_ && mmio_->store32) {
      StoreMmio(address, value);
      return;
    }
    WriteRam16(address, static_cast<uint16_t>(value));
  }

  void Write16(uint32_t address, uint16_t value) const {
    CheckRange(address, 2);
    if (IsMmioAddress(address) && mmio_ && mmio_->store32) {
      StoreMmio(address, value);
      return;
    }
    for (unsigned i = 0; i < 2; ++i)
      WriteRamByte(address + i, uint8_t(value >> ((1 - i) * 8)));
  }

  void Write32(uint32_t address, uint32_t value) const {
    CheckRange(address, 4);
    if (IsMmioAddress(address) && mmio_ && mmio_->store32) {
      StoreMmio(address, value);
      return;
    }
    for (unsigned i = 0; i < 4; ++i)
      WriteRamByte(address + i, uint8_t(value >> ((3 - i) * 8)));
  }

  void Write64(uint32_t address, uint64_t value) const {
    CheckRange(address, 8);
    if (IsMmioAddress(address) && mmio_ && mmio_->store32) {
      StoreMmio(address, uint32_t(value >> 32));
      StoreMmio(address + 4, uint32_t(value));
      return;
    }
    for (unsigned i = 0; i < 8; ++i)
      WriteRamByte(address + i, uint8_t(value >> ((7 - i) * 8)));
  }

 private:
  uint8_t* base_;
  size_t mapped_size_;
  MmioHandler* mmio_;
  inline static std::atomic<MmioHandler*> global_mmio_handler_{nullptr};

  void CheckRange(uint32_t address, size_t size) const {
    if (!base_ || !Contains(address, size)) throw std::out_of_range("guest memory address out of range");
  }

  uint8_t* RamAddress(uint32_t address) const noexcept {
    return base_ + uint64_t(address) + HostOffset(address);
  }

  uint8_t ReadRamByte(uint32_t address) const {
    return *reinterpret_cast<volatile const uint8_t*>(RamAddress(address));
  }

  void WriteRamByte(uint32_t address, uint8_t value) const {
    *reinterpret_cast<volatile uint8_t*>(RamAddress(address)) = value;
  }

  uint32_t LoadMmio(uint32_t address) const {
    return mmio_->load32(mmio_->context, address);
  }

  void StoreMmio(uint32_t address, uint32_t value) const {
    mmio_->store32(mmio_->context, address, value);
  }
};

namespace {
// Physical aliases and MMIO keep the guest's original access order.
inline bool NativeMemoryRange(uint32_t address, uint32_t size) {
  const uint64_t end = uint64_t(address) + size;
  return (address < GuestMemory::kMmioBegin && end <= GuestMemory::kMmioBegin) ||
         (address >= GuestMemory::kMmioEnd && end <= 0xA0000000ull);
}

inline bool NativeCopyRange(uint32_t destination, uint32_t source, uint32_t size) {
  return NativeMemoryRange(destination, size) && NativeMemoryRange(source, size) &&
         (uint64_t(destination) + size <= source ||
          uint64_t(source) + size <= destination);
}
}  // namespace

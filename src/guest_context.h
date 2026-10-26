#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#if __has_include(<simde/x86/sse.h>)
#include <simde/x86/sse2.h>
#define GUEST_CONTEXT_HAS_SIMDE 1
#endif

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

// This is the small, project-owned guest ABI used by native replacements.
// Its field order intentionally matches the generated PPC context so the C
// entry points remain ABI-compatible while the native layer stays standalone.
union GuestRegister {
  int8_t s8;
  uint8_t u8;
  int16_t s16;
  uint16_t u16;
  int32_t s32;
  uint32_t u32;
  int64_t s64;
  uint64_t u64;
  float f32;
  double f64;
};

struct GuestXERRegister {
  uint8_t so;
  uint8_t ov;
  uint8_t ca;
};

struct GuestCRRegister {
  uint8_t lt;
  uint8_t gt;
  uint8_t eq;
  union {
    uint8_t so;
    uint8_t un;
  };

  uint32_t raw() const noexcept { return (lt << 3) | (gt << 2) | (eq << 1) | so; }
  void set_raw(uint32_t value) noexcept {
    lt = (value >> 3) & 1;
    gt = (value >> 2) & 1;
    eq = (value >> 1) & 1;
    so = value & 1;
  }

  template <typename T>
  void compare(T left, T right, const GuestXERRegister& xer) noexcept {
    lt = left < right;
    gt = left > right;
    eq = left == right;
    so = xer.so;
  }

  void compare(double left, double right) noexcept {
    un = __builtin_isnan(left) || __builtin_isnan(right);
    lt = !un && (left < right);
    gt = !un && (left > right);
    eq = !un && (left == right);
  }

#if defined(GUEST_CONTEXT_HAS_SIMDE)
  void setFromMask(simde__m128 mask, int imm) noexcept {
    const int bits = simde_mm_movemask_ps(mask);
    lt = bits == imm;
    gt = 0;
    eq = bits == 0;
    so = 0;
  }

  void setFromMask(simde__m128i mask, int imm) noexcept {
    const int bits = simde_mm_movemask_epi8(mask);
    lt = bits == imm;
    gt = 0;
    eq = bits == 0;
    so = 0;
  }
#endif
};

struct GuestFPSCRRegister {
  uint32_t csr;

  static constexpr uint32_t FlushMask =
#if defined(__aarch64__) || defined(_M_ARM64)
      (1u << 19) | (1u << 24);
#else
      0x0040u | 0x8000u;
#endif

  static uint32_t getHostCSR() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    return _mm_getcsr();
#elif defined(__aarch64__) || defined(_M_ARM64)
    uint64_t value;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(value));
    return static_cast<uint32_t>(value);
#else
    return 0;
#endif
  }

  static void setHostCSR(uint32_t value) noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_setcsr(value);
#elif defined(__aarch64__) || defined(_M_ARM64)
    __asm__ __volatile__("msr fpcr, %0" : : "r"(uint64_t(value)));
#else
    (void)value;
#endif
  }

  uint32_t getcsr() const noexcept { return getHostCSR(); }
  void setcsr(uint32_t value) noexcept { setHostCSR(value); }
  void InitHost() noexcept {
    csr = getHostCSR();
#if defined(__x86_64__) || defined(_M_X64)
    csr |= 0x1F80u;
#elif defined(__aarch64__) || defined(_M_ARM64)
    csr &= ~((0x1Fu << 8) | (1u << 15));
#endif
    setHostCSR(csr);
  }

  void disableFlushMode() noexcept {
    if ((csr & FlushMask) != 0) {
      csr &= ~FlushMask;
      setHostCSR(csr);
    }
  }
  void enableFlushMode() noexcept {
    if ((csr & FlushMask) != FlushMask) {
      csr |= FlushMask;
      setHostCSR(csr);
    }
  }
  void disableFlushModeUnconditional() noexcept {
    csr &= ~FlushMask;
    setHostCSR(csr);
  }

#if defined(__aarch64__) || defined(_M_ARM64)
  static constexpr uint32_t RoundShift = 22;
  static constexpr uint32_t RoundMask = 3u << RoundShift;
  static constexpr uint32_t GuestToHostRound[4] = {0u << RoundShift, 3u << RoundShift,
                                                    1u << RoundShift, 2u << RoundShift};
#else
  static constexpr uint32_t RoundShift = 13;
  static constexpr uint32_t RoundMask = 0x6000u;
  static constexpr uint32_t GuestToHostRound[4] = {0x0000u, 0x6000u, 0x2000u, 0x4000u};
#endif

  uint32_t loadFromHost() noexcept {
    static constexpr uint32_t HostToGuestRound[4] = {0u, 3u, 2u, 1u};
    csr = getHostCSR();
    return HostToGuestRound[(csr & RoundMask) >> RoundShift];
  }

  void storeFromGuest(uint32_t value) noexcept {
    csr = (csr & ~RoundMask) | GuestToHostRound[value & 3u];
    setHostCSR(csr);
  }
  void restoreGuestBits(uint32_t saved) noexcept {
    constexpr uint32_t guest_mask = FlushMask | RoundMask;
    csr = (getHostCSR() & ~guest_mask) | (saved & guest_mask);
    setHostCSR(csr);
  }
  void enableFlushModeUnconditional() noexcept {
    csr |= FlushMask;
    setHostCSR(csr);
  }
};

union alignas(0x10) GuestVectorRegister {
  int8_t s8[16];
  uint8_t u8[16];
  int16_t s16[8];
  uint16_t u16[8];
  int32_t s32[4];
  uint32_t u32[4];
  int64_t s64[2];
  uint64_t u64[2];
  float f32[4];
  double f64[2];
};

struct alignas(0x40) GuestContext {
  GuestRegister r3, r0, r1, r2;
  GuestRegister r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15;
  GuestRegister r16, r17, r18, r19, r20, r21, r22, r23, r24, r25, r26, r27;
  GuestRegister r28, r29, r30, r31;

  uint64_t lr;
  GuestRegister ctr;
  GuestXERRegister xer;
  GuestRegister reserved;
  uint32_t msr = 0x200A000;
  GuestCRRegister cr0, cr1, cr2, cr3, cr4, cr5, cr6, cr7;
  GuestFPSCRRegister fpscr;
  uint8_t vscr_sat = 0;
  uint32_t last_indirect_target = 0;

  GuestRegister f0, f1, f2, f3, f4, f5, f6, f7;
  GuestRegister f8, f9, f10, f11, f12, f13, f14, f15;
  GuestRegister f16, f17, f18, f19, f20, f21, f22, f23;
  GuestRegister f24, f25, f26, f27, f28, f29, f30, f31;

#define GUEST_VECTOR_REGISTER(n) GuestVectorRegister v##n;
  GUEST_VECTOR_REGISTER(0) GUEST_VECTOR_REGISTER(1) GUEST_VECTOR_REGISTER(2)
  GUEST_VECTOR_REGISTER(3) GUEST_VECTOR_REGISTER(4) GUEST_VECTOR_REGISTER(5)
  GUEST_VECTOR_REGISTER(6) GUEST_VECTOR_REGISTER(7) GUEST_VECTOR_REGISTER(8)
  GUEST_VECTOR_REGISTER(9) GUEST_VECTOR_REGISTER(10) GUEST_VECTOR_REGISTER(11)
  GUEST_VECTOR_REGISTER(12) GUEST_VECTOR_REGISTER(13) GUEST_VECTOR_REGISTER(14)
  GUEST_VECTOR_REGISTER(15) GUEST_VECTOR_REGISTER(16) GUEST_VECTOR_REGISTER(17)
  GUEST_VECTOR_REGISTER(18) GUEST_VECTOR_REGISTER(19) GUEST_VECTOR_REGISTER(20)
  GUEST_VECTOR_REGISTER(21) GUEST_VECTOR_REGISTER(22) GUEST_VECTOR_REGISTER(23)
  GUEST_VECTOR_REGISTER(24) GUEST_VECTOR_REGISTER(25) GUEST_VECTOR_REGISTER(26)
  GUEST_VECTOR_REGISTER(27) GUEST_VECTOR_REGISTER(28) GUEST_VECTOR_REGISTER(29)
  GUEST_VECTOR_REGISTER(30) GUEST_VECTOR_REGISTER(31) GUEST_VECTOR_REGISTER(32)
  GUEST_VECTOR_REGISTER(33) GUEST_VECTOR_REGISTER(34) GUEST_VECTOR_REGISTER(35)
  GUEST_VECTOR_REGISTER(36) GUEST_VECTOR_REGISTER(37) GUEST_VECTOR_REGISTER(38)
  GUEST_VECTOR_REGISTER(39) GUEST_VECTOR_REGISTER(40) GUEST_VECTOR_REGISTER(41)
  GUEST_VECTOR_REGISTER(42) GUEST_VECTOR_REGISTER(43) GUEST_VECTOR_REGISTER(44)
  GUEST_VECTOR_REGISTER(45) GUEST_VECTOR_REGISTER(46) GUEST_VECTOR_REGISTER(47)
  GUEST_VECTOR_REGISTER(48) GUEST_VECTOR_REGISTER(49) GUEST_VECTOR_REGISTER(50)
  GUEST_VECTOR_REGISTER(51) GUEST_VECTOR_REGISTER(52) GUEST_VECTOR_REGISTER(53)
  GUEST_VECTOR_REGISTER(54) GUEST_VECTOR_REGISTER(55) GUEST_VECTOR_REGISTER(56)
  GUEST_VECTOR_REGISTER(57) GUEST_VECTOR_REGISTER(58) GUEST_VECTOR_REGISTER(59)
  GUEST_VECTOR_REGISTER(60) GUEST_VECTOR_REGISTER(61) GUEST_VECTOR_REGISTER(62)
  GUEST_VECTOR_REGISTER(63) GUEST_VECTOR_REGISTER(64) GUEST_VECTOR_REGISTER(65)
  GUEST_VECTOR_REGISTER(66) GUEST_VECTOR_REGISTER(67) GUEST_VECTOR_REGISTER(68)
  GUEST_VECTOR_REGISTER(69) GUEST_VECTOR_REGISTER(70) GUEST_VECTOR_REGISTER(71)
  GUEST_VECTOR_REGISTER(72) GUEST_VECTOR_REGISTER(73) GUEST_VECTOR_REGISTER(74)
  GUEST_VECTOR_REGISTER(75) GUEST_VECTOR_REGISTER(76) GUEST_VECTOR_REGISTER(77)
  GUEST_VECTOR_REGISTER(78) GUEST_VECTOR_REGISTER(79) GUEST_VECTOR_REGISTER(80)
  GUEST_VECTOR_REGISTER(81) GUEST_VECTOR_REGISTER(82) GUEST_VECTOR_REGISTER(83)
  GUEST_VECTOR_REGISTER(84) GUEST_VECTOR_REGISTER(85) GUEST_VECTOR_REGISTER(86)
  GUEST_VECTOR_REGISTER(87) GUEST_VECTOR_REGISTER(88) GUEST_VECTOR_REGISTER(89)
  GUEST_VECTOR_REGISTER(90) GUEST_VECTOR_REGISTER(91) GUEST_VECTOR_REGISTER(92)
  GUEST_VECTOR_REGISTER(93) GUEST_VECTOR_REGISTER(94) GUEST_VECTOR_REGISTER(95)
  GUEST_VECTOR_REGISTER(96) GUEST_VECTOR_REGISTER(97) GUEST_VECTOR_REGISTER(98)
  GUEST_VECTOR_REGISTER(99) GUEST_VECTOR_REGISTER(100) GUEST_VECTOR_REGISTER(101)
  GUEST_VECTOR_REGISTER(102) GUEST_VECTOR_REGISTER(103) GUEST_VECTOR_REGISTER(104)
  GUEST_VECTOR_REGISTER(105) GUEST_VECTOR_REGISTER(106) GUEST_VECTOR_REGISTER(107)
  GUEST_VECTOR_REGISTER(108) GUEST_VECTOR_REGISTER(109) GUEST_VECTOR_REGISTER(110)
  GUEST_VECTOR_REGISTER(111) GUEST_VECTOR_REGISTER(112) GUEST_VECTOR_REGISTER(113)
  GUEST_VECTOR_REGISTER(114) GUEST_VECTOR_REGISTER(115) GUEST_VECTOR_REGISTER(116)
  GUEST_VECTOR_REGISTER(117) GUEST_VECTOR_REGISTER(118) GUEST_VECTOR_REGISTER(119)
  GUEST_VECTOR_REGISTER(120) GUEST_VECTOR_REGISTER(121) GUEST_VECTOR_REGISTER(122)
  GUEST_VECTOR_REGISTER(123) GUEST_VECTOR_REGISTER(124) GUEST_VECTOR_REGISTER(125)
  GUEST_VECTOR_REGISTER(126) GUEST_VECTOR_REGISTER(127)
#undef GUEST_VECTOR_REGISTER

  static constexpr size_t kNonVolatileSaveSize =
      18 * sizeof(GuestRegister) + 18 * sizeof(GuestRegister) +
      18 * sizeof(GuestVectorRegister) + 64 * sizeof(GuestVectorRegister) +
      3 * sizeof(GuestCRRegister) + sizeof(GuestFPSCRRegister);

  void SaveNonVolatiles(uint8_t* destination) const noexcept {
    std::memcpy(destination, &r14, 18 * sizeof(GuestRegister));
    destination += 18 * sizeof(GuestRegister);
    std::memcpy(destination, &f14, 18 * sizeof(GuestRegister));
    destination += 18 * sizeof(GuestRegister);
    std::memcpy(destination, &v14, 18 * sizeof(GuestVectorRegister));
    destination += 18 * sizeof(GuestVectorRegister);
    std::memcpy(destination, &v64, 64 * sizeof(GuestVectorRegister));
    destination += 64 * sizeof(GuestVectorRegister);
    std::memcpy(destination, &cr2, 3 * sizeof(GuestCRRegister));
    destination += 3 * sizeof(GuestCRRegister);
    std::memcpy(destination, &fpscr, sizeof(GuestFPSCRRegister));
  }

  void RestoreNonVolatiles(const uint8_t* source) noexcept {
    std::memcpy(&r14, source, 18 * sizeof(GuestRegister));
    source += 18 * sizeof(GuestRegister);
    std::memcpy(&f14, source, 18 * sizeof(GuestRegister));
    source += 18 * sizeof(GuestRegister);
    std::memcpy(&v14, source, 18 * sizeof(GuestVectorRegister));
    source += 18 * sizeof(GuestVectorRegister);
    std::memcpy(&v64, source, 64 * sizeof(GuestVectorRegister));
    source += 64 * sizeof(GuestVectorRegister);
    std::memcpy(&cr2, source, 3 * sizeof(GuestCRRegister));
    source += 3 * sizeof(GuestCRRegister);
    uint32_t saved_fpscr = 0;
    std::memcpy(&saved_fpscr, source, sizeof(saved_fpscr));
    fpscr.restoreGuestBits(saved_fpscr);
  }
};

static_assert(alignof(GuestContext) == 0x40);
static_assert(sizeof(GuestContext) == 0xA80);

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "guest_context.h"
#include "guest_dispatch.h"

namespace guest {

// ReXGlue owns the public PPCFunc ABI. Generated/project guest functions use
// GuestContext internally; this boundary copies the complete layout in and out.
template <auto Function>
inline void ReXGuestEntry(ReXRuntimeContext& runtime_context, uint8_t* base) {
  static_assert(std::is_trivially_copyable_v<ReXRuntimeContext>);
  static_assert(std::is_trivially_copyable_v<GuestContext>);
  static_assert(sizeof(ReXRuntimeContext) == sizeof(GuestContext));
  static_assert(alignof(ReXRuntimeContext) == alignof(GuestContext));

  if constexpr (std::is_same_v<ReXRuntimeContext, GuestContext>) {
    Function(runtime_context, base);
    return;
  }

  GuestContext guest_context;
  std::memcpy(&guest_context, &runtime_context, sizeof(guest_context));
  Function(guest_context, base);
  std::memcpy(&runtime_context, &guest_context, sizeof(runtime_context));
}

template <typename Fallback>
inline void DispatchReXIndirect(GuestContext& context, uint8_t* base, uint32_t address,
                                Fallback&& fallback) {
  auto& registry = FunctionRegistry<GuestContext>::Instance();
  if (const auto entry = registry.Lookup(address); entry != nullptr) {
    entry(context, base);
    return;
  }
  if constexpr (!std::is_same_v<std::decay_t<Fallback>, std::nullptr_t>) {
    if (auto* runtime_entry = fallback(address); runtime_entry != nullptr) {
      ReXRuntimeContext runtime_context;
      std::memcpy(&runtime_context, &context, sizeof(context));
      runtime_entry(runtime_context, base);
      std::memcpy(&context, &runtime_context, sizeof(context));
      return;
    }
  }
  RaiseInvalidFunctionTrap(address, context);
}

}  // namespace guest

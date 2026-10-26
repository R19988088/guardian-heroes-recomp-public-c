#pragma once

#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace guest {

// A generated entry point always has the same two arguments. Keeping the
// table templated lets the native ABI use the project-owned context
// without making the registry depend on ReXGlue headers.
template <typename Context>
using Function = void (*)(Context&, uint8_t*);

class InvalidFunctionTrap final : public std::runtime_error {
 public:
  InvalidFunctionTrap(uint32_t address, uint32_t lr, uint32_t ctr, uint32_t r1,
                      uint32_t r3, uint32_t r4)
      : std::runtime_error(Format(address, lr, ctr, r1, r3, r4)),
        address_(address) {}

  uint32_t address() const noexcept { return address_; }

 private:
  static std::string Format(uint32_t address, uint32_t lr, uint32_t ctr, uint32_t r1,
                            uint32_t r3, uint32_t r4) {
    return "Call to invalid or unregistered function at guest address 0x" + Hex(address) +
           " (lr=0x" + Hex(lr) + ", ctr=0x" + Hex(ctr) + ", r1=0x" + Hex(r1) +
           ", r3=0x" + Hex(r3) + ", r4=0x" + Hex(r4) + ")";
  }

  static std::string Hex(uint32_t value) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result = "00000000";
    for (int i = 7; i >= 0; --i) {
      result[static_cast<size_t>(i)] = digits[value & 0xFu];
      value >>= 4;
    }
    return result;
  }

  uint32_t address_;
};

template <typename Context>
[[noreturn]] inline void RaiseInvalidFunctionTrap(uint32_t address, Context& context) {
  context.last_indirect_target = address;
  throw InvalidFunctionTrap(address, static_cast<uint32_t>(context.lr), context.ctr.u32,
                            context.r1.u32, context.r3.u32, context.r4.u32);
}

template <typename Context>
class FunctionRegistry {
 public:
  using Entry = Function<Context>;

  static FunctionRegistry& Instance() {
    static FunctionRegistry registry;
    return registry;
  }

  bool Register(uint32_t address, Entry entry) {
    if (entry == nullptr) {
      return false;
    }
    std::lock_guard lock(mutex_);
    functions_[address] = entry;
    return true;
  }

  Entry Lookup(uint32_t address) const {
    std::lock_guard lock(mutex_);
    const auto it = functions_.find(address);
    return it == functions_.end() ? nullptr : it->second;
  }

  size_t size() const {
    std::lock_guard lock(mutex_);
    return functions_.size();
  }

  void Dispatch(uint32_t address, Context& context, uint8_t* base) const {
    const Entry entry = Lookup(address);
    if (entry == nullptr) {
      RaiseInvalidFunctionTrap(address, context);
    }
    entry(context, base);
  }

 private:
  mutable std::mutex mutex_;
  std::unordered_map<uint32_t, Entry> functions_;
};

template <typename Context>
inline bool RegisterFunction(uint32_t address, Function<Context> entry) {
  return FunctionRegistry<Context>::Instance().Register(address, entry);
}

// Generated registration still receives a ReXGlue module registrar while the
// runtime is being migrated. The project table is authoritative; forwarding
// is retained only when the registrar exposes the legacy SetFunction method.
template <typename Registrar, typename Context>
inline bool RegisterFunction(Registrar* registrar, uint32_t address,
                             Function<Context> entry) {
  const bool registered = RegisterFunction<Context>(address, entry);
  if constexpr (requires { registrar->SetFunction(address, entry); }) {
    if (registrar != nullptr) {
      registrar->SetFunction(address, entry);
    }
  }
  return registered;
}

// Registration adapters expose both signatures: the project entry is what
// the local table dispatches, while the runtime thunk keeps the old module
// registrar ABI alive during the migration.
template <typename Registrar, typename ProjectContext, typename RuntimeContext>
inline bool RegisterFunction(Registrar* registrar, uint32_t address,
                             Function<ProjectContext> project_entry,
                             Function<RuntimeContext> runtime_entry) {
  const bool registered = RegisterFunction<ProjectContext>(address, project_entry);
  if constexpr (requires { registrar->SetFunction(address, runtime_entry); }) {
    if (registrar != nullptr) {
      registrar->SetFunction(address, runtime_entry);
    }
  }
  return registered;
}

template <typename Context, typename Fallback>
inline void DispatchIndirect(Context& context, uint8_t* base, uint32_t address,
                             Fallback&& fallback) {
  auto& registry = FunctionRegistry<Context>::Instance();
  if (const auto entry = registry.Lookup(address); entry != nullptr) {
    entry(context, base);
    return;
  }
  if constexpr (!std::is_same_v<std::decay_t<Fallback>, std::nullptr_t>) {
    if (const auto entry = fallback(address); entry != nullptr) {
      entry(context, base);
      return;
    }
  }
  RaiseInvalidFunctionTrap(address, context);
}

}  // namespace guest

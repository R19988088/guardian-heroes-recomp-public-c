#include "generated/default/gh_pch.h"
#include "cheats.h"

void GhKeepHealthMax(PPCRegister& character) {
  auto* base = rex::system::kernel_memory()->virtual_membase();
  gh::cheats::RefillHealth(base, character.u32);
}

void GhKeepSharedHealthMax(PPCRegister& character, PPCRegister& health) {
  auto* base = rex::system::kernel_memory()->virtual_membase();
  if (gh::cheats::RefillHealth(base, character.u32)) {
    health.u64 = REX_LOAD_U16(character.u32 + 202);
  }
}

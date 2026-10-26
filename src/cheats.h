#pragma once

#include <rex/ppc/context.h>

namespace gh::cheats {
void RebuildPauseMenu(PPCContext& call, uint8_t* base, uint32_t menu);
bool RefillHealth(uint8_t* base, uint32_t character);
}  // namespace gh::cheats

#pragma once

#include <rex/ppc/context.h>

void GhAppendLanguageOption(PPCRegister& stack, PPCRegister& menu);
void GhHandleLanguageOption(PPCRegister& stack, PPCRegister& menu, PPCRegister& scene);
void GhRefreshTitleOptions(PPCRegister& stack, PPCRegister& scene);
void GhRefreshPauseOptions(PPCRegister& stack, PPCRegister& scene);

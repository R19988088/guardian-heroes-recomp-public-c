#include "generated/default/gh_pch.h"

#include "cheats.h"
#include "localization.h"
#include "localization_options.h"

#include <string>
#include <string_view>

DECLARE_REX_FUNC(sub_82417CF0);
DECLARE_REX_FUNC(sub_82424CE8);
DECLARE_REX_FUNC(sub_82435C30);
DECLARE_REX_FUNC(sub_824A50F0);
DECLARE_REX_FUNC(sub_824AA5E8);
DECLARE_REX_FUNC(sub_824AAE10);
DECLARE_REX_FUNC(sub_824B3248);
DECLARE_REX_FUNC(sub_824C9B20);
DECLARE_REX_FUNC(sub_824C9D50);
DECLARE_REX_FUNC(sub_824CA1F8);
DECLARE_REX_FUNC(sub_824CBD40);
DECLARE_REX_FUNC(sub_82887F88);

namespace {

constexpr uint32_t kLanguageRow = 4;
constexpr uint32_t kRowSize = 80;

PPCContext HookContext(const PPCRegister& stack) {
  auto* current = rex::runtime::ThreadState::Get()->context();
  PPCContext call{};
  call.r1.u32 = stack.u32 - 0x5000;
  call.r13.u64 = current->r13.u64;
  call.fpscr.csr = current->fpscr.csr;
  return call;
}

void AssignText(PPCContext& call, uint8_t* base, uint32_t destination,
                uint32_t scratch, std::u16string_view text) {
  for (size_t i = 0; i < text.size(); ++i) {
    REX_STORE_U16(scratch + i * 2, static_cast<uint16_t>(text[i]));
  }
  REX_STORE_U16(scratch + text.size() * 2, 0);
  call.r3.u32 = destination;
  call.r4.u32 = scratch;
  call.r5.u32 = static_cast<uint32_t>(text.size());
  sub_82417CF0(call, base);
}

void SetLanguageText(PPCContext& call, uint8_t* base, uint32_t row,
                     uint32_t scratch) {
  AssignText(call, base, row, scratch, gh::localization::LanguageOption());
  AssignText(call, base, row + 28, scratch, gh::localization::LanguageDescription());
}

void RefreshMenuText(PPCContext& call, uint8_t* base, uint32_t menu,
                     void (*label)(PPCContext&, uint8_t*), void (*description)(PPCContext&, uint8_t*), uint32_t limit) {
  const uint32_t begin = REX_LOAD_U32(menu + 1664);
  const uint32_t end = REX_LOAD_U32(menu + 1668);
  uint32_t index = 0;
  for (uint32_t row = begin; row < end && index < limit; row += kRowSize) {
    if (REX_LOAD_U32(row + 72) == 3) continue;
    for (uint32_t field = 0; field < 2; ++field) {
      call.r3.u32 = index;
      (field == 0 ? label : description)(call, base);
      const uint32_t source = call.r3.u32;
      sub_82887F88(call, base);
      call.r5 = call.r3;
      call.r3.u32 = row + field * 28;
      call.r4.u32 = source;
      sub_82417CF0(call, base);
    }
    ++index;
  }
}

std::u16string TranslateCachedText(std::u16string_view text) {
  if (const auto translated = gh::localization::MenuTranslation(text);
      !translated.empty()) {
    return std::u16string(translated);
  }

  // Control descriptions join whole resource strings with a newline, a native
  // button glyph and a colon. Preserve those boundaries and the label indent.
  std::u16string result;
  size_t position = 0;
  while (position < text.size()) {
    const size_t boundary = text.find_first_of(u"\n\u01D3:", position);
    const size_t end = boundary == text.npos ? text.size() : boundary;
    auto part = text.substr(position, end - position);
    const auto translated = gh::localization::MenuTranslation(part);
    if (!translated.empty()) {
      result.append(translated);
    } else {
      const size_t indent = part.find_first_not_of(u' ');
      const auto content = indent == part.npos ? part : part.substr(indent);
      const auto indented = gh::localization::MenuTranslation(content);
      if (indented.empty()) {
        result.append(part);
      } else {
        result.append(part.substr(0, indent));
        result.append(indented);
      }
    }
    if (boundary == text.npos) break;
    result += text[boundary];
    position = boundary + 1;
  }
  return result;
}

bool RefreshCachedString(PPCContext& call, uint8_t* base, uint32_t string,
                         uint32_t scratch) {
  const uint32_t length = REX_LOAD_U32(string + 16);
  const uint32_t data = REX_LOAD_U32(string + 20) < 8
                            ? string : REX_LOAD_U32(string);
  std::u16string current;
  current.reserve(length);
  for (uint32_t i = 0; i < length; ++i) {
    current += static_cast<char16_t>(REX_LOAD_U16(data + i * 2));
  }
  const auto translated = TranslateCachedText(current);
  if (translated == current) return false;
  AssignText(call, base, string, scratch, translated);
  return true;
}

void RefreshCachedMenu(PPCContext& call, uint8_t* base, uint32_t menu,
                       uint32_t scratch) {
  const uint32_t begin = REX_LOAD_U32(menu + 1664);
  const uint32_t end = REX_LOAD_U32(menu + 1668);
  bool changed = false;
  for (uint32_t row = begin; row < end; row += kRowSize) {
    for (uint32_t field : {0u, 28u}) {
      changed |= RefreshCachedString(call, base, row + field, scratch);
    }
  }
  if (changed) {
    const uint32_t selection = REX_LOAD_U32(menu + 1180);
    call.r3.u32 = menu;
    sub_824AAE10(call, base);
    REX_STORE_U32(menu + 1180, selection);
  }
}

void RefreshCaptions(PPCContext& call, uint8_t* base, uint32_t scene,
                     bool title, uint32_t scratch) {
  const uint32_t captions = scene + (title ? 616 : 3396);
  // The native caption controller owns ten 588-byte text widgets.
  for (uint32_t index = 0; index < 10; ++index) {
    const uint32_t widget = captions + 32 + index * 588;
    if (RefreshCachedString(call, base, widget + 32, scratch)) {
      call.r3.u32 = widget;
      sub_824AA5E8(call, base);
    }
  }
}

void RefreshOptionsScene(PPCRegister& stack, PPCRegister& scene, bool title) {
  auto* base = rex::system::kernel_memory()->virtual_membase();
  if (!REX_LOAD_U8(scene.u32 + (title ? 18 : 2510))) return;
  const uint32_t state = REX_LOAD_U16(scene.u32 + (title ? 12 : 2504));
  // The title and pause constructors build these same Options objects once.
  // Credits uses a different graphical widget and has no cached menu rows.
  const uint32_t first = title ? 15 : 11;
  constexpr uint32_t title_offsets[] = {
      20788, 22504, 26700, 36104, 41460, 43336,
      45052, 52188, 59356, 61072, 62804, 64536};
  constexpr uint32_t pause_offsets[] = {
      23096, 24812, 29008, 38412, 43768, 45644,
      47360, 54496, 61664, 63380, 65112};
  const uint32_t count = title ? 12 : 11;
  if (state < first || state >= first + count) return;
  const uint32_t index = state - first;
  const uint32_t offset = title ? title_offsets[index] : pause_offsets[index];
  auto call = HookContext(stack);
  const uint32_t players = index == 6 || index == 7 ? 4 : 1;
  const uint32_t stride = index == 6 ? 1784 : 1792;
  for (uint32_t player = 0; player < players; ++player) {
    RefreshCachedMenu(call, base, scene.u32 + offset + player * stride,
                      stack.u32 - 0x4000);
  }
}

void RefreshParentMenu(PPCContext& call, uint8_t* base, uint32_t scene,
                       bool title) {
  if (title) {
    const uint32_t main = scene + 6532;
    const uint32_t selection = REX_LOAD_U32(main + 1180);
    sub_82435C30(call, base);
    const bool trial = call.r3.u8 != 0;
    call.r3.u32 = main;
    // These two builders destroy the old rows before rebuilding the main menu.
    (trial ? sub_824C9D50 : sub_824C9B20)(call, base);
    REX_STORE_U32(main + 1180, selection);
    return;
  }

  gh::cheats::RebuildPauseMenu(call, base, scene + 9312);
}

}  // namespace

void GhRefreshTitleOptions(PPCRegister& stack, PPCRegister& scene) {
  RefreshOptionsScene(stack, scene, true);
}

void GhRefreshPauseOptions(PPCRegister& stack, PPCRegister& scene) {
  RefreshOptionsScene(stack, scene, false);
}

void GhAppendLanguageOption(PPCRegister& stack, PPCRegister& menu) {
  auto* base = rex::system::kernel_memory()->virtual_membase();
  auto call = HookContext(stack);
  // Reuse the builder's live row, including its native enabled/type fields.
  const uint32_t row = stack.u32 + 80;
  SetLanguageText(call, base, row, stack.u32 - 0x100);
  call.r3.u32 = menu.u32 + 1664;
  call.r4.u32 = row;
  sub_824B3248(call, base);
}

void GhHandleLanguageOption(PPCRegister& stack, PPCRegister& menu,
                            PPCRegister& scene) {
  auto* base = rex::system::kernel_memory()->virtual_membase();
  if (REX_LOAD_U32(menu.u32 + 1180) != kLanguageRow) {
    return;
  }
  const bool confirmed = REX_LOAD_U8(menu.u32 + 1186) != 0;
  const uint32_t input = REX_LOAD_U8(menu.u32 + 1188)
                             ? REX_LOAD_U32(menu.u32 + 60) : 0;
  // Reuse the native menu's repeat-filtered D-pad/stick direction bits.
  const bool horizontal = REX_LOAD_U16(menu.u32 + 4) == 3 && (input & 0xCC);
  if (!confirmed && !horizontal) return;

  // Consume this action before the original submenu transition dispatch.
  REX_STORE_U8(menu.u32 + 1186, 0);
  auto call = HookContext(stack);
  if (confirmed) {
    // Confirm queues a closing animation; language remains on this page.
    call.r3.u32 = menu.u32;
    sub_824A50F0(call, base);
  }
  const int direction = horizontal && (input & 0x44) ? -1 : 1;
  if (!gh::localization::ChangeLanguage(direction, base)) {
    return;
  }

  RefreshMenuText(call, base, menu.u32, sub_824CA1F8, sub_824CBD40, 4);
  const uint32_t row = REX_LOAD_U32(menu.u32 + 1664) + kLanguageRow * kRowSize;
  SetLanguageText(call, base, row, stack.u32 - 0x100);
  call.r3.u32 = menu.u32;
  sub_824AAE10(call, base);
  REX_STORE_U32(menu.u32 + 1180, kLanguageRow);
  const bool title = menu.u32 - scene.u32 == 19072;
  RefreshCaptions(call, base, scene.u32, title, stack.u32 - 0x4000);
  RefreshParentMenu(call, base, scene.u32, title);
  call.r3.u32 = 69;
  call.r4.u32 = 0;
  sub_82424CE8(call, base);
}

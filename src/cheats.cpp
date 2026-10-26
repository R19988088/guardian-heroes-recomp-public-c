#include "generated/default/gh_pch.h"
#include "cheats.h"
#include "localization.h"

#include <array>
#include <algorithm>
#include <bit>
#include <cstring>
#include <string>

DECLARE_REX_FUNC(sub_82417CF0);
DECLARE_REX_FUNC(sub_82424CE8);
DECLARE_REX_FUNC(sub_82493E48);
DECLARE_REX_FUNC(sub_824A50F0);
DECLARE_REX_FUNC(sub_824AA710);
DECLARE_REX_FUNC(sub_824AAE10);
DECLARE_REX_FUNC(sub_824B3248);
DECLARE_REX_FUNC(sub_82495BC0);
DECLARE_REX_FUNC(sub_8243EEA0);
DECLARE_REX_FUNC(sub_82440228);
DECLARE_REX_FUNC(sub_8243EEC0);
DECLARE_REX_FUNC(sub_8243FDC8);
DECLARE_REX_FUNC(sub_82468FA8);
DECLARE_REX_FUNC(sub_824418C8);
DECLARE_REX_FUNC(sub_824429A8);
DECLARE_REX_FUNC(sub_82441248);
DECLARE_REX_FUNC(sub_824C1A68);
DECLARE_REX_FUNC(sub_824D67D8);
DECLARE_REX_FUNC(sub_824DEA90);
DECLARE_REX_FUNC(sub_82478988);
DECLARE_REX_FUNC(sub_824411C0);
DECLARE_REX_FUNC(sub_824E4308);
DECLARE_REX_FUNC(sub_824BFF00);
DECLARE_REX_FUNC(sub_824BFF78);
DECLARE_REX_FUNC(sub_824BFFE8);
DECLARE_REX_FUNC(sub_824C0058);
DECLARE_REX_FUNC(sub_824C00C8);
DECLARE_REX_FUNC(sub_824C0158);

namespace {
constexpr uint32_t kGame = 0x829CBFE8;
constexpr uint32_t kRowSize = 80;
constexpr uint32_t kCheatSelection = 2;
std::array<bool, 10> switches{};
uint32_t pause_menu = 0;
bool cheat_page = false;
std::array<uint32_t, 2> pause_text_scale{};

struct AttributeOverride {
  uint32_t character = 0;
  uint32_t handle = 0;
  std::array<uint8_t, 6> natural{}, applied{};
};
std::array<AttributeOverride, 12> attributes{};

bool IsPlayer(uint8_t* base, uint32_t character) {
  // Native input source 1 is human; source 0/2 is inactive/AI.
  if (!character || REX_LOAD_U8(character + 1166) != 1) return false;
  for (uint32_t slot = 0; slot < 12; ++slot) {
    const uint32_t node = REX_LOAD_U32(kGame + 1672 + slot * 8 + 4);
    if (node && REX_LOAD_U32(node) == character) return true;
  }
  return false;
}

void RefillPlayer(uint8_t* base, uint32_t character) {
  if (!switches[0] || !IsPlayer(base, character) ||
      static_cast<int16_t>(REX_LOAD_U16(character + 202)) <= 0) return;
  gh::cheats::RefillHealth(base, character);
  const int16_t maximum = REX_LOAD_U16(character + 208);
  if (switches[2] && maximum > 0) REX_STORE_U16(character + 206, maximum);
}

void RefillContinues(uint8_t* base) {
  REX_STORE_U8(kGame + 942, 99);
}

PPCContext MenuContext(const PPCContext& context) {
  PPCContext call{};
  call.r1.u32 = context.r1.u32 - 0x5000;
  call.r13 = context.r13;
  call.fpscr = context.fpscr;
  return call;
}

void SetMaximum(uint8_t* base, uint32_t character, uint32_t offset, uint16_t maximum) {
  REX_STORE_U16(character + offset, maximum);
  const int16_t current = REX_LOAD_U16(character + offset - 2);
  if (current > static_cast<int16_t>(maximum)) REX_STORE_U16(character + offset - 2, maximum);
}

void RefreshAttributes(const PPCContext& context, uint8_t* base, uint32_t character,
                       const std::array<uint8_t, 6>& before) {
  auto call = MenuContext(context);
  const uint32_t level = REX_LOAD_U8(character + 225);
  const uint32_t vitality = REX_LOAD_U8(character + 227);
  const uint32_t mentality = REX_LOAD_U8(character + 229);
  if (before[1] != vitality) {
    call.r3.u32 = character; call.r4.u32 = level; call.r5.u32 = vitality;
    sub_8243EEA0(call, base);
    SetMaximum(base, character, 204, call.r3.u16);
  }
  if (before[3] != mentality) {
    call.r3.u32 = character; call.r4.u32 = level; call.r5.u32 = mentality;
    sub_82440228(call, base);
    SetMaximum(base, character, 208, call.r3.u16);
  }
  if (before[1] != vitality || before[3] != mentality) {
    call.r3.u32 = character; call.r4.u32 = level;
    call.r5.u32 = mentality; call.r6.u32 = vitality;
    sub_8243EEC0(call, base);
    SetMaximum(base, character, 212, call.r3.u16);
  }
  if (before[2] != REX_LOAD_U8(character + 228)) {
    call.r3.u32 = character;
    sub_8243FDC8(call, base);
  }
}

void UpdateAttributes(const PPCContext& context, uint8_t* base) {
  for (uint32_t slot = 0; slot < attributes.size(); ++slot) {
    auto& state = attributes[slot];
    const uint32_t handle = REX_LOAD_U32(kGame + 1672 + slot * 8);
    const uint32_t node = REX_LOAD_U32(kGame + 1676 + slot * 8);
    const uint32_t character = node ? REX_LOAD_U32(node) : 0;
    if (!IsPlayer(base, character)) { state = {}; continue; }
    std::array<uint8_t, 6> before;
    for (uint32_t i = 0; i < 6; ++i) before[i] = REX_LOAD_U8(character + 226 + i);
    if (state.character != character || state.handle != handle) {
      state = {character, handle, before, before};
    }
    const uint32_t resource = REX_LOAD_U32(character + 168);
    for (uint32_t i = 0; i < 6; ++i) {
      if (before[i] != state.applied[i]) state.natural[i] = before[i];
      uint32_t maximum = 32;
      if (REX_LOAD_U8(kGame + 1177) & 2) {
        maximum += REX_LOAD_U8(resource + 14 + i);
        if (REX_LOAD_U8(character + 84) == 7) {
          auto call = MenuContext(context);
          call.r3.u32 = call.r1.u32 + 0x1000;
          sub_82495BC0(call, base);
          maximum += REX_LOAD_U8(call.r1.u32 + 0x1000 + i);
        }
      }
      state.applied[i] = switches[0] && switches[i + 4]
          ? std::max<uint32_t>(state.natural[i], maximum) : state.natural[i];
      REX_STORE_U8(character + 226 + i, state.applied[i]);
    }
    RefreshAttributes(context, base, character, before);
  }
}

void BeforeAttributeRebuild(const PPCContext& context, uint8_t* base, uint32_t character) {
  for (auto& state : attributes) {
    if (state.character != character) continue;
    for (uint32_t i = 0; i < 6; ++i) REX_STORE_U8(character + 226 + i, state.natural[i]);
    RefreshAttributes(context, base, character, state.applied);
    state = {};
  }
}

void ApplyCheats(const PPCContext& context, uint8_t* base) {
  UpdateAttributes(context, base);
  RefillContinues(base);
  if (!switches[0]) return;
  for (uint32_t slot = 0; slot < 12; ++slot) {
    const uint32_t node = REX_LOAD_U32(kGame + 1676 + slot * 8);
    if (node) RefillPlayer(base, REX_LOAD_U32(node));
  }
}

void AssignText(PPCContext& call, uint8_t* base, uint32_t string,
                std::u16string_view text) {
  const uint32_t scratch = call.r1.u32 + 0x1000;
  for (size_t i = 0; i < text.size(); ++i) REX_STORE_U16(scratch + i * 2, text[i]);
  REX_STORE_U16(scratch + text.size() * 2, 0);
  call.r3.u32 = string;
  call.r4.u32 = scratch;
  call.r5.u32 = text.size();
  sub_82417CF0(call, base);
}

std::u16string SwitchLabel(uint32_t index) {
  constexpr std::u16string_view chinese[] = {u"作弊", u"血量 max", u"魔力 max", u"接关 max",
      u"力量 max", u"体质 max", u"智力 max", u"精神 max", u"敏捷 max", u"幸运 max"};
  constexpr std::u16string_view english[] = {u"Cheats", u"HP max", u"MP max", u"Continues max",
      u"Strength max", u"Vitality max", u"Intelligence max", u"Mentality max", u"Agility max", u"Luck max"};
  const bool chinese_ui = gh::localization::IsChinese();
  std::u16string text((chinese_ui ? chinese : english)[index]);
  text += chinese_ui ? (switches[index] ? u"    开启" : u"    关闭")
                     : (switches[index] ? u"    On" : u"    Off");
  return text;
}

void AppendRow(PPCContext& call, uint8_t* base, uint32_t menu,
               std::u16string_view text, uint32_t type = 1) {
  const uint32_t row = call.r1.u32 + 0x800;
  call.r3.u32 = row;
  sub_824AA710(call, base);
  AssignText(call, base, row, text);
  REX_STORE_U32(row + 72, type);
  REX_STORE_U32(row + 76, 1);
  call.r3.u32 = menu + 1664;
  call.r4.u32 = row;
  sub_824B3248(call, base);
  call.r3.u32 = row;
  call.r4.u32 = row + kRowSize;
  sub_82493E48(call, base);
}

void ClearRows(PPCContext& call, uint8_t* base, uint32_t menu) {
  const uint32_t begin = REX_LOAD_U32(menu + 1664);
  call.r3.u32 = begin;
  call.r4.u32 = REX_LOAD_U32(menu + 1668);
  sub_82493E48(call, base);
  REX_STORE_U32(menu + 1668, begin);
}

void Layout(PPCContext& call, uint8_t* base, uint32_t menu, uint32_t selection) {
  call.r3.u32 = menu;
  sub_824AAE10(call, base);
  REX_STORE_U32(menu + 1180, selection);
}

void BuildCheatPage(PPCContext& call, uint8_t* base, uint32_t menu) {
  for (uint32_t i = 0; i < 2; ++i) pause_text_scale[i] = REX_LOAD_U32(menu + 252 + i * 4);
  ClearRows(call, base, menu);
  AppendRow(call, base, menu, SwitchLabel(0));
  AppendRow(call, base, menu, u"", 3);
  for (uint32_t i = 1; i < switches.size(); ++i) AppendRow(call, base, menu, SwitchLabel(i));
  AppendRow(call, base, menu, u"", 3);
  AppendRow(call, base, menu, gh::localization::IsChinese() ? u"返回" : u"Back");
  Layout(call, base, menu, 0);
  const float height = std::bit_cast<float>(REX_LOAD_U32(menu + 1216));
  if (height > 620.0f) {
    for (uint32_t i = 0; i < 2; ++i) {
      const float scale = std::bit_cast<float>(pause_text_scale[i]) * (620.0f / height);
      REX_STORE_U32(menu + 252 + i * 4, std::bit_cast<uint32_t>(scale));
    }
    Layout(call, base, menu, 0);
  }
  cheat_page = true;
}

void ConsumeAction(uint8_t* base, uint32_t menu) {
  REX_STORE_U8(menu + 1184, 0);
  REX_STORE_U8(menu + 1186, 0);
  // +1188 enables input for the lifetime of the widget; consume only events.
  REX_STORE_U32(menu + 60, 0);
}

void Reopen(PPCContext& call, uint8_t* base, uint32_t menu) {
  ConsumeAction(base, menu);
  // Confirmation can queue closing before the current active state advances.
  if (REX_LOAD_U16(menu + 4) == 3) REX_STORE_U16(menu + 6, 3);
  call.r3.u32 = menu;
  sub_824A50F0(call, base);
}

void OriginalPredicate(PPCContext& ctx, uint8_t* base, void (*original)(PPCContext&, uint8_t*)) {
  const uint32_t menu = ctx.r3.u32;
  const uint32_t selection = REX_LOAD_U32(menu + 1180);
  if (menu == pause_menu && !cheat_page && selection > kCheatSelection) {
    REX_STORE_U32(menu + 1180, selection - 1);
  }
  original(ctx, base);
  REX_STORE_U32(menu + 1180, selection);
}
}  // namespace

namespace gh::cheats {
bool RefillHealth(uint8_t* base, uint32_t character) {
  if (!switches[0] || !switches[1] || !IsPlayer(base, character)) return false;
  const int16_t maximum = REX_LOAD_U16(character + 204);
  if (maximum <= 0) return false;
  REX_STORE_U16(character + 202, maximum);
  return true;
}

void RebuildPauseMenu(PPCContext& call, uint8_t* base, uint32_t menu) {
  const uint32_t selection = REX_LOAD_U32(menu + 1180);
  if (menu == pause_menu && cheat_page) {
    for (uint32_t i = 0; i < 2; ++i) REX_STORE_U32(menu + 252 + i * 4, pause_text_scale[i]);
  }
  ClearRows(call, base, menu);
  call.r3.u32 = menu;
  sub_824C1A68(call, base);
  REX_STORE_U32(menu + 1180, selection);
}
}  // namespace gh::cheats

REX_HOOK_RAW(sub_824C1A68) {
  const uint32_t menu = ctx.r3.u32;
  __imp__sub_824C1A68(ctx, base);
  pause_menu = menu;
  cheat_page = false;
  auto call = MenuContext(ctx);
  AppendRow(call, base, menu, gh::localization::IsChinese() ? u"作弊" : u"Cheats");
  // Transfer the appended owning row after Moves, preserving native separators.
  const uint32_t begin = REX_LOAD_U32(menu + 1664);
  const uint32_t last = REX_LOAD_U32(menu + 1668) - kRowSize;
  uint32_t insertion = begin;
  uint32_t selectable = 0;
  while (insertion < last) {
    if (REX_LOAD_U32(insertion + 72) != 3 && selectable++ == kCheatSelection) break;
    insertion += kRowSize;
  }
  std::array<uint8_t, kRowSize> row;
  std::memcpy(row.data(), REX_RAW_ADDR(last), kRowSize);
  std::memmove(REX_RAW_ADDR(insertion + kRowSize), REX_RAW_ADDR(insertion), last - insertion);
  std::memcpy(REX_RAW_ADDR(insertion), row.data(), kRowSize);
  Layout(call, base, menu, 0);
}

REX_HOOK_RAW(sub_824D67D8) {
  const uint32_t scene = ctx.r3.u32;
  const uint32_t menu = scene + 9312;
  const uint32_t selected = REX_LOAD_U32(menu + 1180);
  const bool confirmed = REX_LOAD_U8(menu + 1186) != 0;
  if (menu != pause_menu || (!cheat_page && !(confirmed && selected == kCheatSelection))) {
    __imp__sub_824D67D8(ctx, base);
    return;
  }
  auto call = MenuContext(ctx);
  if (!cheat_page) {
    BuildCheatPage(call, base, menu);
    Reopen(call, base, menu);
  } else {
    const uint32_t input = REX_LOAD_U8(menu + 1188) ? REX_LOAD_U32(menu + 60) : 0;
    const uint32_t back_mask = REX_LOAD_U32(menu + 1196) | REX_LOAD_U32(menu + 1200);
    const bool back_pressed = REX_LOAD_U8(menu + 1184) || (input & back_mask) != 0;
    const bool horizontal = REX_LOAD_U16(menu + 4) == 3 && (input & 0xCC);
    if (back_pressed || (confirmed && selected == switches.size())) {
      gh::cheats::RebuildPauseMenu(call, base, menu);
      REX_STORE_U32(menu + 1180, kCheatSelection);
      Reopen(call, base, menu);
    } else if (selected < switches.size() && (confirmed || horizontal)) {
      switches[selected] = !switches[selected];
      const uint32_t mapping = REX_LOAD_U32(menu + 1680);
      const uint32_t row = REX_LOAD_U32(menu + 1664) + REX_LOAD_U32(mapping + selected * 4) * kRowSize;
      AssignText(call, base, row, SwitchLabel(selected));
      ConsumeAction(base, menu);
      // Cancel the queued close without changing geometry or animation progress.
      REX_STORE_U16(menu + 6, 3);
      ApplyCheats(ctx, base);
    }
  }
  // Consumed menu events leave the native pause/session maintenance running.
  ctx.r3.u32 = scene;
  __imp__sub_824D67D8(ctx, base);
}

// Only the action predicates see the original indices; captions keep the real selection.
REX_HOOK_RAW(sub_824BFF00) { OriginalPredicate(ctx, base, __imp__sub_824BFF00); }
REX_HOOK_RAW(sub_824BFF78) { OriginalPredicate(ctx, base, __imp__sub_824BFF78); }
REX_HOOK_RAW(sub_824BFFE8) { OriginalPredicate(ctx, base, __imp__sub_824BFFE8); }
REX_HOOK_RAW(sub_824C0058) { OriginalPredicate(ctx, base, __imp__sub_824C0058); }
REX_HOOK_RAW(sub_824C00C8) { OriginalPredicate(ctx, base, __imp__sub_824C00C8); }
REX_HOOK_RAW(sub_824C0158) { OriginalPredicate(ctx, base, __imp__sub_824C0158); }

REX_HOOK_RAW(sub_824DEA90) {
  const uint32_t scene = ctx.r3.u32;
  if (cheat_page && pause_menu == scene + 9312 &&
      (!REX_LOAD_U8(scene) || REX_LOAD_U16(scene + 2504) != 5)) {
    auto call = MenuContext(ctx);
    gh::cheats::RebuildPauseMenu(call, base, pause_menu);
  }
  ApplyCheats(ctx, base);
  __imp__sub_824DEA90(ctx, base);
}

REX_HOOK_RAW(sub_82478988) {
  const uint32_t character = ctx.r3.u32;
  RefillPlayer(base, character);
  __imp__sub_82478988(ctx, base);
  RefillPlayer(base, character);
}

REX_HOOK_RAW(sub_824E4308) {
  RefillContinues(base);
  __imp__sub_824E4308(ctx, base);
  RefillContinues(base);
}

REX_HOOK_RAW(sub_824411C0) {
  const uint32_t character = ctx.r3.u32;
  gh::cheats::RefillHealth(base, character);
  __imp__sub_824411C0(ctx, base);
  gh::cheats::RefillHealth(base, character);
}

// Native initialization/level allocation must operate on the normal attributes.
#define ATTRIBUTE_REBUILD(name) \
  REX_HOOK_RAW(name) { \
    BeforeAttributeRebuild(ctx, base, ctx.r3.u32); \
    __imp__##name(ctx, base); \
  }
ATTRIBUTE_REBUILD(sub_82468FA8)
ATTRIBUTE_REBUILD(sub_824418C8)
ATTRIBUTE_REBUILD(sub_824429A8)
ATTRIBUTE_REBUILD(sub_82441248)

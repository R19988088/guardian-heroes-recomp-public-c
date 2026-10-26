#include "generated/default/gh_pch.h"
#include "localization.h"
#include "gh_filesystem.h"
#include <stdexcept>
#include <rex/logging.h>
#include "native_memory.h"

#include <array>
#include <cstring>
#include <sstream>
#include <optional>
#include <vector>
#include <unordered_map>
#include <utility>
#include <rex/memory.h>

DECLARE_REX_FUNC(sub_824299A8);
DECLARE_REX_FUNC(sub_82496CB8);
DECLARE_REX_FUNC(sub_824D1E90);

namespace gh::localization {
namespace {
struct MenuText {
  uint32_t address;
  std::vector<uint8_t> english;
  std::vector<uint8_t> chinese;
  std::u16string english_text;
  std::u16string chinese_text;
};
std::vector<MenuText> menu_texts;
std::unordered_map<std::u16string, size_t> menu_lookup;
struct Language {
  const char* code;
  const char* script;
  uint32_t native_index;
  std::u16string_view option;
  std::u16string_view description;
};
constexpr std::array<Language, 7> languages{{
    {"en", "eng", 1, u"Language: < English >",
     u"Press left/right to change language. Your choice is saved."},
    {"zh-Hans", "zhs", 1, u"语言：< 简体中文 >",
     u"按左／右切换语言，选择会自动保存。"},
    {"ja", "jpn", 0, u"言語：< 日本語 >",
     u"左右で言語を切り替えます。設定は自動保存されます。"},
    {"fr", "fre", 2, u"Langue : < Français >",
     u"Appuyez à gauche/droite pour changer de langue. Le choix est enregistré."},
    {"it", "ita", 3, u"Lingua: < Italiano >",
     u"Premi sinistra/destra per cambiare lingua. La scelta viene salvata."},
    {"de", "ger", 4, u"Sprache: < Deutsch >",
     u"Mit links/rechts die Sprache wechseln. Die Auswahl wird gespeichert."},
    {"es", "spa", 5, u"Idioma: < Español >",
     u"Pulsa izquierda/derecha para cambiar el idioma. La selección se guarda."},
}};
size_t language_index = 0;
std::function<void(bool)> language_changed;
using LocalizedText = std::array<std::u16string, languages.size()>;
std::vector<LocalizedText> native_menu_texts;
std::unordered_map<std::u16string, size_t> native_menu_lookup;
bool initialized = false;
std::array<uint32_t, languages.size()> scripts{};
rex::memory::Memory* guest_memory = nullptr;
std::optional<gh::filesystem::Storage> storage;

uint32_t ReadU32(const std::vector<uint8_t>& bytes, size_t& offset) {
  if (offset + 4 > bytes.size()) throw std::runtime_error("Truncated Chinese menu resource");
  const auto* data = bytes.data() + offset;
  offset += 4;
  return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) |
         (uint32_t(data[2]) << 8) | data[3];
}

std::u16string ReadText(uint8_t* base, uint32_t address) {
  std::u16string result;
  if (!address) return result;
  while (const auto value = REX_LOAD_U16(address)) {
    result += static_cast<char16_t>(value);
    address += 2;
  }
  return result;
}

void ReadNativeMenuTables(uint8_t* base) {
  // Original language-indexed tables: labels, descriptions, names and help.
  constexpr uint32_t counts[] = {277, 275, 225, 187, 277, 55, 218, 14, 38, 720};
  for (uint32_t group = 0; group < std::size(counts); ++group) {
    const uint32_t table = 0x82915038 + group * 28;
    for (uint32_t row = 0; row < counts[group]; ++row) {
      LocalizedText text;
      for (size_t locale = 0; locale < languages.size(); ++locale) {
        const auto entries = REX_LOAD_U32(table + languages[locale].native_index * 4);
        const auto address = REX_LOAD_U32(entries + row * 4);
        if (address && (address < 0x82000000 || address >= 0x82410000)) {
    throw std::runtime_error("Invalid original menu language table");
        }
        text[locale] = ReadText(base, address);
      }
      if (text[0].empty()) continue;
      if (const auto it = menu_lookup.find(text[0]); it != menu_lookup.end()) {
        text[1] = menu_texts[it->second].chinese_text;
      }
      const auto index = native_menu_texts.size();
      for (const auto& value : text) {
        if (!value.empty()) native_menu_lookup.try_emplace(value, index);
      }
      native_menu_texts.push_back(std::move(text));
    }
  }
}

void ApplyMenu(uint8_t* base) {
  REX_STORE_U32(0x82991B90, languages[language_index].native_index);
  auto* heap = guest_memory->LookupHeap(0x8201C000);
  uint32_t previous_protection = 0;
  if (!heap->Protect(0x8201C000, 0x54000,
                     rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite,
                     &previous_protection)) {
    throw std::runtime_error("Could not update game language text");
  }
  for (const auto& text : menu_texts) {
    const auto& bytes = IsChinese() ? text.chinese : text.english;
    std::memcpy(base + text.address, bytes.data(), bytes.size());
  }
  if (!heap->Protect(0x8201C000, 0x54000, previous_protection)) {
    throw std::runtime_error("Could not restore game text memory protection");
  }
}
}  // namespace

void Initialize(const std::filesystem::path& assets,
                const std::filesystem::path& user_data, rex::memory::Memory* memory,
                std::function<void(bool)> on_language_changed) {
  language_changed = std::move(on_language_changed);
  storage.emplace(assets, user_data);
  guest_memory = memory;
  std::string preference_text;
  std::string language;
  if (storage->ReadUserText("language.txt", preference_text)) {
    std::istringstream preference(preference_text);
    preference >> language;
  }
  for (size_t i = 0; i < languages.size(); ++i) {
    if (language == languages[i].code) language_index = i;
  }

  std::vector<uint8_t> input;
  if (!storage->ReadResource("media/menu_zhs.bin", input)) {
    throw std::runtime_error("Chinese menu resource is missing or invalid");
  }
  size_t input_offset = 0;
  if (ReadU32(input, input_offset) != 0x47484C31) {
    throw std::runtime_error("Chinese menu resource is missing or invalid");
  }
  const auto count = ReadU32(input, input_offset);
  for (uint32_t i = 0; i < count; ++i) {
    MenuText text;
    text.address = ReadU32(input, input_offset);
    const auto size = ReadU32(input, input_offset);
    if (text.address < 0x82000000 || text.address + size > 0x82410000 ||
        size < 2 || size > 16384 || size % 2) {
      throw std::runtime_error("Invalid Chinese menu text span");
    }
    text.english.resize(size);
    text.chinese.resize(size);
    if (input_offset + size * 2 > input.size()) {
      throw std::runtime_error("Truncated Chinese menu resource");
    }
    std::memcpy(text.english.data(), input.data() + input_offset, size);
    input_offset += size;
    std::memcpy(text.chinese.data(), input.data() + input_offset, size);
    input_offset += size;
    auto decode = [](const auto& bytes) {
      std::u16string result;
      for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
        const char16_t value = (uint16_t(bytes[i]) << 8) | bytes[i + 1];
        if (!value) break;
        result += value;
      }
      return result;
    };
    text.english_text = decode(text.english);
    text.chinese_text = decode(text.chinese);
    menu_lookup[text.english_text] = menu_texts.size();
    menu_lookup[text.chinese_text] = menu_texts.size();
    menu_texts.push_back(std::move(text));
  }
  auto load_script = [&](const std::string& filename) {
    std::vector<uint8_t> bytes;
    // The retail package used for this build only carries the English archive.
    // Missing optional locales reuse it; Chinese has its own generated archive.
    if (!storage->ReadResource(std::string("media/") + filename, bytes) &&
        !storage->ReadResource("media/script_eng.mrg", bytes)) {
      throw std::runtime_error("Dialogue resource is missing or invalid");
    }
    if (bytes.size() < 1632 || bytes[3] != 50) {
      throw std::runtime_error("Dialogue resource is missing or invalid");
    }
    const auto address = memory->SystemHeapAlloc(static_cast<uint32_t>(bytes.size()));
    if (!address) throw std::runtime_error("Could not allocate dialogue resource");
    // Guest allocation remains runtime-owned, but address translation follows
    // the project ABI so physical alias padding has one implementation.
    std::memcpy(rex::memory::GuestPtr(memory->virtual_membase(), address),
                bytes.data(), bytes.size());
    return address;
  };
  for (size_t i = 0; i < languages.size(); ++i) {
    scripts[i] = load_script(std::string("script_") + languages[i].script + ".mrg");
  }
  language_changed(IsChinese());
}

bool IsChinese() { return language_index == 1; }

std::u16string_view LanguageOption() { return languages[language_index].option; }
std::u16string_view LanguageDescription() { return languages[language_index].description; }

std::u16string_view MenuTranslation(std::u16string_view current) {
  const std::u16string key(current);
  if (const auto native = native_menu_lookup.find(key); native != native_menu_lookup.end()) {
    return native_menu_texts[native->second][language_index];
  }
  const auto it = menu_lookup.find(key);
  if (it == menu_lookup.end()) return {};
  const auto& text = menu_texts[it->second];
  return IsChinese() ? text.chinese_text : text.english_text;
}

bool ChangeLanguage(int direction, uint8_t* base) {
  const auto next = (language_index + languages.size() + direction) % languages.size();
  const std::string preference = std::string(languages[next].code) + '\n';
  if (!storage->WriteUserText("language.txt", preference)) {
    REXLOG_ERROR("Could not save game language in user data directory");
    return false;
  }
  language_index = next;
  ApplyMenu(base);
  language_changed(IsChinese());
  REXLOG_INFO(std::string("Game language: ") + languages[language_index].code);
  return true;
}

void AfterLanguageInit(uint8_t* base) {
  if (initialized) return;
  for (const auto& text : menu_texts) {
    if (std::memcmp(base + text.address, text.english.data(), text.english.size())) {
      throw std::runtime_error("Chinese menu resource does not match this game image");
    }
  }
  ReadNativeMenuTables(base);
  initialized = true;
  ApplyMenu(base);
  REXLOG_INFO(std::string("Game language restored: ") + languages[language_index].code);
}

uint32_t SelectedScript() { return scripts[language_index]; }

void SelectDialoguePage(uint32_t parser, uint8_t* base) {
  if (REX_LOAD_U16(parser + 978) != 2) return;
  const auto file = REX_LOAD_U32(parser + 4);
  for (uint32_t i = 0; i < 50; ++i) {
    const auto entry = (i + 1) * 32;
    const auto target_archive = SelectedScript();
    const auto target = target_archive + REX_LOAD_U32(target_archive + entry);
    if (file == target) return;
    for (const auto archive : scripts) {
      if (file != archive + REX_LOAD_U32(archive + entry)) continue;
      const auto block = REX_LOAD_U32(parser + 3532);
      if (!block || block > REX_LOAD_U32(file)) return;
      const auto start = file + REX_LOAD_U32(file + block * 4);
      const auto cursor = REX_LOAD_U32(parser + 972);
      const auto size = REX_LOAD_U32(archive + entry + 4);
      if (cursor < start || cursor >= file + size) return;
      uint32_t pages = 0;
      for (auto position = start; position < cursor; position += 2) {
        const auto value = REX_LOAD_U16(position);
        if (value == 0xF800) ++pages;
        if (value == 0xE000 || value == 0xE100 || value == 0xE200) position += 2;
      }
      const auto target_start = target + REX_LOAD_U32(target + block * 4);
      auto target_cursor = target_start;
      const auto target_size = REX_LOAD_U32(target_archive + entry + 4);
      while (pages && target_cursor + 2 <= target + target_size) {
        const auto value = REX_LOAD_U16(target_cursor);
        target_cursor += 2;
        if (value == 0xF800) --pages;
        if (value == 0xE000 || value == 0xE100 || value == 0xE200) target_cursor += 2;
      }
      if (pages) throw std::runtime_error("Dialogue page correspondence is invalid");
      REX_STORE_U32(parser + 4, target);
      REX_STORE_U32(parser + 968, target_start);
      REX_STORE_U32(parser + 972, target_cursor);
      return;
    }
  }
}
}  // namespace gh::localization

// Packed UTF-16 pools must be scanned by the original initializer before edits.
extern "C" void sub_824299A8(GuestContext& ctx, uint8_t* base) {
  __imp__sub_824299A8(ctx, base);
  gh::localization::AfterLanguageInit(base);
}

// Keep every archive alive so existing parsers can finish their current message.
extern "C" void sub_82496CB8(GuestContext& ctx, uint8_t* base) {
  __imp__sub_82496CB8(ctx, base);
  if (ctx.r3.u32) ctx.r3.u32 = gh::localization::SelectedScript();
}

extern "C" void sub_824D1E90(GuestContext& ctx, uint8_t* base) {
  gh::localization::SelectDialoguePage(ctx.r3.u32, base);
  __imp__sub_824D1E90(ctx, base);
}

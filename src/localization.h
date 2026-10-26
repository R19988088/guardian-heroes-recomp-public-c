#pragma once

#include <filesystem>
#include <functional>
#include <string_view>
#include <rex/ppc/context.h>
namespace rex::memory { class Memory; }

namespace gh::localization {
void Initialize(const std::filesystem::path& assets,
                const std::filesystem::path& user_data, rex::memory::Memory* memory,
                std::function<void(bool)> on_language_changed);
bool IsChinese();
bool ChangeLanguage(int direction, uint8_t* base);
std::u16string_view LanguageOption();
std::u16string_view LanguageDescription();
std::u16string_view MenuTranslation(std::u16string_view current);
}

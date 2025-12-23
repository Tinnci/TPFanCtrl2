#pragma once

#include <string>
#include <map>
#include <vector>

class I18nManager {
public:
    static I18nManager& Get() {
        static I18nManager instance;
        return instance;
    }

    void SetLanguage(const std::string& langCode);
    const char* Translate(const std::string& key);

    struct LanguageInfo {
        std::string code;
        std::string name;
    };
    const std::vector<LanguageInfo>& GetAvailableLanguages() const { return m_languages; }
    const std::string& GetCurrentLanguage() const { return m_currentLang; }

private:
    I18nManager();
    void LoadTranslations();

    std::string m_currentLang = "en";
    std::map<std::string, std::map<std::string, std::string>> m_translations;
    std::vector<LanguageInfo> m_languages;
};

// Helper macro
#define _TR(key) I18nManager::Get().Translate(key)

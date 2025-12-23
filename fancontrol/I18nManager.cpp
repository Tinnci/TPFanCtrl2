#include "_prec.h"
#include "I18nManager.h"

I18nManager::I18nManager() {
    m_languages = {
        {"en", "English"},
        {"zh", "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87"}
    };
    LoadTranslations();
}

void I18nManager::SetLanguage(const std::string& langCode) {
    for (const auto& lang : m_languages) {
        if (lang.code == langCode) {
            m_currentLang = langCode;
            return;
        }
    }
}

const char* I18nManager::Translate(const std::string& key) {
    auto itLang = m_translations.find(m_currentLang);
    if (itLang != m_translations.end()) {
        auto itKey = itLang->second.find(key);
        if (itKey != itLang->second.end()) {
            return itKey->second.c_str();
        }
    }
    
    // Fallback to English
    auto itEn = m_translations.find("en");
    if (itEn != m_translations.end()) {
        auto itKey = itEn->second.find(key);
        if (itKey != itEn->second.end()) {
            return itKey->second.c_str();
        }
    }

    return key.c_str();
}

void I18nManager::LoadTranslations() {
    // English
    auto& en = m_translations["en"];
    en["APP_TITLE"] = "TPFanCtrl2 - Modernized";
    en["TAB_DASHBOARD"] = "Dashboard";
    en["TAB_SETTINGS"] = "Settings";
    en["SECTION_STATUS"] = "System Status";
    en["SECTION_CONTROL"] = "Quick Control";
    en["SECTION_HISTORY"] = "History";
    en["SECTION_LOGS"] = "System Logs";
    en["MODE_BIOS"] = "BIOS";
    en["MODE_MANUAL"] = "Manual";
    en["MODE_SMART"] = "Smart";
    en["ALGO_STEP"] = "Step (Classic)";
    en["ALGO_PID"] = "PID (Modern)";
    en["BTN_MINIMIZE"] = "Minimize to Tray";
    en["LBL_MANUAL_LEVEL"] = "Manual Level:";
    en["LBL_ALGORITHM"] = "Algorithm:";
    en["LBL_TEMP"] = "Temp";
    en["LBL_FAN"] = "Fan";
    en["SETTING_BEHAVIOR"] = "Application Behavior";
    en["SETTING_POLLING"] = "Hardware Polling";
    en["SETTING_PID"] = "PID Controller (Advanced)";
    en["SETTING_SENSORS"] = "Sensor Management";
    en["SIDEBAR_GENERAL"] = "General";
    en["SIDEBAR_PID"] = "PID Tuning";
    en["SIDEBAR_SENSORS"] = "Sensors";
    en["OPT_START_MINIMIZED"] = "Start Minimized to Tray";
    en["OPT_MINIMIZE_TRAY"] = "Minimize to Tray";
    en["OPT_MINIMIZE_CLOSE"] = "Close Button Minimizes";
    en["LBL_CYCLE"] = "Sensor Refresh Cycle (seconds):";
    en["LBL_REFRESH_SENSOR"] = "Sensor Refresh";
    en["LBL_REFRESH_FAN"] = "Fan RPM Refresh";
    en["LBL_REFRESH_CTRL"] = "Control Frequency";
    en["LBL_TARGET_TEMP"] = "Target Temp";
    en["LBL_KP"] = "Kp (Prop)";
    en["LBL_KI"] = "Ki (Int)";
    en["LBL_KD"] = "Kd (Deriv)";
    en["BTN_RESET_PID"] = "Reset PID to Defaults";
    en["DESC_SENSORS"] = "Uncheck sensors that provide invalid readings. Grayed out sensors have never returned a valid value.";
    en["TIP_SENSOR_UNAVAILABLE"] = "This sensor index (0x%02X) has not returned any valid temperature data since startup.";
    en["BTN_SAVE_ALL"] = "Save All Settings to INI";
    en["LBL_DEBUG_INFO"] = "Debug Info";
    en["LOG_SAVE_SUCCESS"] = "All settings saved successfully.";
    en["LOG_SAVE_ERROR"] = "ERROR: Failed to save to TPFanCtrl2.ini";
    en["LBL_FAN_SPEED"] = "Fan Speed:";
    en["LBL_MAX_TEMP"] = "MAX TEMPERATURE";
    en["LBL_FAN_SPEEDS"] = "FAN SPEEDS";
    en["LBL_CONTROL_MODE"] = "CONTROL MODE";
    en["LBL_SMART_STEP"] = "SMART (STEP)";
    en["LBL_SMART_PID"] = "SMART (PID)";

    // Chinese (Simplified)
    auto& zh = m_translations["zh"];
    zh["APP_TITLE"] = "TPFanCtrl2 - \xE7\x8E\xB0\xE4\xBB\xA3\xE5\x8C\x96\xE7\x89\x88";
    zh["TAB_DASHBOARD"] = "\xE4\xBB\xAA\xE8\xA1\xA8\xE7\x9B\x98";
    zh["TAB_SETTINGS"] = "\xE8\xAE\xBE\xE7\xBD\xAE";
    zh["SECTION_STATUS"] = "\xE7\xB3\xBB\xE7\xBB\x9F\xE7\x8A\xB6\xE6\x80\x81";
    zh["SECTION_CONTROL"] = "\xE5\xBF\xAB\xE9\x80\x9F\xE6\x8E\xA7\xE5\x88\xB6";
    zh["SECTION_HISTORY"] = "\xE5\x8E\x86\xE5\x8F\xB2\xE6\x9B\xB2\xE7\xBA\xBF";
    zh["SECTION_LOGS"] = "\xE7\xB3\xBB\xE7\xBB\x9F\xE6\x97\xA5\xE5\xBF\x97";
    zh["MODE_BIOS"] = "BIOS \xE6\xA8\xA1\xE5\xBC\x8F";
    zh["MODE_MANUAL"] = "\xE6\x89\x8B\xE5\x8A\xA8\xE6\xA8\xA1\xE5\xBC\x8F";
    zh["MODE_SMART"] = "\xE6\x99\xBA\xE8\x83\xBD\xE6\xA8\xA1\xE5\xBC\x8F";
    zh["ALGO_STEP"] = "\xE5\x88\x86\xE7\xBA\xA7\xE6\x8E\xA7\xE5\x88\xB6 (\xE7\xBB\x8F\xE5\x85\xB8)";
    zh["ALGO_PID"] = "PID \xE6\x8E\xA7\xE5\x88\xB6 (\xE7\x8E\x80\xE4\xBB\xA3)";
    zh["BTN_MINIMIZE"] = "\xE6\x9C\x80\xE5\xB0\x8F\xE5\x8C\x96\xE5\x88\xB0\xE6\x89\x98\xE7\x9B\x98";
    zh["LBL_MANUAL_LEVEL"] = "\xE6\x89\x8B\xE5\x8A\xA8\xE6\x8C\xA1\xE4\xBD\x8D:";
    zh["LBL_ALGORITHM"] = "\xE6\x8E\xA7\xE5\x88\xB6\xE7\xAE\x97\xE6\xB3\x95:";
    zh["LBL_TEMP"] = "\xE6\xB8\xA9\xE5\xBA\xA6";
    zh["LBL_FAN"] = "\xE9\xA3\x8E\xE6\x89\x87";
    zh["SETTING_BEHAVIOR"] = "\xE7\xA8\x8B\xE5\xBA\x8F\xE8\xA1\x8C\xE4\xB8\xBA";
    zh["SETTING_POLLING"] = "\xE7\xA1\xAC\xE4\xBB\xB6\xE8\xBD\xAE\xE8\xAF\xA2";
    zh["SETTING_PID"] = "PID \xE6\x8E\xA7\xE5\x88\xB6\xE5\x99\xA8 (\xE9\xAB\x98\xE7\xBA\xA7)";
    zh["SETTING_SENSORS"] = "\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8\xE7\xAE\xA1\xE7\x90\x86";
    zh["SIDEBAR_GENERAL"] = "\xE5\xB8\xB8\xE8\xA7\x84\xE8\xAE\xBE\xE7\xBD\xAE";
    zh["SIDEBAR_PID"] = "PID \xE8\xB0\x83\xE4\xBC\x98";
    zh["SIDEBAR_SENSORS"] = "\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8";
    zh["OPT_START_MINIMIZED"] = "\xE5\x90\xAF\xE5\x8A\xA8\xE6\x97\xB6\xE6\x9C\x80\xE5\xB0\x8F\xE5\x8C\x96\xE5\x88\xB0\xE6\x89\x98\xE7\x9B\x98";
    zh["OPT_MINIMIZE_TRAY"] = "\xE6\x9C\x80\xE5\xB0\x8F\xE5\x8C\x96\xE5\x88\xB0\xE6\x89\x98\xE7\x9B\x98";
    zh["OPT_MINIMIZE_CLOSE"] = "\xE5\x85\xB3\xE9\x97\xAD\xE6\x8C\x89\xE9\x92\xAE\xE6\x9C\x80\xE5\xB0\x8F\xE5\x8C\x96";
    zh["LBL_CYCLE"] = "\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8\xE5\x88\xB7\xE6\x96\xB0\xE5\x91\xA8\xE6\x9C\x9F (\xE7\xA7\x92):";
    zh["LBL_REFRESH_SENSOR"] = "\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8\xE5\x88\xB7\xE6\x96\xB0";
    zh["LBL_REFRESH_FAN"] = "\xE9\xA3\x8E\xE6\x89\x87\xE8\xBD\xAC\xE9\x80\x9F\xE5\x88\xB7\xE6\x96\xB0";
    zh["LBL_REFRESH_CTRL"] = "\xE6\x8E\xA7\xE5\x88\xB6\xE9\xA2\x91\xE7\x8E\x87";
    zh["LBL_TARGET_TEMP"] = "\xE7\x9B\xAE\xE6\xA0\x87\xE6\xB8\xA9\xE5\xBA\xA6";
    zh["LBL_KP"] = "Kp (\xE6\xAF\x94\xE4\xBE\x8B)";
    zh["LBL_KI"] = "Ki (\xE7\xA7\xAF\xE5\x88\x86)";
    zh["LBL_KD"] = "Kd (\xE5\xBE\xAE\xE5\x88\x86)";
    zh["BTN_RESET_PID"] = "\xE9\x87\x8D\xE7\xBD\xAE PID \xE4\xB8\xBA\xE9\xBB\x98\xE8\xAE\xA4\xE5\x80\xBC";
    zh["DESC_SENSORS"] = "\xE5\x8F\x96\xE6\xB6\x88\xE5\x8B\xBE\xE9\x80\x89\xE6\x8F\x90\xE4\xBE\x9B\xE6\x97\xA0\xE6\x95\x88\xE8\xAF\xBB\xE5\x8F\x96\xE5\x80\xBC\xE7\x9A\x84\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8\xE3\x80\x82\xE7\x81\xB0\xE8\x89\xB2\xE6\x98\xBE\xE7\xA4\xBA\xE7\x9A\x84\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8\xE4\xBB\x8E\xE6\x9C\xAA\xE8\xBF\x94\xE5\x9B\x9E\xE8\xBF\x87\xE6\x9C\x89\xE6\x95\x88\xE5\x80\xBC\xE3\x80\x82";
    zh["TIP_SENSOR_UNAVAILABLE"] = "\xE8\xAF\xA5\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8\xE7\xB4\xA2\xE5\xBC\x95 (0x%02X) \xE8\x87\xAA\xE5\x90\xAF\xE5\x8A\xA8\xE4\xBB\xA5\xE6\x9D\xA5\xE6\x9C\xAA\xE8\xBF\x94\xE5\x9B\x9E\xE4\xBB\xBB\xE4\xBD\x95\xE6\x9C\x89\xE6\x95\x88\xE7\x9A\x84\xE6\xB8\xA9\xE5\xBA\xA6\xE6\x95\xB0\xE6\x8D\xAE\xE3\x80\x82";
    zh["BTN_SAVE_ALL"] = "\xE4\xBF\x9D\xE5\xAD\x98\xE6\x89\x80\xE6\x9C\x89\xE8\xAE\xBE\xE7\xBD\xAE\xE5\x88\xB0 INI";
    zh["LBL_DEBUG_INFO"] = "\xE8\xB0\x83\xE8\xAF\x95\xE4\xBF\xA1\xE6\x81\xAF";
    zh["LOG_SAVE_SUCCESS"] = "\xE6\x89\x80\xE6\x9C\x89\xE8\xAE\xBE\xE7\xBD\xAE\xE5\xBB\xAD\xE6\x88\x90\xE5\x8A\x9F\xE4\xBF\x9D\xE5\xAD\x98\xE3\x80\x82";
    zh["LOG_SAVE_ERROR"] = "\xE9\x94\x99\xE8\xAF\xAF\xEF\xBC\x9A\xE6\x97\xA0\xE6\xB3\x95\xE4\xBF\x9D\xE5\xAD\x98\xE5\x88\xB0 TPFanCtrl2.ini";
    zh["LBL_FAN_SPEED"] = "\xE9\xA3\x8E\xE6\x89\x87\xE8\xBD\xAC\xE9\x80\x9F:";
    zh["LBL_MAX_TEMP"] = "\xE6\x9C\x80\xE9\xAB\x98\xE6\xB8\xA9\xE5\xBA\xA6";
    zh["LBL_FAN_SPEEDS"] = "\xE9\xA3\x8E\xE6\x89\x87\xE8\xBD\xAC\xE9\x80\x9F";
    zh["LBL_CONTROL_MODE"] = "\xE6\x8E\xA7\xE5\x88\xB6\xE6\xA8\xA1\xE5\xBC\x8F";
    zh["LBL_SMART_STEP"] = "\xE6\x99\xBA\xE8\x83\xBD (\xE5\x88\x86\xE7\xBA\xA7)";
    zh["LBL_SMART_PID"] = "\xE6\x99\xBA\xE8\x83\xBD (PID)";
}

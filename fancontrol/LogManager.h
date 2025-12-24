#pragma once

// LogManager.h - Unified logging system for TPFanCtrl2
// Wraps spdlog and provides a unified interface for both Win32 and ImGui UIs

#include <string>
#include <deque>
#include <mutex>
#include <format>
#include "spdlog/spdlog.h"

namespace Log {

// Log Levels (matching spdlog)
enum class Level {
    Debug,
    Info,
    Warn,
    Error
};

// UI-visible log buffer (for ImGui log panel)
class UILogBuffer {
public:
    static UILogBuffer& Get() {
        static UILogBuffer instance;
        return instance;
    }

    template<typename... Args>
    void Add(Level level, std::format_string<Args...> fmt, Args&&... args) {
        std::string msg = std::format(fmt, std::forward<Args>(args)...);
        
        // Route to spdlog
        switch (level) {
            case Level::Debug: spdlog::debug(msg); break;
            case Level::Info:  spdlog::info(msg);  break;
            case Level::Warn:  spdlog::warn(msg);  break;
            case Level::Error: spdlog::error(msg); break;
        }
        
        // Buffer for UI display
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Add prefix for UI log
        std::string prefix;
        switch (level) {
            case Level::Warn:  prefix = "[WARN] "; break;
            case Level::Error: prefix = "[ERROR] "; break;
            default: break;
        }
        
        m_items.push_back(prefix + msg);
        if (m_items.size() > MaxItems) {
            m_items.pop_front();
        }
    }

    // Get a copy of the log items for rendering
    std::deque<std::string> GetItems() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_items;
    }
    
    // Clear all items
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_items.clear();
    }

private:
    UILogBuffer() = default;
    
    static constexpr size_t MaxItems = 100;
    std::deque<std::string> m_items;
    mutable std::mutex m_mutex;
};

// Convenience logging functions
template<typename... Args>
inline void Debug(std::format_string<Args...> fmt, Args&&... args) {
    UILogBuffer::Get().Add(Level::Debug, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void Info(std::format_string<Args...> fmt, Args&&... args) {
    UILogBuffer::Get().Add(Level::Info, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void Warn(std::format_string<Args...> fmt, Args&&... args) {
    UILogBuffer::Get().Add(Level::Warn, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void Error(std::format_string<Args...> fmt, Args&&... args) {
    UILogBuffer::Get().Add(Level::Error, fmt, std::forward<Args>(args)...);
}

} // namespace Log

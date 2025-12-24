// Core/IThermalObserver.h - Observer interface for thermal events
// Part of the Core library - NO Windows UI dependencies allowed here
#pragma once

#include "Events.h"
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>

namespace Core {

/// Interface for receiving thermal events
/// Implement this to receive notifications from ThermalManager
class IThermalObserver {
public:
    virtual ~IThermalObserver() = default;
    
    /// Called when any thermal event occurs
    /// Note: This may be called from a background thread!
    virtual void OnThermalEvent(const ThermalEvent& event) = 0;
};

/// Convenience base class that dispatches events to typed handlers
/// Inherit from this and override only the handlers you need
class ThermalObserverBase : public IThermalObserver {
public:
    void OnThermalEvent(const ThermalEvent& event) override {
        std::visit([this](const auto& e) { DispatchEvent(e); }, event);
    }

protected:
    // Override these in derived classes as needed
    virtual void OnTemperatureUpdate(const TemperatureUpdateEvent& /*event*/) {}
    virtual void OnFanStateChange(const FanStateChangeEvent& /*event*/) {}
    virtual void OnModeChange(const ModeChangeEvent& /*event*/) {}
    virtual void OnError(const ErrorEvent& /*event*/) {}
    virtual void OnLog(const LogEvent& /*event*/) {}

private:
    void DispatchEvent(const TemperatureUpdateEvent& e) { OnTemperatureUpdate(e); }
    void DispatchEvent(const FanStateChangeEvent& e) { OnFanStateChange(e); }
    void DispatchEvent(const ModeChangeEvent& e) { OnModeChange(e); }
    void DispatchEvent(const ErrorEvent& e) { OnError(e); }
    void DispatchEvent(const LogEvent& e) { OnLog(e); }
};

/// Thread-safe event dispatcher
/// ThermalManager uses this internally to notify observers
class EventDispatcher {
public:
    using Callback = std::function<void(const ThermalEvent&)>;
    using SubscriptionId = size_t;

    /// Subscribe a callback. Returns an ID that can be used to unsubscribe.
    SubscriptionId Subscribe(Callback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        SubscriptionId id = m_nextId++;
        m_callbacks[id] = std::move(callback);
        return id;
    }

    /// Subscribe an observer object
    SubscriptionId Subscribe(std::weak_ptr<IThermalObserver> observer) {
        return Subscribe([observer](const ThermalEvent& event) {
            if (auto obs = observer.lock()) {
                obs->OnThermalEvent(event);
            }
        });
    }

    /// Unsubscribe by ID
    void Unsubscribe(SubscriptionId id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.erase(id);
    }

    /// Dispatch an event to all subscribers
    /// Thread-safe: can be called from any thread
    void Dispatch(const ThermalEvent& event) {
        std::vector<Callback> callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            callbacks.reserve(m_callbacks.size());
            for (const auto& [id, cb] : m_callbacks) {
                callbacks.push_back(cb);
            }
        }
        // Call callbacks outside of lock to prevent deadlocks
        for (const auto& cb : callbacks) {
            cb(event);
        }
    }

    /// Clear all subscriptions
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.clear();
    }

private:
    std::mutex m_mutex;
    std::unordered_map<SubscriptionId, Callback> m_callbacks;
    std::atomic<SubscriptionId> m_nextId{1};
};

} // namespace Core

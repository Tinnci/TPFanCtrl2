// tests/core_test.cpp - Unit tests for Core library (ThermalManager, UIAdapter)
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

#include "Core/ThermalManager.h"
#include "Core/UIAdapter.h"
#include "Core/Events.h"
#include "Core/SensorConfig.h"
#include "ECManager.h"
#include "MockIOProvider.h"
#include "ConfigManager.h"

using namespace Core;

// ============================================================================
// ThermalManager Tests
// ============================================================================

class ThermalManagerTest : public ::testing::Test {
protected:
    std::shared_ptr<MockIOProvider> mockIO;
    std::shared_ptr<ECManager> ecManager;
    std::shared_ptr<ThermalManager> thermalManager;
    ThermalConfig config;

    void SetUp() override {
        mockIO = std::make_shared<MockIOProvider>();
        ecManager = std::make_shared<ECManager>(mockIO, nullptr);
        
        // Setup default config
        config = ThermalConfig();
        config.cycleSeconds = 1;
        config.sensors = CreateDefaultSensorConfig();
        config.sensors[0].name = "CPU";
        config.sensors[1].name = "GPU";
        
        // Add a simple smart profile
        config.smartProfiles[0].push_back(SmartLevelDefinition(50, 0, 2, 2));
        config.smartProfiles[0].push_back(SmartLevelDefinition(60, 3, 2, 2));
        config.smartProfiles[0].push_back(SmartLevelDefinition(70, 7, 2, 2));
        
        // Set initial sensor values
        mockIO->SetECByte(0x78, 45);  // CPU = 45
        mockIO->SetECByte(0x79, 50);  // GPU = 50
    }

    void TearDown() override {
        if (thermalManager && thermalManager->IsRunning()) {
            thermalManager->Stop();
        }
    }
    
    void CreateManager() {
        thermalManager = std::make_shared<ThermalManager>(ecManager, config);
    }
};

TEST_F(ThermalManagerTest, InitializationState) {
    CreateManager();
    
    EXPECT_FALSE(thermalManager->IsRunning());
    EXPECT_EQ(thermalManager->GetMode(), ControlMode::BIOS);
    EXPECT_EQ(thermalManager->GetSmartProfileIndex(), 0);
}

TEST_F(ThermalManagerTest, StartAndStop) {
    CreateManager();
    
    thermalManager->Start();
    EXPECT_TRUE(thermalManager->IsRunning());
    
    // Give it time to run at least one cycle
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    thermalManager->Stop();
    EXPECT_FALSE(thermalManager->IsRunning());
}

TEST_F(ThermalManagerTest, ModeChange) {
    CreateManager();
    
    thermalManager->SetMode(ControlMode::Smart, 0);
    EXPECT_EQ(thermalManager->GetMode(), ControlMode::Smart);
    EXPECT_EQ(thermalManager->GetSmartProfileIndex(), 0);
    
    thermalManager->SetMode(ControlMode::Smart, 1);
    EXPECT_EQ(thermalManager->GetSmartProfileIndex(), 1);
    
    thermalManager->SetMode(ControlMode::Manual);
    EXPECT_EQ(thermalManager->GetMode(), ControlMode::Manual);
    
    thermalManager->SetMode(ControlMode::BIOS);
    EXPECT_EQ(thermalManager->GetMode(), ControlMode::BIOS);
}

TEST_F(ThermalManagerTest, GetStateSnapshot) {
    CreateManager();
    thermalManager->Start();
    
    // Wait for state to be populated
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    ThermalState state = thermalManager->GetState();
    
    thermalManager->Stop();
    
    EXPECT_FALSE(state.sensors.empty());
    EXPECT_TRUE(state.isOperational);
}

TEST_F(ThermalManagerTest, ForceUpdate) {
    CreateManager();
    thermalManager->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    thermalManager->ForceUpdate();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    ThermalState state = thermalManager->GetState();
    thermalManager->Stop();
    
    // If force update worked, state should be valid
    EXPECT_TRUE(state.isOperational);
}

// ============================================================================
// UIAdapter Tests
// ============================================================================

class UIAdapterTest : public ::testing::Test {
protected:
    std::shared_ptr<MockIOProvider> mockIO;
    std::shared_ptr<ECManager> ecManager;
    std::shared_ptr<ThermalManager> thermalManager;
    std::unique_ptr<UIAdapter> uiAdapter;
    ThermalConfig config;

    void SetUp() override {
        mockIO = std::make_shared<MockIOProvider>();
        ecManager = std::make_shared<ECManager>(mockIO, nullptr);
        
        config = ThermalConfig();
        config.cycleSeconds = 1;
        config.sensors = CreateDefaultSensorConfig();
        config.sensors[0].name = "CPU";
        
        mockIO->SetECByte(0x78, 55);
        
        thermalManager = std::make_shared<ThermalManager>(ecManager, config);
        uiAdapter = std::make_unique<UIAdapter>(thermalManager);
    }

    void TearDown() override {
        if (thermalManager->IsRunning()) {
            thermalManager->Stop();
        }
    }
};

TEST_F(UIAdapterTest, GetSnapshotEmpty) {
    UISnapshot snapshot = uiAdapter->GetSnapshot();
    
    EXPECT_EQ(snapshot.Mode, 2);  // Default is Smart (index 2)
    EXPECT_EQ(snapshot.MaxTemp, 0);  // No data yet
}

TEST_F(UIAdapterTest, StateUpdatesFromManager) {
    thermalManager->Start();
    
    // Wait for data
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    uiAdapter->Update(0.1f);
    
    UISnapshot snapshot = uiAdapter->GetSnapshot();
    
    thermalManager->Stop();
    
    EXPECT_GT(snapshot.MaxTemp, 0);
    EXPECT_FALSE(snapshot.Sensors.empty());
}

TEST_F(UIAdapterTest, SetMode) {
    uiAdapter->SetMode(0);  // BIOS
    EXPECT_EQ(thermalManager->GetMode(), ControlMode::BIOS);
    
    uiAdapter->SetMode(1);  // Manual
    EXPECT_EQ(thermalManager->GetMode(), ControlMode::Manual);
    
    uiAdapter->SetMode(2);  // Smart
    EXPECT_EQ(thermalManager->GetMode(), ControlMode::Smart);
}

TEST_F(UIAdapterTest, SetManualLevel) {
    uiAdapter->SetManualLevel(5);
    
    UISnapshot snapshot = uiAdapter->GetSnapshot();
    EXPECT_EQ(snapshot.ManualLevel, 5);
}

TEST_F(UIAdapterTest, TempHistoryAccumulates) {
    thermalManager->Start();
    
    // Let it run for a bit to accumulate history
    for (int i = 0; i < 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        uiAdapter->Update(0.2f);
    }
    
    thermalManager->Stop();
    
    auto history = uiAdapter->GetAllTempHistory();
    
    // Should have at least one sensor with history
    bool hasHistory = false;
    for (const auto& kv : history) {
        if (!kv.second.empty()) {
            hasHistory = true;
            break;
        }
    }
    EXPECT_TRUE(hasHistory);
}

TEST_F(UIAdapterTest, TrayCallbackInvoked) {
    std::atomic<bool> callbackCalled{false};
    
    uiAdapter->SetTrayUpdateCallback([&callbackCalled](int temp, int fan) {
        callbackCalled = true;
    });
    
    thermalManager->Start();
    
    // Run update loop for a while to trigger tray callback
    for (int i = 0; i < 20; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        uiAdapter->Update(0.1f);
        if (callbackCalled) break;
    }
    
    thermalManager->Stop();
    
    EXPECT_TRUE(callbackCalled.load());
}

// ============================================================================
// EventDispatcher Tests
// ============================================================================

class EventDispatcherTest : public ::testing::Test {
protected:
    EventDispatcher dispatcher;
};

TEST_F(EventDispatcherTest, SubscribeAndDispatch) {
    std::atomic<int> callCount{0};
    
    auto subId = dispatcher.Subscribe([&callCount](const Core::ThermalEvent& event) {
        callCount++;
    });
    
    LogEvent event;
    event.timestamp = std::chrono::steady_clock::now();
    event.level = LogLevel::Info;
    event.message = "Test";
    
    dispatcher.Dispatch(event);
    EXPECT_EQ(callCount.load(), 1);
    
    dispatcher.Dispatch(event);
    EXPECT_EQ(callCount.load(), 2);
    
    dispatcher.Unsubscribe(subId);
    dispatcher.Dispatch(event);
    EXPECT_EQ(callCount.load(), 2);  // No longer subscribed
}

TEST_F(EventDispatcherTest, MultipleSubscribers) {
    std::atomic<int> count1{0}, count2{0};
    
    dispatcher.Subscribe([&count1](const Core::ThermalEvent&) { count1++; });
    dispatcher.Subscribe([&count2](const Core::ThermalEvent&) { count2++; });
    
    LogEvent event;
    event.timestamp = std::chrono::steady_clock::now();
    event.level = LogLevel::Info;
    event.message = "Test";
    
    dispatcher.Dispatch(event);
    
    EXPECT_EQ(count1.load(), 1);
    EXPECT_EQ(count2.load(), 1);
}

TEST_F(EventDispatcherTest, ClearAll) {
    std::atomic<int> callCount{0};
    
    dispatcher.Subscribe([&callCount](const Core::ThermalEvent&) { callCount++; });
    dispatcher.Subscribe([&callCount](const Core::ThermalEvent&) { callCount++; });
    
    dispatcher.Clear();
    
    LogEvent event;
    event.timestamp = std::chrono::steady_clock::now();
    event.level = LogLevel::Info;
    event.message = "Test";
    
    dispatcher.Dispatch(event);
    EXPECT_EQ(callCount.load(), 0);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

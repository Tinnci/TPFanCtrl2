#include <gtest/gtest.h>
#include <memory>
#include "ECManager.h"
#include "SensorManager.h"
#include "FanController.h"
#include "MockIOProvider.h"
#include "ConfigManager.h"

// Define global config for tests
ConfigManager* g_Config = nullptr;

class FanControlTest : public ::testing::Test {
protected:
    std::shared_ptr<MockIOProvider> mockIO;
    std::shared_ptr<ECManager> ecManager;
    std::shared_ptr<SensorManager> sensorManager;
    std::shared_ptr<FanController> fanController;

    void SetUp() override {
        if (!g_Config) g_Config = new ConfigManager();
        mockIO = std::make_shared<MockIOProvider>();
        ecManager = std::make_shared<ECManager>(mockIO, nullptr);
        sensorManager = std::make_shared<SensorManager>(ecManager);
        fanController = std::make_shared<FanController>(ecManager);
    }
};

TEST_F(FanControlTest, SmartControlLogic) {
    // Setup sensors
    sensorManager->SetSensorName(0, "CPU");
    mockIO->SetECByte(0x78, 45); // CPU temp = 45

    // Setup smart levels
    std::vector<SmartLevel> levels = {
        {50, 0, 2, 2},  // 50C -> Fan 0
        {60, 3, 2, 2},  // 60C -> Fan 3
        {70, 7, 2, 2},  // 70C -> Fan 7
        {-1, 0, 0, 0}
    };

    // Test 1: Temp below first level
    sensorManager->UpdateSensors(false, false, false);
    int maxIndex = 0;
    EXPECT_EQ(sensorManager->GetMaxTemp(maxIndex, ""), 45);
    
    fanController->UpdateSmartControl(45, levels);
    
    // Test 2: Temp reaches 55
    mockIO->SetECByte(0x78, 55);
    for(int i=0; i<5; i++) sensorManager->UpdateSensors(false, false, false);
    int maxTemp = sensorManager->GetMaxTemp(maxIndex, "");
    fanController->UpdateSmartControl(maxTemp, levels);
    EXPECT_EQ(fanController->GetCurrentFanCtrl(), 0);

    // Test 3: Temp reaches 65
    mockIO->SetECByte(0x78, 65);
    for(int i=0; i<5; i++) sensorManager->UpdateSensors(false, false, false);
    maxTemp = sensorManager->GetMaxTemp(maxIndex, "");
    fanController->UpdateSmartControl(maxTemp, levels);
    EXPECT_EQ(fanController->GetCurrentFanCtrl(), 3);

    // Test 4: Hysteresis check (cooling down)
    mockIO->SetECByte(0x78, 59); // Should stay at Fan 3 because 59 > 60 - 2
    for(int i=0; i<5; i++) sensorManager->UpdateSensors(false, false, false);
    maxTemp = sensorManager->GetMaxTemp(maxIndex, "");
    fanController->UpdateSmartControl(maxTemp, levels);
    EXPECT_EQ(fanController->GetCurrentFanCtrl(), 3);

    mockIO->SetECByte(0x78, 57); // Should drop to Fan 0 because 57 < 60 - 2
    for(int i=0; i<5; i++) sensorManager->UpdateSensors(false, false, false);
    maxTemp = sensorManager->GetMaxTemp(maxIndex, "");
    fanController->UpdateSmartControl(maxTemp, levels);
    EXPECT_EQ(fanController->GetCurrentFanCtrl(), 0);
}

TEST_F(FanControlTest, SensorOffsets) {
    sensorManager->SetSensorName(0, "CPU");
    sensorManager->SetOffset(0, 5, 0, 0); // Offset 5
    
    mockIO->SetECByte(0x78, 50);
    sensorManager->UpdateSensors(true, false, false);
    
    int maxIndex = 0;
    EXPECT_EQ(sensorManager->GetMaxTemp(maxIndex, ""), 45); // 50 - 5
}

TEST_F(FanControlTest, IgnoredSensors) {
    sensorManager->SetSensorName(0, "CPU");
    sensorManager->SetSensorName(1, "GPU");
    
    mockIO->SetECByte(0x78, 60); // CPU
    mockIO->SetECByte(0x79, 80); // GPU
    
    sensorManager->UpdateSensors(false, false, false);
    
    int maxIndex = 0;
    EXPECT_EQ(sensorManager->GetMaxTemp(maxIndex, "GPU"), 60); // GPU ignored, CPU is max
}

TEST_F(FanControlTest, DualFanControl) {
    // This is a placeholder for more complex dual fan logic tests
    fanController->SetFanLevel(7, true); 
    EXPECT_EQ(fanController->GetCurrentFanCtrl(), 7);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

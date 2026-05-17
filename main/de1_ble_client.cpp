#include "de1_ble_client.h"
#include <algorithm>
extern "C" void appLogf(const char* fmt, ...);

// Global instance
DE1BLEClient de1Client;

// Scan callback class (NimBLE-Arduino API)
class DE1ScanCallbacks : public NimBLEScanCallbacks {
private:
    DE1BLEClient* pClient;
    
public:
    explicit DE1ScanCallbacks(DE1BLEClient* client) : pClient(client) {}

    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        pClient->onScanResult(advertisedDevice);
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        Serial.printf("DE1 scan ended, reason: %d, found %d devices\n", reason, results.getCount());
    }
};

// Client callback class
class DE1ClientCallbacks : public NimBLEClientCallbacks {
private:
    DE1BLEClient* pClient;
    
public:
    DE1ClientCallbacks(DE1BLEClient* client) : pClient(client) {}
    
    void onConnect(NimBLEClient* pClient) override {
        this->pClient->onConnect(pClient);
    }
    
    void onDisconnect(NimBLEClient* pClient, int reason) override {
        this->pClient->onDisconnect(pClient, reason);
    }
};

// Constructor
DE1BLEClient::DE1BLEClient() {
    pBLEScan = nullptr;
    pClient = nullptr;
    pRemoteService = nullptr;
    
    // Initialize all characteristic pointers
    pStateInfoChar = nullptr;
    pTemperaturesChar = nullptr;
    pShotSampleChar = nullptr;
    pShotSettingsChar = nullptr;
    pWaterLevelsChar = nullptr;
    pReadFromMMRChar = nullptr;
    pWriteToMMRChar = nullptr;
    pVersionsChar = nullptr;
    pRequestedStateChar = nullptr;
    
    // Initialize connection state
    deviceFound = false;
    isConnected = false;
    deviceReady = false;
    lastMMROperationTime = 0;
    
    // Initialize data structures
    initializeDataStructures();
}

// Destructor
DE1BLEClient::~DE1BLEClient() {
    disconnect();
}

// Scan for DE1 devices
bool DE1BLEClient::scanForDE1Devices(int scanTime) {
    appLogf("Starting DE1 device scan...");
    
    // Clear previous discoveries
    discoveredDevices.clear();
    deviceFound = false;
    
    // Create scan object if it doesn't exist
    if (pBLEScan == nullptr) {
        pBLEScan = NimBLEDevice::getScan();
        pBLEScan->setScanCallbacks(new DE1ScanCallbacks(this), true /* wantDuplicates */);
        pBLEScan->setActiveScan(true);
        // Use high duty scan for maximum discovery likelihood
        pBLEScan->setInterval(80);  // units of 0.625ms -> 50ms
        pBLEScan->setWindow(80);    // match interval for near-continuous scanning
        pBLEScan->setDuplicateFilter(false);
    }

    // Clear previous results to avoid acting on stale devices
    pBLEScan->clearResults();

    // Start scanning
    bool scanStarted = pBLEScan->start(scanTime, false);
    if (!scanStarted) {
        appLogf("Scan failed to start");
        return false;
    }
    
    appLogf("Scan complete. Found devices: %d", pBLEScan->getResults().getCount());
    
    return deviceFound;
}

// Connect to DE1 device
bool DE1BLEClient::connectToDE1(const NimBLEAddress& address) {
    appLogf("=== STARTING DE1 CONNECTION SEQUENCE ===");
    appLogf("Target address: %s", address.toString().c_str());
    appLogf("Target device: %s", connectedDevice.name.c_str());
    
    try {
        // Create client if it doesn't exist
        if (pClient == nullptr) {
            appLogf("Creating BLE client...");
            pClient = NimBLEDevice::createClient();
            if (pClient == nullptr) {
                appLogf("❌ Failed to create BLE client");
                return false;
            }
            appLogf("✅ BLE client created successfully");
            pClient->setClientCallbacks(new DE1ClientCallbacks(this));
            
            // Set connection parameters for DE1 compatibility
            // Use more conservative parameters for ESP32-DE1 connection
            pClient->setConnectionParams(24, 48, 0, 200); // 30-60ms intervals, 2000ms timeout
            pClient->setConnectTimeout(60); // 60 second timeout for DE1 connections
            appLogf("✅ Connection parameters configured for DE1");
        }
        
        // Ensure we're not already connected
        if (pClient->isConnected()) {
            appLogf("Client already connected, disconnecting first");
            pClient->disconnect();
            delay(1000); // Give time for clean disconnect
        }
        
        // Connect to the device
        appLogf("⏳ Establishing BLE connection...");
        if (!pClient->connect(address)) {
            appLogf("❌ Failed to establish BLE connection");
            return false;
        }
        
        appLogf("🔗 BLE connection established successfully!");
        
        // Check MTU for DE1 communication
        appLogf("🔧 Checking MTU for DE1 communication...");
        uint16_t mtu = pClient->getMTU();
        appLogf("Current MTU: %d bytes", mtu);
        if (mtu < 185) {
            appLogf("⚠️ MTU is smaller than optimal for DE1 (%d < 185), but continuing", mtu);
        } else {
            appLogf("✅ MTU is sufficient for DE1 communication");
        }
        
        // Get the DE1 service (ensure discovery happened)
        appLogf("🔍 Discovering DE1 service...");
        appLogf("Looking for service UUID: %s", DE1_SERVICE_UUID);
        pRemoteService = pClient->getService(DE1_SERVICE_UUID);
        if (pRemoteService == nullptr) {
            appLogf("⚠️ Service not found on first attempt; discovering attributes and retrying...");
            pClient->discoverAttributes();
            pRemoteService = pClient->getService(DE1_SERVICE_UUID);
            if (pRemoteService == nullptr) {
                appLogf("❌ Failed to find DE1 service - device may not be a DE1");
                pClient->disconnect();
                return false;
            }
        }
        
        appLogf("✅ Found DE1 service successfully!");
        
        // Discover and subscribe to all characteristics
        appLogf("🔍 Discovering characteristics...");
        if (!discoverAndSubscribeToCharacteristics()) {
            appLogf("❌ Failed to discover required characteristics");
            pClient->disconnect();
            return false;
        }
        
        // Update connected device info
        connectedDevice.address = address;
        connectedDevice.isConnected = true;
        isConnected = true;
        appLogf("✅ Device state updated");
        
        // Notify UI of connection state change
        if (uiConnectionCallback) {
            uiConnectionCallback(true);
        }
        
        appLogf("✅ Characteristics discovered and subscribed");
        
        // Perform device setup after successful connection
        appLogf("🔧 Performing device setup...");
        if (!performDeviceSetup()) {
            appLogf("⚠️ Device setup failed - device may have limited functionality");
            // Don't disconnect - device might still be partially functional
        } else {
            deviceReady = true;
            appLogf("✅ Device setup completed successfully");
        }
        
        appLogf("🎉 === CONNECTION SEQUENCE COMPLETE ===");
        appLogf("✅ DE1 connection established successfully!");
        appLogf("Device: %s (%s)", connectedDevice.name.c_str(), connectedDevice.address.toString().c_str());
        return true;
        
    } catch (const std::exception& e) {
        appLogf("Exception during connection: %s", e.what());
        if (pClient != nullptr && pClient->isConnected()) {
            pClient->disconnect();
        }
        resetConnectionState();
        return false;
    } catch (...) {
        appLogf("Unknown exception during connection");
        if (pClient != nullptr && pClient->isConnected()) {
            pClient->disconnect();
        }
        resetConnectionState();
        return false;
    }
}

// Disconnect from DE1 device
void DE1BLEClient::disconnect() {
    if (pClient != nullptr && pClient->isConnected()) {
    appLogf("Disconnecting from DE1 device");
        pClient->disconnect();
    }
    
    resetConnectionState();
}

// Scan result callback
void DE1BLEClient::onScanResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    // Dual name checking (NimBLE exposes only the device name); sanitize it
    std::string deviceName = advertisedDevice->getName();
    auto sanitize = [](std::string name) {
        // Remove at first embedded null and trailing whitespace
        auto it = std::find(name.begin(), name.end(), '\0');
        if (it != name.end()) name.erase(it, name.end());
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t' || name.back() == '\r' || name.back() == '\n')) {
            name.pop_back();
        }
        return name;
    };
    deviceName = sanitize(deviceName);
    
    const char* nameStr = deviceName.empty() ? "(unnamed)" : deviceName.c_str();
    appLogf("=== DISCOVERED BLE DEVICE ===");
    appLogf("  Name: '%s'", nameStr);
    appLogf("  Raw Name: '%s'", advertisedDevice->getName().c_str());
    appLogf("  Address: %s", advertisedDevice->getAddress().toString().c_str());
    appLogf("  RSSI: %d dBm", advertisedDevice->getRSSI());
    appLogf("  Has Service UUID: %s", advertisedDevice->haveServiceUUID() ? "Yes" : "No");
    if (advertisedDevice->haveServiceUUID()) {
        // Use getServiceUUID() instead of getServiceUUIDs()
        appLogf("  Service UUID: %s", advertisedDevice->getServiceUUID().toString().c_str());
    }
    
    // Store all discovered devices
    DE1Device device;
    device.address = advertisedDevice->getAddress();
    device.name = deviceName;
    device.rssi = advertisedDevice->getRSSI();
    device.isConnected = false;
    discoveredDevices.push_back(device);
    
    // Check for DE1 devices using pattern matching on both names
    bool isDE1Device = false;
    std::string matchedName = "";
    bool isDE1Match = false;
    bool isNordicMatch = false;
    
    // Helper function to check DE1 patterns
    auto checkDE1Pattern = [&](const std::string& name) -> bool {
        if (name.empty()) return false;
        
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        // Check for various DE1 patterns found in real devices
        return lowerName.find("de1") != std::string::npos ||
               lowerName.find("decent") != std::string::npos ||
               lowerName == "de1pro" ||
               lowerName == "de1+" ||
               lowerName.find("de1 ") != std::string::npos;  // DE1 with space
    };
    
    // Helper function to check Nordic patterns
    auto checkNordicPattern = [&](const std::string& name) -> bool {
        if (name.empty()) return false;
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        return lowerName == "nrf5x";
    };
    
    // Check device name for DE1 pattern
    if (checkDE1Pattern(deviceName)) {
        appLogf("*** DE1 DEVICE FOUND (DE1 device name pattern: %s) ***", deviceName.c_str());
        isDE1Device = true;
        isDE1Match = true;
        matchedName = deviceName;
    }
    
    // Check device name for Nordic pattern if not already found
    if (!isDE1Device && checkNordicPattern(deviceName)) {
        appLogf("*** DE1 DEVICE FOUND (nRF5x device name pattern) ***");
        isDE1Device = true;
        isNordicMatch = true;
        matchedName = deviceName;
    }
    
    // Also check if device advertises the DE1 service UUID
    if (!isDE1Device && advertisedDevice->haveServiceUUID() && 
        advertisedDevice->isAdvertisingService(NimBLEUUID(DE1_SERVICE_UUID))) {
        
        appLogf("*** DE1 DEVICE FOUND (service UUID match) ***");
        isDE1Device = true;
        matchedName = deviceName.empty() ? "Service UUID Match" : deviceName;
    }
    
    if (isDE1Device) {
        // Enhanced logging for matched devices
        appLogf("DE1 Device discovered:");
        appLogf("  Device ID: %s", advertisedDevice->getAddress().toString().c_str());
        appLogf("  Device Name: %s", deviceName.c_str());
        appLogf("  Matched Name: %s", matchedName.c_str());
        appLogf("  Is DE1: %s", isDE1Match ? "true" : "false");
        appLogf("  Is Nordic: %s", isNordicMatch ? "true" : "false");
        appLogf("  RSSI: %d dBm", advertisedDevice->getRSSI());
        
        // Store device info for connection - with safety checks
        try {
            connectedDevice.address = advertisedDevice->getAddress();
            connectedDevice.name = deviceName;
            connectedDevice.rssi = advertisedDevice->getRSSI();
            connectedDevice.isConnected = false;
        } catch (const std::exception& e) {
            appLogf("❌ Error storing device info: %s", e.what());
            return; // Don't set deviceFound if we can't store the device
        }
        
        deviceFound = true;
        
        // Stop scanning when DE1 device is found - but don't auto-connect from callback
        appLogf("=== DE1 DEVICE DISCOVERED - STOPPING SCAN ===");
        appLogf("Found DE1 device: %s (%s)", deviceName.c_str(), advertisedDevice->getAddress().toString().c_str());
        appLogf("Connection will be initiated from main thread to avoid callback crashes");
        
        if (pBLEScan != nullptr && pBLEScan->isScanning()) {
            pBLEScan->stop();
        }
    }
}

// Connect callback
void DE1BLEClient::onConnect(NimBLEClient* pClient) {
    appLogf("🎉 BLE CONNECTION ESTABLISHED!");
    appLogf("Connected to DE1: %s", connectedDevice.name.c_str());
    appLogf("Address: %s", connectedDevice.address.toString().c_str());
    appLogf("RSSI: %d dBm", connectedDevice.rssi);
    appLogf("Starting service discovery and setup...");
}

// Disconnect callback
void DE1BLEClient::onDisconnect(NimBLEClient* pClient, int reason) {
    appLogf("Disconnected from DE1 device, reason: %d", reason);
    
    // Comprehensive disconnect handling
    resetConnectionState();
    
    // Notify UI of disconnection
    if (uiConnectionCallback) {
        uiConnectionCallback(false);
    }
    
    // Log disconnect reason details
    switch (reason) {
        case 0x08:
            appLogf("Disconnect reason: Connection timeout");
            break;
        case 0x13:
            appLogf("Disconnect reason: Remote user terminated connection");
            break;
        case 0x16:
            appLogf("Disconnect reason: Connection terminated by local host");
            break;
        case 0x3D:
            appLogf("Disconnect reason: Unacceptable connection parameters");
            break;
        default:
            appLogf("Disconnect reason: Unknown (0x%02X)", reason);
            break;
    }
    
    appLogf("Connection state reset, ready for reconnection");
}

// State info notification callback
void DE1BLEClient::onStateInfoNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length >= 2) {
        uint8_t newMajorState = pData[0];
        uint8_t newMinorState = pData[1];
        
        // Check for error states and handle them appropriately
        bool isErrorState = false;
        bool isCriticalError = false;
        
        if (newMajorState == DE1_STATE_FATAL_ERROR) {
            isErrorState = true;
            isCriticalError = true;
            Serial.println("🚨 FATAL ERROR STATE DETECTED!");
        }
        
        // Check for error minor states
        if (newMinorState >= DE1_MINOR_ERROR_NAN && newMinorState <= DE1_MINOR_ERROR_BOOT_FILL) {
            isErrorState = true;
            if (newMinorState == DE1_MINOR_ERROR_ASSERTION || 
                newMinorState == DE1_MINOR_ERROR_FLASH ||
                newMinorState == DE1_MINOR_ERROR_OOM) {
                isCriticalError = true;
            }
            Serial.printf("⚠️  Error minor state detected: %s\n", minorStateToString(newMinorState).c_str());
        }
        
        // Update current state
        currentState.majorState = newMajorState;
        currentState.minorState = newMinorState;
        
        // Enhanced logging based on state type
        if (isCriticalError) {
            Serial.printf("🚨 CRITICAL ERROR: %s - %s\n", 
                         majorStateToString(currentState.majorState).c_str(),
                         minorStateToString(currentState.minorState).c_str());
        } else if (isErrorState) {
            Serial.printf("⚠️  DE1 Error State: %s - %s\n", 
                         majorStateToString(currentState.majorState).c_str(),
                         minorStateToString(currentState.minorState).c_str());
        } else {
            Serial.printf("ℹ️  DE1 State Update: %s - %s\n", 
                         majorStateToString(currentState.majorState).c_str(),
                         minorStateToString(currentState.minorState).c_str());
        }
        
        // Mark device as ready when it reaches idle state
        if (newMajorState == DE1_STATE_IDLE && !deviceReady) {
            deviceReady = true;
            Serial.println("✅ DE1 device is now ready");
        }
    } else {
        Serial.printf("⚠️  Invalid state notification length: %d bytes\n", length);
    }
}

// Temperatures notification callback
void DE1BLEClient::onTemperaturesNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length >= 16) {
        // Parse temperature data according to DE1 protocol
        // Temperatures are uint16 big-endian, divide by 256
        currentTemperatures.waterHeaterTemp = ((pData[0] << 8) | pData[1]) / 256.0f;
        currentTemperatures.steamHeaterTemp = ((pData[2] << 8) | pData[3]) / 256.0f;
        currentTemperatures.groupHeaterTemp = ((pData[4] << 8) | pData[5]) / 256.0f;
        currentTemperatures.coldWaterTemp = ((pData[6] << 8) | pData[7]) / 256.0f;
        currentTemperatures.targetWaterHeaterTemp = ((pData[8] << 8) | pData[9]) / 256.0f;
        currentTemperatures.targetSteamHeaterTemp = ((pData[10] << 8) | pData[11]) / 256.0f;
        currentTemperatures.targetGroupHeaterTemp = ((pData[12] << 8) | pData[13]) / 256.0f;
        currentTemperatures.targetColdWaterTemp = ((pData[14] << 8) | pData[15]) / 256.0f;
        
        Serial.printf("DE1 Temperatures: WH=%.1f°C, SH=%.1f°C, GH=%.1f°C, CW=%.1f°C\n",
                     currentTemperatures.waterHeaterTemp,
                     currentTemperatures.steamHeaterTemp,
                     currentTemperatures.groupHeaterTemp,
                     currentTemperatures.coldWaterTemp);
    }
}

// Shot sample notification callback (real-time shot data)
void DE1BLEClient::onShotSampleNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length >= 19) {
        // Parse shot sample data according to DE1 protocol
        currentShotSample.sampleTime = (pData[0] << 8) | pData[1];
        currentShotSample.groupPressure = ((pData[2] << 8) | pData[3]) / 4096.0f;
        currentShotSample.groupFlow = ((pData[4] << 8) | pData[5]) / 4096.0f;
        currentShotSample.mixTemperature = ((pData[6] << 8) | pData[7]) / 256.0f;
        currentShotSample.headTemperature = ((pData[8] << 24) | (pData[9] << 16) | (pData[10] << 8) | pData[11]) / 65536.0f;
        currentShotSample.setMixTemperature = ((pData[11] << 8) | pData[12]) / 256.0f;
        currentShotSample.setHeadTemperature = ((pData[13] << 8) | pData[14]) / 256.0f;
        currentShotSample.setGroupPressure = pData[15] / 16.0f;
        currentShotSample.setGroupFlow = pData[16] / 16.0f;
        currentShotSample.frameNumber = pData[17];
        currentShotSample.steamTemperature = pData[18];
        
        Serial.printf("DE1 Shot: P=%.2fbar, F=%.2fml/s, T=%.1f°C, Frame=%d\n",
                     currentShotSample.groupPressure,
                     currentShotSample.groupFlow,
                     currentShotSample.mixTemperature,
                     currentShotSample.frameNumber);
    }
}

// Shot settings notification callback
void DE1BLEClient::onShotSettingsNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length >= 8) {
        // Parse shot settings data according to DE1 protocol
        currentShotSettings.steamSettings = pData[0];
        currentShotSettings.targetSteamTemp = pData[1];
        currentShotSettings.targetSteamLength = pData[2];
        currentShotSettings.targetHotWaterTemp = pData[3];
        currentShotSettings.targetHotWaterVolume = pData[4];
        currentShotSettings.targetHotWaterLength = pData[5];
        currentShotSettings.targetEspressoVolume = pData[6];
        currentShotSettings.targetGroupTemp = ((pData[7] << 8) | pData[8]) / 256.0f; // U16P8 format
        
        Serial.printf("DE1 Shot Settings: Steam=%d°C/%ds, HW=%d°C/%dml/%ds, Espresso=%dml, Group=%.1f°C\n",
                     currentShotSettings.targetSteamTemp,
                     currentShotSettings.targetSteamLength,
                     currentShotSettings.targetHotWaterTemp,
                     currentShotSettings.targetHotWaterVolume,
                     currentShotSettings.targetHotWaterLength,
                     currentShotSettings.targetEspressoVolume,
                     currentShotSettings.targetGroupTemp);
    }
}

// Water levels notification callback
void DE1BLEClient::onWaterLevelsNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length >= 4) {
        // Parse water levels data according to DE1 protocol (toU16P8 format)
        currentWaterLevels.minimumLevel = ((pData[0] << 8) | pData[1]) / 256.0f;
        currentWaterLevels.maximumLevel = ((pData[2] << 8) | pData[3]) / 256.0f;
        
        Serial.printf("DE1 Water Levels: Min=%.2f, Max=%.2f\n",
                     currentWaterLevels.minimumLevel,
                     currentWaterLevels.maximumLevel);
    }
}

// MMR read notification callback
void DE1BLEClient::onMMRReadNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length >= 4) {
        // Parse MMR read response according to DE1 protocol
        uint8_t responseLength = pData[0];
        uint32_t address = (pData[1] << 16) | (pData[2] << 8) | pData[3];
        
        appLogf("MMR Read Response: addr=0x%08X, len=%d", address, responseLength);
        
        // Keep raw dump on serial for brevity of on-screen logs
        Serial.print("  Data: ");
        for (int i = 4; i < (int)length && i < 4 + responseLength * 4; i++) {
            Serial.printf("%02X ", pData[i]);
        }
        Serial.println();
        
        // Remove completed operation from pending list
        for (auto it = pendingMMROperations.begin(); it != pendingMMROperations.end(); ++it) {
            if (!it->isWrite && it->address == address) {
                pendingMMROperations.erase(it);
                break;
            }
        }
    }
}

// Utility functions
std::string DE1BLEClient::stateToString(const DE1MachineState& state) {
    return majorStateToString(state.majorState) + " - " + minorStateToString(state.minorState);
}

std::string DE1BLEClient::majorStateToString(uint8_t majorState) {
    switch (majorState) {
        case DE1_STATE_SLEEP: return "Sleep";
        case DE1_STATE_GOING_TO_SLEEP: return "Going to Sleep";
        case DE1_STATE_IDLE: return "Idle";
        case DE1_STATE_BUSY: return "Busy";
        case DE1_STATE_ESPRESSO: return "Espresso";
        case DE1_STATE_STEAM: return "Steam";
        case DE1_STATE_HOT_WATER: return "Hot Water";
        case DE1_STATE_SHORT_CAL: return "Short Calibration";
        case DE1_STATE_SELF_TEST: return "Self Test";
        case DE1_STATE_LONG_CAL: return "Long Calibration";
        case DE1_STATE_DESCALE: return "Descale";
        case DE1_STATE_FATAL_ERROR: return "Fatal Error";
        case DE1_STATE_INIT: return "Init";
        case DE1_STATE_NO_REQUEST: return "No Request";
        case DE1_STATE_SKIP_TO_NEXT: return "Skip to Next";
        case DE1_STATE_HOT_WATER_RINSE: return "Hot Water Rinse";
        case DE1_STATE_STEAM_RINSE: return "Steam Rinse";
        case DE1_STATE_REFILL: return "Refill";
        case DE1_STATE_CLEAN: return "Clean";
        case DE1_STATE_IN_BOOT_LOADER: return "In Boot Loader";
        case DE1_STATE_AIR_PURGE: return "Air Purge";
        case DE1_STATE_SCHEDULED_WAKE: return "Scheduled Wake";
        default: return "Unknown";
    }
}

std::string DE1BLEClient::minorStateToString(uint8_t minorState) {
    switch (minorState) {
        case DE1_MINOR_NO_STATE: return "No State";
        case DE1_MINOR_HEAT_WATER_TANK: return "Heat Water Tank";
        case DE1_MINOR_HEAT_WATER_HEATER: return "Heat Water Heater";
        case DE1_MINOR_STABILIZE_MIX_TEMP: return "Stabilize Mix Temp";
        case DE1_MINOR_PRE_INFUSE: return "Pre-Infuse";
        case DE1_MINOR_POUR: return "Pour";
        case DE1_MINOR_FLUSH: return "Flush";
        case DE1_MINOR_STEAMING: return "Steaming";
        case DE1_MINOR_DESCALE_INIT: return "Descale Init";
        case DE1_MINOR_DESCALE_FILL_GROUP: return "Descale Fill Group";
        case DE1_MINOR_DESCALE_RETURN: return "Descale Return";
        case DE1_MINOR_DESCALE_GROUP: return "Descale Group";
        case DE1_MINOR_DESCALE_STEAM: return "Descale Steam";
        case DE1_MINOR_CLEAN_INIT: return "Clean Init";
        case DE1_MINOR_CLEAN_FILL_GROUP: return "Clean Fill Group";
        case DE1_MINOR_CLEAN_SOAK: return "Clean Soak";
        case DE1_MINOR_CLEAN_GROUP: return "Clean Group";
        case DE1_MINOR_PAUSED_REFILL: return "Paused Refill";
        case DE1_MINOR_PAUSED_STEAM: return "Paused Steam";
        case DE1_MINOR_USER_NOT_PRESENT: return "User Not Present";
        case DE1_MINOR_STEAM_PUFF: return "Steam Puff";
        // Error states
        case DE1_MINOR_ERROR_NAN: return "Error: NaN";
        case DE1_MINOR_ERROR_INF: return "Error: Inf";
        case DE1_MINOR_ERROR_GENERIC: return "Error: Generic";
        case DE1_MINOR_ERROR_ACC: return "Error: ACC";
        case DE1_MINOR_ERROR_TSENSOR: return "Error: Temperature Sensor";
        case DE1_MINOR_ERROR_PSENSOR: return "Error: Pressure Sensor";
        case DE1_MINOR_ERROR_WLEVEL: return "Error: Water Level";
        case DE1_MINOR_ERROR_DIP: return "Error: DIP";
        case DE1_MINOR_ERROR_ASSERTION: return "Error: Assertion";
        case DE1_MINOR_ERROR_UNSAFE: return "Error: Unsafe";
        case DE1_MINOR_ERROR_INVALID_PARM: return "Error: Invalid Parameter";
        case DE1_MINOR_ERROR_FLASH: return "Error: Flash";
        case DE1_MINOR_ERROR_OOM: return "Error: Out of Memory";
        case DE1_MINOR_ERROR_DEADLINE: return "Error: Deadline";
        case DE1_MINOR_ERROR_HI_CURRENT: return "Error: High Current";
        case DE1_MINOR_ERROR_LO_CURRENT: return "Error: Low Current";
        case DE1_MINOR_ERROR_BOOT_FILL: return "Error: Boot Fill";
        default: return "Unknown";
    }
}

std::string DE1BLEClient::getDeviceListAsString() const {
    if (discoveredDevices.empty()) {
        return "No devices found";
    }
    
    std::string result = "Found " + std::to_string(discoveredDevices.size()) + " devices:\n";
    
    for (size_t i = 0; i < discoveredDevices.size() && i < 10; ++i) { // Limit to 10 devices
        const DE1Device& device = discoveredDevices[i];
        std::string name = device.name.empty() ? "(unnamed)" : device.name;
        result += name + " (" + std::to_string(device.rssi) + "dBm)\n";
    }
    
    if (discoveredDevices.size() > 10) {
        result += "... and " + std::to_string(discoveredDevices.size() - 10) + " more";
    }
    
    return result;
}

// Initialize all data structures to default values
void DE1BLEClient::initializeDataStructures() {
    // Initialize state
    currentState.majorState = DE1_STATE_INIT;
    currentState.minorState = DE1_MINOR_NO_STATE;
    
    // Initialize temperatures
    memset(&currentTemperatures, 0, sizeof(currentTemperatures));
    
    // Initialize shot sample
    memset(&currentShotSample, 0, sizeof(currentShotSample));
    
    // Initialize shot settings
    memset(&currentShotSettings, 0, sizeof(currentShotSettings));
    
    // Initialize water levels
    memset(&currentWaterLevels, 0, sizeof(currentWaterLevels));
    
    // Clear MMR operations
    pendingMMROperations.clear();
}

// Reset connection state
void DE1BLEClient::resetConnectionState() {
    isConnected = false;
    deviceReady = false;
    connectedDevice.isConnected = false;
    
    // Reset all characteristic pointers
    pStateInfoChar = nullptr;
    pTemperaturesChar = nullptr;
    pShotSampleChar = nullptr;
    pShotSettingsChar = nullptr;
    pWaterLevelsChar = nullptr;
    pReadFromMMRChar = nullptr;
    pWriteToMMRChar = nullptr;
    pVersionsChar = nullptr;
    pRequestedStateChar = nullptr;
    
    // Clear pending operations
    pendingMMROperations.clear();
    
    // Reinitialize data structures
    initializeDataStructures();
}

// Discover and subscribe to all DE1 characteristics
bool DE1BLEClient::discoverAndSubscribeToCharacteristics() {
    appLogf("🔍 Discovering DE1 characteristics...");
    
    // Force complete service discovery to ensure all characteristics are available
    appLogf("🔄 Performing complete service discovery...");
    pClient->discoverAttributes();
    delay(500); // Allow discovery to complete
    
    // Get critical characteristics for real-time monitoring
    appLogf("📡 Discovering critical characteristics...");
    pStateInfoChar = pRemoteService->getCharacteristic(DE1_CHAR_STATE_INFO);
    if (pStateInfoChar != nullptr) appLogf("✅ Found StateInfo characteristic");
    
    pTemperaturesChar = pRemoteService->getCharacteristic(DE1_CHAR_TEMPERATURES);
    if (pTemperaturesChar != nullptr) appLogf("✅ Found Temperatures characteristic");
    
    pShotSampleChar = pRemoteService->getCharacteristic(DE1_CHAR_SHOT_SAMPLE);
    if (pShotSampleChar != nullptr) appLogf("✅ Found ShotSample characteristic");
    
    pShotSettingsChar = pRemoteService->getCharacteristic(DE1_CHAR_SHOT_SETTINGS);
    if (pShotSettingsChar != nullptr) appLogf("✅ Found ShotSettings characteristic");
    
    pWaterLevelsChar = pRemoteService->getCharacteristic(DE1_CHAR_WATER_LEVELS);
    if (pWaterLevelsChar != nullptr) appLogf("✅ Found WaterLevels characteristic");
    
    // Get MMR characteristics
    appLogf("🔧 Discovering MMR characteristics...");
    pReadFromMMRChar = pRemoteService->getCharacteristic(DE1_CHAR_READ_FROM_MMR);
    if (pReadFromMMRChar != nullptr) appLogf("✅ Found ReadFromMMR characteristic");
    
    pWriteToMMRChar = pRemoteService->getCharacteristic(DE1_CHAR_WRITE_TO_MMR);
    if (pWriteToMMRChar != nullptr) appLogf("✅ Found WriteToMMR characteristic");
    
    // Get additional characteristics
    appLogf("📋 Discovering additional characteristics...");
    pVersionsChar = pRemoteService->getCharacteristic(DE1_CHAR_VERSIONS);
    if (pVersionsChar != nullptr) appLogf("✅ Found Versions characteristic");
    
    pRequestedStateChar = pRemoteService->getCharacteristic(DE1_CHAR_REQUESTED_STATE);
    if (pRequestedStateChar != nullptr) appLogf("✅ Found RequestedState characteristic");
    
    // Check for essential characteristics - need at least StateInfo OR Temperatures
    bool hasRequired = (pStateInfoChar != nullptr) || (pTemperaturesChar != nullptr);
    if (!hasRequired) {
        appLogf("❌ No essential characteristics found (StateInfo or Temperatures)");
        return false;
    }
    
    // Log characteristic summary
    int foundChars = 0;
    if (pStateInfoChar != nullptr) foundChars++;
    if (pTemperaturesChar != nullptr) foundChars++;
    if (pShotSampleChar != nullptr) foundChars++;
    if (pShotSettingsChar != nullptr) foundChars++;
    if (pWaterLevelsChar != nullptr) foundChars++;
    if (pReadFromMMRChar != nullptr) foundChars++;
    if (pWriteToMMRChar != nullptr) foundChars++;
    if (pVersionsChar != nullptr) foundChars++;
    if (pRequestedStateChar != nullptr) foundChars++;
    
    appLogf("📊 Discovered %d/9 DE1 characteristics", foundChars);
    
    // Subscribe to notifications for critical characteristics
    int subscribed = 0;
    
    // Subscribe with error handling
    appLogf("🔔 Subscribing to characteristic notifications...");
    
    if (pStateInfoChar != nullptr && pStateInfoChar->canNotify()) {
        try {
            if (pStateInfoChar->subscribe(true, [this](NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
                this->onStateInfoNotify(pRemoteCharacteristic, pData, length, isNotify);
            })) {
                appLogf("✅ Subscribed to StateInfo notifications");
                subscribed++;
            } else {
                appLogf("⚠️ Failed to subscribe to StateInfo notifications");
            }
        } catch (const std::exception& e) {
            appLogf("❌ StateInfo subscription error: %s", e.what());
        }
        delay(100); // Prevent subscription flooding
    }
    
    if (pTemperaturesChar != nullptr && pTemperaturesChar->canNotify()) {
        try {
            if (pTemperaturesChar->subscribe(true, [this](NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
                this->onTemperaturesNotify(pRemoteCharacteristic, pData, length, isNotify);
            })) {
                appLogf("✅ Subscribed to Temperatures notifications");
                subscribed++;
            } else {
                appLogf("⚠️ Failed to subscribe to Temperatures notifications");
            }
        } catch (const std::exception& e) {
            appLogf("❌ Temperatures subscription error: %s", e.what());
        }
        delay(100);
    }
    
    if (pShotSampleChar != nullptr && pShotSampleChar->canNotify()) {
        try {
            if (pShotSampleChar->subscribe(true, [this](NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
                this->onShotSampleNotify(pRemoteCharacteristic, pData, length, isNotify);
            })) {
                appLogf("✅ Subscribed to ShotSample notifications");
                subscribed++;
            } else {
                appLogf("⚠️ Failed to subscribe to ShotSample notifications");
            }
        } catch (const std::exception& e) {
            appLogf("❌ ShotSample subscription error: %s", e.what());
        }
        delay(100);
    }
    
    // Only subscribe to essential characteristics to reduce connection load
    // Skip ShotSettings and WaterLevels for now to improve connection reliability
    
    if (pReadFromMMRChar != nullptr && pReadFromMMRChar->canNotify()) {
        try {
            if (pReadFromMMRChar->subscribe(true, [this](NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
                this->onMMRReadNotify(pRemoteCharacteristic, pData, length, isNotify);
            })) {
                appLogf("✅ Subscribed to MMR Read notifications");
                subscribed++;
            } else {
                appLogf("⚠️ Failed to subscribe to MMR Read notifications");
            }
        } catch (const std::exception& e) {
            appLogf("❌ MMR Read subscription error: %s", e.what());
        }
        delay(100);
    }
    
    if (subscribed > 0) {
        appLogf("🎉 Successfully subscribed to %d characteristic notifications", subscribed);
        appLogf("✅ Minimum connection requirements met");
        return true;
    } else {
        appLogf("❌ Failed to subscribe to any characteristics - connection may be unstable");
        return false;
    }
}

// Perform device setup sequence according to DE1 protocol - matches TypeScript implementation
bool DE1BLEClient::performDeviceSetup() {
    appLogf("🚀 Starting comprehensive DE1 device setup sequence...");
    
    // Wait for characteristics to stabilize after connection
    appLogf("⏳ Waiting for connection stabilization...");
    delay(2000);
    
    try {
        // Step 1: Skip profile writing to prevent brewing interference
        appLogf("📝 Step 1: Skipping profile write to prevent brewing interference...");
        
        // Step 2: Configure fan threshold (lower temperature = fan runs more often)
        appLogf("🌀 Step 2: Setting fan threshold to 80°C...");
        uint8_t fanThresholdData[4];
        formatUint32ForMMR(80, fanThresholdData);
        if (!writeMMR(0x803808, fanThresholdData, 4)) { // MMRAddr.FanThreshold
            appLogf("⚠️ Failed to set fan threshold");
        }
        
        // Step 3: Write default shot settings (basic settings only, no profile)
        appLogf("☕ Step 3: Writing default shot settings...");
        if (!writeDefaultShotSettings()) {
            appLogf("⚠️ Failed to write default shot settings");
        }
        
        // Step 4: Configure steam settings - using fixed values as requested
        appLogf("💨 Step 4: Configuring steam settings...");
        
        // Steam flow rate: 2.5 m/s converted to machine value (2.5 * 100 = 250)
        uint8_t steamFlowData[4];
        formatUint32ForMMR(250, steamFlowData);
        if (!writeMMR(0x803828, steamFlowData, 4)) { // MMRAddr.TargetSteamFlow
            appLogf("⚠️ Failed to set steam flow");
        } else {
            appLogf("💨 Set steam flow to 2.5 m/s (machine value: 250)");
        }
        
        // Steam start seconds: 0.7s * 100 = 70
        uint8_t steamStartData[4];
        formatUint32ForMMR(70, steamStartData);
        if (!writeMMR(0x80382C, steamStartData, 4)) { // MMRAddr.SteamStartSecs
            appLogf("⚠️ Failed to set steam start seconds");
        }
        
        // Step 5: Configure water levels
        appLogf("💧 Step 5: Setting water levels...");
        if (!configureWaterLevels()) {
            appLogf("⚠️ Failed to configure water levels");
        }
        
        // Step 6: Tweak heaters
        appLogf("🔥 Step 6: Tweaking heaters...");
        if (!tweakHeaters()) {
            appLogf("⚠️ Failed to tweak heaters");
        }
        
        // Step 7: Configure refill kit
        appLogf("🔄 Step 7: Configuring refill kit...");
        uint8_t refillKitData[4];
        formatUint32ForMMR(2, refillKitData); // RefillPreset.AutoDetect
        if (!writeMMR(0x803858, refillKitData, 4)) { // MMRAddr.RefillKitPresent
            appLogf("⚠️ Failed to configure refill kit");
        }
        
        // Step 8: Wait for stabilization
        appLogf("⏳ Step 8: Waiting for device stabilization (5s)...");
        delay(5000);
        
        // Step 9: Read initial state info
        appLogf("📊 Step 9: Reading initial state info...");
        if (pStateInfoChar != nullptr) {
            try {
                NimBLEAttValue stateData = pStateInfoChar->readValue();
                if (stateData.length() >= 2) {
                    currentState.majorState = static_cast<DE1MajorState>(stateData[0]);
                    currentState.minorState = static_cast<DE1MinorState>(stateData[1]);
                    appLogf("✅ Current state: %s - %s", 
                           majorStateToString(currentState.majorState).c_str(),
                           minorStateToString(currentState.minorState).c_str());
                }
            } catch (const std::exception& e) {
                appLogf("⚠️ Failed to read state data: %s", e.what());
            }
        }
        
        // Step 10: Final wait and heater voltage read
        appLogf("⏳ Step 10: Final stabilization wait (7s)...");
        delay(7000);
        
        appLogf("🔋 Step 11: Reading heater voltage...");
        if (!readMMR(0x803834, 1)) { // MMRAddr.HeaterV
            appLogf("⚠️ Failed to read heater voltage");
        }
        
        // Mark device as ready
        deviceReady = true;
        appLogf("✅ DE1 device setup sequence completed successfully!");
        
        return true;
        
    } catch (const std::exception& e) {
        appLogf("❌ Device setup failed: %s", e.what());
        return false;
    }
}

// Request a specific machine state
bool DE1BLEClient::requestState(DE1MajorState state) {
    if (pRequestedStateChar == nullptr) {
        appLogf("⚠️ RequestedState characteristic not available");
        return false;
    }
    
    if (!pClient->isConnected()) {
        appLogf("❌ Cannot request state - not connected");
        return false;
    }
    
    uint8_t stateData = static_cast<uint8_t>(state);
    
    try {
        appLogf("📤 Requesting machine state: %s (0x%02X)", majorStateToString(state).c_str(), stateData);
        bool result = pRequestedStateChar->writeValue(&stateData, 1);
        if (result) {
            appLogf("✅ State request sent successfully");
            return true;
        } else {
            appLogf("❌ Failed to send state request");
            return false;
        }
    } catch (const std::exception& e) {
        appLogf("❌ Exception during state request: %s", e.what());
        return false;
    }
}

// Read from Memory-Mapped Register - matches the proven TypeScript implementation
bool DE1BLEClient::readMMR(uint32_t address, uint8_t length) {
    if (!isConnected || !deviceReady) {
        appLogf("❌ MMR Read failed: Device not connected or ready");
        return false;
    }
    
    if (pReadFromMMRChar == nullptr) {
        appLogf("❌ MMR Read failed: ReadFromMMR characteristic not available");
        return false;
    }
    
    // Create MMR read request buffer - matches TypeScript implementation exactly
    uint8_t buffer[20];
    memset(buffer, 0, 20);
    
    // Write address in big endian (bytes 0-3) - EXACTLY like TypeScript
    buffer[0] = (address >> 24) & 0xFF;
    buffer[1] = (address >> 16) & 0xFF;
    buffer[2] = (address >> 8) & 0xFF;
    buffer[3] = address & 0xFF;
    
    // Overwrite first byte with length (matches TypeScript line 575)
    buffer[0] = length;
    
    appLogf("📤 MMR Read Request - Address: 0x%06X, Length: %d", address, length);
    appLogf("📦 Buffer: %02X %02X %02X %02X %02X %02X %02X %02X...", 
           buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
    
    try {
        bool result = pReadFromMMRChar->writeValue(buffer, 20);
        if (result) {
            appLogf("✅ MMR read request sent successfully");
            return true;
        } else {
            appLogf("❌ Failed to send MMR read request");
            return false;
        }
    } catch (const std::exception& e) {
        appLogf("❌ Exception during MMR read: %s", e.what());
        return false;
    }
    // notified on the same.
    if (pReadFromMMRChar == nullptr) {
        Serial.println("❌ MMR Read failed: ReadFromMMR characteristic not available");
        return false;
    }
    
    if (length == 0 || length > 16) {
        Serial.printf("❌ MMR Read failed: Invalid length %d (must be 1-16)\n", length);
        return false;
    }
    
    // Check for too many pending operations
    if (pendingMMROperations.size() > 10) {
        Serial.println("⚠️  Too many pending MMR operations, clearing old ones");
        pendingMMROperations.clear();
    }
    
    try {
        // Format MMR read request according to DE1 protocol
        uint8_t request[20] = {0};
        
        // Write address in big-endian format
        request[0] = length;  // Length overwrites first byte of address
        request[1] = (address >> 16) & 0xFF;
        request[2] = (address >> 8) & 0xFF;
        request[3] = address & 0xFF;
        
        pReadFromMMRChar->writeValue(request, 20);
        
        // Track the operation
        MMROperation op;
        op.address = address;
        op.length = length;
        op.isWrite = false;
        op.timestamp = millis();
        pendingMMROperations.push_back(op);
        
        lastMMROperationTime = millis();
        
        appLogf("MMR Read request sent: addr=0x%08X, len=%d", address, length);
        return true;
    } catch (const std::exception& e) {
        appLogf("MMR Read failed: %s", e.what());
        return false;
    } catch (...) {
        appLogf("MMR Read failed: Unknown exception");
        return false;
    }
}

// Write to Memory-Mapped Register - matches the proven TypeScript implementation
bool DE1BLEClient::writeMMR(uint32_t address, const uint8_t* data, uint8_t length) {
    if (!isConnected || !deviceReady) {
        appLogf("❌ MMR Write failed: Device not connected or ready");
        return false;
    }
    
    if (pWriteToMMRChar == nullptr) {
        appLogf("❌ MMR Write failed: WriteToMMR characteristic not available");
        return false;
    }
    
    if (data == nullptr) {
        appLogf("❌ MMR Write failed: Data pointer is null");
        return false;
    }
    
    if (length == 0 || length > 16) {
        appLogf("❌ MMR Write failed: Invalid length %d (must be 1-16)", length);
        return false;
    }
    
    // Create MMR write request buffer - matches TypeScript implementation exactly
    uint8_t buffer[20];
    memset(buffer, 0, 20);
    
    // Write address in big endian (bytes 0-3) - EXACTLY like TypeScript
    buffer[0] = (address >> 24) & 0xFF;
    buffer[1] = (address >> 16) & 0xFF;
    buffer[2] = (address >> 8) & 0xFF;
    buffer[3] = address & 0xFF;
    
    // Overwrite first byte with length (matches TypeScript line 630)
    buffer[0] = length;
    
    // Copy data starting at byte 4 (matches TypeScript)
    for (int i = 0; i < length && i < 16; i++) {
        buffer[4 + i] = data[i];
    }
    
    appLogf("📤 MMR Write Request - Address: 0x%06X, Length: %d", address, length);
    appLogf("📦 Buffer: %02X %02X %02X %02X %02X %02X %02X %02X...", 
           buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
    
    try {
        bool result = pWriteToMMRChar->writeValue(buffer, 20);
        if (result) {
            appLogf("✅ MMR write request sent successfully");
            return true;
        } else {
            appLogf("❌ Failed to send MMR write request");
            return false;
        }
    } catch (const std::exception& e) {
        appLogf("❌ Exception during MMR write: %s", e.what());
        return false;
    }
}

// Format a uint32 value for MMR operations (little endian) - matches TypeScript implementation
void DE1BLEClient::formatUint32ForMMR(uint32_t value, uint8_t* buffer) {
    buffer[0] = value & 0xFF;
    buffer[1] = (value >> 8) & 0xFF;
    buffer[2] = (value >> 16) & 0xFF;
    buffer[3] = (value >> 24) & 0xFF;
}

// Write default shot settings (basic machine settings only) - matches TypeScript implementation
bool DE1BLEClient::writeDefaultShotSettings() {
    if (pShotSettingsChar == nullptr) {
        appLogf("❌ ShotSettings characteristic not available");
        return false;
    }
    
    // Using fixed steam temperature of 160°C as requested
    uint8_t defaultSettings[9] = {
        0,      // SteamSettings (LowPower)
        160,    // TargetSteamTemp (fixed value as requested)
        120,    // TargetSteamLength  
        98,     // TargetHotWaterTemp
        70,     // TargetHotWaterVol
        60,     // TargetHotWaterLength
        200,    // TargetEspressoVol
        0x58,   // TargetGroupTemp (88°C in big endian - high byte)
        0x00    // TargetGroupTemp (88°C in big endian - low byte)
    };
    
    try {
        bool result = pShotSettingsChar->writeValue(defaultSettings, 9);
        if (result) {
            appLogf("✅ Default shot settings written (Steam: 160°C)");
            return true;
        } else {
            appLogf("❌ Failed to write shot settings");
            return false;
        }
    } catch (const std::exception& e) {
        appLogf("❌ Exception writing shot settings: %s", e.what());
        return false;
    }
}

// Configure water levels - matches TypeScript implementation
bool DE1BLEClient::configureWaterLevels() {
    if (pWaterLevelsChar == nullptr) {
        appLogf("❌ WaterLevels characteristic not available");
        return false;
    }
    
    // Water levels configuration: [0, 1] using toU16P8 format
    // toU16P8(0) = 0, toU16P8(1) = 256 (1 * 256) = 0x0100
    uint8_t waterLevelsBuffer[4] = {
        0x00, 0x00, // toU16P8(0) = 0 as uint16BE (absolute minimum)
        0x01, 0x00, // toU16P8(1) = 256 = 0x0100 as uint16BE (low water trigger)
    };
    
    try {
        bool result = pWaterLevelsChar->writeValue(waterLevelsBuffer, 4);
        if (result) {
            appLogf("✅ Water levels configured");
            return true;
        } else {
            appLogf("❌ Failed to write water levels");
            return false;
        }
    } catch (const std::exception& e) {
        appLogf("❌ Exception writing water levels: %s", e.what());
        return false;
    }
}

// Tweak heaters - matches TypeScript implementation
bool DE1BLEClient::tweakHeaters() {
    uint8_t buffer[4];
    bool allSuccess = true;
    
    // HeaterUp1Flow = 20
    formatUint32ForMMR(20, buffer);
    if (!writeMMR(0x803810, buffer, 4)) { // MMRAddr.HeaterUp1Flow
        appLogf("⚠️ Failed to set HeaterUp1Flow");
        allSuccess = false;
    }
    
    // HeaterUp2Flow = 40  
    formatUint32ForMMR(40, buffer);
    if (!writeMMR(0x803814, buffer, 4)) { // MMRAddr.HeaterUp2Flow
        appLogf("⚠️ Failed to set HeaterUp2Flow");
        allSuccess = false;
    }
    
    // WaterHeaterIdleTemp = 990 (99.0°C)
    formatUint32ForMMR(990, buffer);
    if (!writeMMR(0x803818, buffer, 4)) { // MMRAddr.WaterHeaterIdleTemp
        appLogf("⚠️ Failed to set WaterHeaterIdleTemp");
        allSuccess = false;
    }
    
    // HeaterUp2Timeout = 10
    formatUint32ForMMR(10, buffer);
    if (!writeMMR(0x803838, buffer, 4)) { // MMRAddr.HeaterUp2Timeout
        appLogf("⚠️ Failed to set HeaterUp2Timeout");
        allSuccess = false;
    }
    
    // SteamPurgeMode = 0
    formatUint32ForMMR(0, buffer);
    if (!writeMMR(0x803850, buffer, 4)) { // MMRAddr.SteamPurgeMode
        appLogf("⚠️ Failed to set SteamPurgeMode");
        allSuccess = false;
    }
    
    // FlushTimeout = 50 (5.0s * 10)
    formatUint32ForMMR(50, buffer);
    if (!writeMMR(0x803848, buffer, 4)) { // MMRAddr.FlushTimeout
        appLogf("⚠️ Failed to set FlushTimeout");
        allSuccess = false;
    }
    
    // FlushFlowRate = 60 (6.0 * 10)
    formatUint32ForMMR(60, buffer);
    if (!writeMMR(0x803840, buffer, 4)) { // MMRAddr.FlushFlowRate
        appLogf("⚠️ Failed to set FlushFlowRate");
        allSuccess = false;
    }
    
    // HotWaterFlowRate = 100 (10.0 * 10)
    formatUint32ForMMR(100, buffer);
    if (!writeMMR(0x80384C, buffer, 4)) { // MMRAddr.HotWaterFlowRate (corrected address)
        appLogf("⚠️ Failed to set HotWaterFlowRate");
        allSuccess = false;
    }
    
    if (allSuccess) {
        appLogf("✅ All heater settings configured successfully");
    } else {
        appLogf("⚠️ Some heater settings failed to configure");
    }
    
    return allSuccess;
} 
#ifndef DE1_BLE_CLIENT_H
#define DE1_BLE_CLIENT_H

#include "NimBLEDevice.h"
#include "NimBLEScan.h"
#include "NimBLEClient.h"
#include <vector>

// DE1 Service UUID
#define DE1_SERVICE_UUID "0000a000-0000-1000-8000-00805f9b34fb"

// DE1 Characteristic UUIDs - Complete set from protocol documentation
#define DE1_CHAR_VERSIONS "0000a001-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_REQUESTED_STATE "0000a002-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_SET_TIME "0000a003-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_SHOT_DIRECTORY "0000a004-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_READ_FROM_MMR "0000a005-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_WRITE_TO_MMR "0000a006-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_SHOT_MAP_REQUEST "0000a007-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_DELETE_SHOT_RANGE "0000a008-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_FW_MAP_REQUEST "0000a009-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_TEMPERATURES "0000a00a-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_SHOT_SETTINGS "0000a00b-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_DEPRECATED "0000a00c-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_SHOT_SAMPLE "0000a00d-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_STATE_INFO "0000a00e-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_HEADER_WRITE "0000a00f-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_FRAME_WRITE "0000a010-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_WATER_LEVELS "0000a011-0000-1000-8000-00805f9b34fb"
#define DE1_CHAR_CALIBRATION "0000a012-0000-1000-8000-00805f9b34fb"

// DE1 Machine States - Complete set from protocol documentation
enum DE1MajorState {
    DE1_STATE_SLEEP = 0x00,
    DE1_STATE_GOING_TO_SLEEP = 0x01,
    DE1_STATE_IDLE = 0x02,
    DE1_STATE_BUSY = 0x03,
    DE1_STATE_ESPRESSO = 0x04,
    DE1_STATE_STEAM = 0x05,
    DE1_STATE_HOT_WATER = 0x06,
    DE1_STATE_SHORT_CAL = 0x07,
    DE1_STATE_SELF_TEST = 0x08,
    DE1_STATE_LONG_CAL = 0x09,
    DE1_STATE_DESCALE = 0x0A,
    DE1_STATE_FATAL_ERROR = 0x0B,
    DE1_STATE_INIT = 0x0C,
    DE1_STATE_NO_REQUEST = 0x0D,
    DE1_STATE_SKIP_TO_NEXT = 0x0E,
    DE1_STATE_HOT_WATER_RINSE = 0x0F,
    DE1_STATE_STEAM_RINSE = 0x10,
    DE1_STATE_REFILL = 0x11,
    DE1_STATE_CLEAN = 0x12,
    DE1_STATE_IN_BOOT_LOADER = 0x13,
    DE1_STATE_AIR_PURGE = 0x14,
    DE1_STATE_SCHEDULED_WAKE = 0x15
};

enum DE1MinorState {
    DE1_MINOR_NO_STATE = 0x00,
    DE1_MINOR_HEAT_WATER_TANK = 0x01,
    DE1_MINOR_HEAT_WATER_HEATER = 0x02,
    DE1_MINOR_STABILIZE_MIX_TEMP = 0x03,
    DE1_MINOR_PRE_INFUSE = 0x04,
    DE1_MINOR_POUR = 0x05,
    DE1_MINOR_FLUSH = 0x06,
    DE1_MINOR_STEAMING = 0x07,
    DE1_MINOR_DESCALE_INIT = 0x08,
    DE1_MINOR_DESCALE_FILL_GROUP = 0x09,
    DE1_MINOR_DESCALE_RETURN = 0x0A,
    DE1_MINOR_DESCALE_GROUP = 0x0B,
    DE1_MINOR_DESCALE_STEAM = 0x0C,
    DE1_MINOR_CLEAN_INIT = 0x0D,
    DE1_MINOR_CLEAN_FILL_GROUP = 0x0E,
    DE1_MINOR_CLEAN_SOAK = 0x0F,
    DE1_MINOR_CLEAN_GROUP = 0x10,
    DE1_MINOR_PAUSED_REFILL = 0x11,
    DE1_MINOR_PAUSED_STEAM = 0x12,
    DE1_MINOR_USER_NOT_PRESENT = 0x13,
    DE1_MINOR_STEAM_PUFF = 0x14,
    // Error minor states
    DE1_MINOR_ERROR_NAN = 0xC8,
    DE1_MINOR_ERROR_INF = 0xC9,
    DE1_MINOR_ERROR_GENERIC = 0xCA,
    DE1_MINOR_ERROR_ACC = 0xCB,
    DE1_MINOR_ERROR_TSENSOR = 0xCC,
    DE1_MINOR_ERROR_PSENSOR = 0xCD,
    DE1_MINOR_ERROR_WLEVEL = 0xCE,
    DE1_MINOR_ERROR_DIP = 0xCF,
    DE1_MINOR_ERROR_ASSERTION = 0xD0,
    DE1_MINOR_ERROR_UNSAFE = 0xD1,
    DE1_MINOR_ERROR_INVALID_PARM = 0xD2,
    DE1_MINOR_ERROR_FLASH = 0xD3,
    DE1_MINOR_ERROR_OOM = 0xD4,
    DE1_MINOR_ERROR_DEADLINE = 0xD5,
    DE1_MINOR_ERROR_HI_CURRENT = 0xD6,
    DE1_MINOR_ERROR_LO_CURRENT = 0xD7,
    DE1_MINOR_ERROR_BOOT_FILL = 0xD8
};

// DE1 Machine State Structure
struct DE1MachineState {
    uint8_t majorState;
    uint8_t minorState;
};

// DE1 Temperature Structure
struct DE1Temperatures {
    float waterHeaterTemp;
    float steamHeaterTemp;
    float groupHeaterTemp;
    float coldWaterTemp;
    float targetWaterHeaterTemp;
    float targetSteamHeaterTemp;
    float targetGroupHeaterTemp;
    float targetColdWaterTemp;
};

// DE1 Shot Sample Structure (real-time shot data)
struct DE1ShotSample {
    uint16_t sampleTime;
    float groupPressure;      // divide by 4096
    float groupFlow;          // divide by 4096
    float mixTemperature;     // divide by 256
    float headTemperature;    // divide by 65536
    float setMixTemperature;  // divide by 256
    float setHeadTemperature; // divide by 256
    float setGroupPressure;   // divide by 16
    float setGroupFlow;       // divide by 16
    uint8_t frameNumber;
    uint8_t steamTemperature;
};

// DE1 Water Levels Structure
struct DE1WaterLevels {
    float minimumLevel;       // toU16P8 format
    float maximumLevel;       // toU16P8 format
};

// DE1 Shot Settings Structure
struct DE1ShotSettings {
    uint8_t steamSettings;
    uint8_t targetSteamTemp;      // 140-160 range
    uint8_t targetSteamLength;    // seconds
    uint8_t targetHotWaterTemp;
    uint8_t targetHotWaterVolume;
    uint8_t targetHotWaterLength; // seconds
    uint8_t targetEspressoVolume;
    float targetGroupTemp;        // U16P8 format
};

// MMR (Memory-Mapped Register) operation structure
struct MMROperation {
    uint32_t address;
    uint8_t length;
    uint8_t data[16];
    bool isWrite;
    unsigned long timestamp;
};

// DE1 Device Information
struct DE1Device {
    NimBLEAddress address;
    std::string name;
    int rssi;
    bool isConnected;
};

class DE1BLEClient {
private:
    NimBLEScan* pBLEScan;
    NimBLEClient* pClient;
    NimBLERemoteService* pRemoteService;
    
    // Critical characteristics for real-time monitoring
    NimBLERemoteCharacteristic* pStateInfoChar;
    NimBLERemoteCharacteristic* pTemperaturesChar;
    NimBLERemoteCharacteristic* pShotSampleChar;
    NimBLERemoteCharacteristic* pShotSettingsChar;
    NimBLERemoteCharacteristic* pWaterLevelsChar;
    
    // MMR characteristics for device configuration
    NimBLERemoteCharacteristic* pReadFromMMRChar;
    NimBLERemoteCharacteristic* pWriteToMMRChar;
    
    // Additional characteristics
    NimBLERemoteCharacteristic* pVersionsChar;
    NimBLERemoteCharacteristic* pRequestedStateChar;
    
    // Current device state and data
    DE1Device connectedDevice;
    DE1MachineState currentState;
    DE1Temperatures currentTemperatures;
    DE1ShotSample currentShotSample;
    DE1ShotSettings currentShotSettings;
    DE1WaterLevels currentWaterLevels;
    
    // Discovery and connection state
    std::vector<DE1Device> discoveredDevices;
    bool deviceFound;
    bool isConnected;
    bool deviceReady;
    
    // MMR operation tracking
    std::vector<MMROperation> pendingMMROperations;
    unsigned long lastMMROperationTime;
    
    // UI callback for connection state changes
    void (*uiConnectionCallback)(bool connected) = nullptr;
    
public:
    // Callback handlers (made public for friend classes)
    void onScanResult(const NimBLEAdvertisedDevice* advertisedDevice);
    void onConnect(NimBLEClient* pClient);
    void onDisconnect(NimBLEClient* pClient, int reason);
    void onStateInfoNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void onTemperaturesNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void onShotSampleNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void onShotSettingsNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void onWaterLevelsNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void onMMRReadNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

private:

public:
    DE1BLEClient();
    ~DE1BLEClient();
    
    // Basic operations
    bool scanForDE1Devices(int scanTime = 10);
    bool connectToDE1(const NimBLEAddress& address);
    void disconnect();
    
    // State and data access
    DE1MachineState getCurrentState() const { return currentState; }
    DE1Temperatures getCurrentTemperatures() const { return currentTemperatures; }
    DE1ShotSample getCurrentShotSample() const { return currentShotSample; }
    DE1ShotSettings getCurrentShotSettings() const { return currentShotSettings; }
    DE1WaterLevels getCurrentWaterLevels() const { return currentWaterLevels; }
    DE1Device getConnectedDevice() const { return connectedDevice; }
    bool isDeviceConnected() const { return isConnected; }
    bool isDeviceReady() const { return deviceReady; }
    
    // Device discovery
    const std::vector<DE1Device>& getDiscoveredDevices() const { return discoveredDevices; }
    void clearDiscoveredDevices() { discoveredDevices.clear(); }
    std::string getDeviceListAsString() const;
    bool hasFoundDevice() const { return deviceFound; }
    NimBLEAddress getFoundDeviceAddress() const { return connectedDevice.address; }
    
    // MMR (Memory-Mapped Register) operations
    bool readMMR(uint32_t address, uint8_t length);
    bool writeMMR(uint32_t address, const uint8_t* data, uint8_t length);
    
    // Device setup and initialization
    bool performDeviceSetup();
    bool requestState(DE1MajorState state);
    
    // MMR utility functions
    void formatUint32ForMMR(uint32_t value, uint8_t* buffer);
    bool writeDefaultShotSettings();
    bool configureWaterLevels();
    bool tweakHeaters();
    
    // Utility functions
    static std::string stateToString(const DE1MachineState& state);
    static std::string majorStateToString(uint8_t majorState);
    static std::string minorStateToString(uint8_t minorState);
    
    // UI callback management
    void setUIConnectionCallback(void (*callback)(bool connected)) { uiConnectionCallback = callback; }
    
private:
    // Internal helper methods
    bool discoverAndSubscribeToCharacteristics();
    void initializeDataStructures();
    void resetConnectionState();
};

// Global instance
extern DE1BLEClient de1Client;

#endif // DE1_BLE_CLIENT_H 
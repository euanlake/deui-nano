# DE1 Bluetooth Protocol Documentation

This document provides a comprehensive overview of the DE1 espresso machine's Bluetooth Low Energy (BLE) communication protocol, including characteristic definitions, state management, and memory-mapped register (MMR) operations.

## Table of Contents

1. [Overview](#overview)
2. [BLE Service and Characteristics](#ble-service-and-characteristics)
3. [Machine States](#machine-states)
4. [Memory-Mapped Registers (MMR)](#memory-mapped-registers-mmr)
5. [Communication Protocol](#communication-protocol)
6. [Data Formats](#data-formats)
7. [Error Handling](#error-handling)
8. [Connection Lifecycle](#connection-lifecycle)

## Overview

The DE1 espresso machine implements a custom Bluetooth Low Energy protocol that provides real-time monitoring and control capabilities. The protocol is built around a set of BLE characteristics that handle different aspects of machine operation, from basic state information to complex shot profiling.

### Key Features

- **Real-time State Monitoring**: Continuous updates of machine state, temperatures, and shot data
- **Memory-Mapped Register Access**: Direct access to internal machine parameters
- **Shot Profiling**: Support for complex espresso shot profiles with pressure/flow control
- **Error Reporting**: Comprehensive error state management
- **Configuration Management**: Device settings and calibration data access

## BLE Service and Characteristics

The DE1 exposes a single BLE service with multiple characteristics for different functionality:

### Service UUID
```
0000a000-0000-1000-8000-00805f9b34fb
```

### Characteristic Definitions

| Characteristic | UUID | Type | Description |
|----------------|------|------|-------------|
| **Versions** | `0000a001-0000-1000-8000-00805f9b34fb` | R | Firmware and hardware version information |
| **RequestedState** | `0000a002-0000-1000-8000-00805f9b34fb` | RW | Request machine state changes |
| **SetTime** | `0000a003-0000-1000-8000-00805f9b34fb` | RW | Set current machine time |
| **ShotDirectory** | `0000a004-0000-1000-8000-00805f9b34fb` | R | Access to shot history and metadata |
| **ReadFromMMR** | `0000a005-0000-1000-8000-00805f9b34fb` | RW | Read from memory-mapped registers |
| **WriteToMMR** | `0000a006-0000-1000-8000-00805f9b34fb` | W | Write to memory-mapped registers |
| **ShotMapRequest** | `0000a007-0000-1000-8000-00805f9b34fb` | W | Map shot data for read/write access |
| **DeleteShotRange** | `0000a008-0000-1000-8000-00805f9b34fb` | W | Delete shot data from storage |
| **FWMapRequest** | `0000a009-0000-1000-8000-00805f9b34fb` | W | Map firmware image to MMR |
| **Temperatures** | `0000a00a-0000-1000-8000-00805f9b34fb` | R | Real-time temperature readings |
| **ShotSettings** | `0000a00b-0000-1000-8000-00805f9b34fb` | RW | Shot configuration parameters |
| **Deprecated** | `0000a00c-0000-1000-8000-00805f9b34fb` | RW | Legacy shot description (deprecated) |
| **ShotSample** | `0000a00d-0000-1000-8000-00805f9b34fb` | R | Real-time shot monitoring data |
| **StateInfo** | `0000a00e-0000-1000-8000-00805f9b34fb` | R | Current machine state information |
| **HeaderWrite** | `0000a00f-0000-1000-8000-00805f9b34fb` | RW | Shot header modification |
| **FrameWrite** | `0000a010-0000-1000-8000-00805f9b34fb` | RW | Shot frame modification |
| **WaterLevels** | `0000a011-0000-1000-8000-00805f9b34fb` | RW | Water level configuration |
| **Calibration** | `0000a012-0000-1000-8000-00805f9b34fb` | RW | Calibration data access |

## Machine States

The DE1 uses a hierarchical state system with major and minor states to represent the current machine operation.

### Major States

| State ID | Name | Description |
|----------|------|-------------|
| `0x00` | **Sleep** | Everything is off |
| `0x01` | **GoingToSleep** | Transitioning to sleep mode |
| `0x02` | **Idle** | Heaters are controlled, tank water will be heated if required |
| `0x03` | **Busy** | Firmware is doing something you can't interrupt (e.g., cooling down water heater after a shot, calibrating sensors on startup) |
| `0x04` | **Espresso** | Making espresso |
| `0x05` | **Steam** | Making steam |
| `0x06` | **HotWater** | Making hot water |
| `0x07` | **ShortCal** | Running a short calibration |
| `0x08` | **SelfTest** | Checking as much as possible within the firmware (manufacture/repair only) |
| `0x09` | **LongCal** | Long and involved calibration, possibly involving user interaction |
| `0x0A` | **Descale** | Descale the whole machine |
| `0x0B` | **FatalError** | Something has gone horribly wrong |
| `0x0C` | **Init** | Machine has not been run yet |
| `0x0D` | **NoRequest** | State for T_RequestedState - means nothing is specifically requested |
| `0x0E` | **SkipToNext** | In Espresso, skip to next frame. Others, go to Idle if possible |
| `0x0F` | **HotWaterRinse** | Produce hot water at whatever temperature is available |
| `0x10` | **SteamRinse** | Produce a blast of steam |
| `0x11` | **Refill** | Attempting, or needs, a refill |
| `0x12` | **Clean** | Clean group head |
| `0x13` | **InBootLoader** | The main firmware has not run for some reason. Bootloader is active |
| `0x14` | **AirPurge** | Air purge operation |
| `0x15` | **ScheduledWake** | Scheduled wake up idle state |

### Minor States

| State ID | Name | Description |
|----------|------|-------------|
| `0x00` | **NoState** | State is not relevant |
| `0x01` | **HeatWaterTank** | Cold water is not hot enough. Heating hot water tank |
| `0x02` | **HeatWaterHeater** | Warm up hot water heater for shot |
| `0x03` | **StabilizeMixTemp** | Stabilize mix temp and get entire water path up to temperature |
| `0x04` | **PreInfuse** | Espresso pre-infusion phase (Hot Water and Steam skip this state) |
| `0x05` | **Pour** | Main extraction phase (Not used in Steam) |
| `0x06` | **Flush** | Espresso flush phase (Espresso only) |
| `0x07` | **Steaming** | Steam only - active steam production |
| `0x08` | **DescaleInit** | Starting descale |
| `0x09` | **DescaleFillGroup** | Get some descaling solution into the group and let it sit |
| `0x0A` | **DescaleReturn** | Descaling internals |
| `0x0B` | **DescaleGroup** | Descaling group |
| `0x0C` | **DescaleSteam** | Descaling steam |
| `0x0D` | **CleanInit** | Starting clean |
| `0x0E` | **CleanFillGroup** | Fill the group |
| `0x0F` | **CleanSoak** | Wait for 60 seconds so we soak the group head |
| `0x10` | **CleanGroup** | Flush through group |
| `0x11` | **PausedRefill** | Have we given up on a refill |
| `0x12` | **PausedSteam** | Are we paused in steam? |
| `0x13` | **UserNotPresent** | Tell the tablet we think the user is not present |
| `0x14` | **SteamPuff** | Steaming in puff mode |

### Error Minor States

| State ID | Name | Description |
|----------|------|-------------|
| `0xC8` | **Error_NaN** | Something died with a NaN |
| `0xC9` | **Error_Inf** | Something died with an Inf |
| `0xCA` | **Error_Generic** | An error for which we have no more specific description |
| `0xCB` | **Error_ACC** | ACC not responding, unlocked, or incorrectly programmed |
| `0xCC` | **Error_TSensor** | We are getting an error that is probably a broken temperature sensor |
| `0xCD` | **Error_PSensor** | Pressure sensor error |
| `0xCE` | **Error_WLevel** | Water level sensor error |
| `0xCF` | **Error_DIP** | DIP switches told us to wait in the error state |
| `0xD0` | **Error_Assertion** | Assertion failed |
| `0xD1` | **Error_Unsafe** | Unsafe value assigned to variable |
| `0xD2` | **Error_InvalidParm** | Invalid parameter passed to function |
| `0xD3` | **Error_Flash** | Error accessing external flash |
| `0xD4` | **Error_OOM** | Could not allocate memory |
| `0xD5` | **Error_Deadline** | Realtime deadline missed |
| `0xD6` | **Error_HiCurrent** | Measured a current that is out of bounds |
| `0xD7` | **Error_LoCurrent** | Not enough current flowing, despite something being turned on |
| `0xD8` | **Error_BootFill** | Could not get up to pressure during boot pressure test, possibly because no water |

## Memory-Mapped Registers (MMR)

The DE1 uses a memory-mapped register system for accessing internal device parameters and configuration data.

### MMR Address Space

| Address Range | Description |
|---------------|-------------|
| `0x000000` - `0x7FFFFF` | Flash memory (read/write) |
| `0x800000` - `0x803FFF` | Hardware configuration and device info |

### Key MMR Addresses

| Address | Name | Description | Format |
|---------|------|-------------|--------|
| `0x000000` | **ExternalFlash** | Flash memory access | Raw data |
| `0x800000` | **HWConfig** | Hardware configuration | Raw data |
| `0x800004` | **Model** | Device model identifier | uint32 |
| `0x800008` | **CPUBoardModel** | CPU board model * 1000 | uint32 |
| `0x80000C` | **v13Model** | Firmware model (0=Unset, 1=DE1, 2=DE1Plus, 3=DE1Pro, 4=DE1XL, 5=DE1Cafe) | uint32 |
| `0x800010` | **CPUFirmwareBuild** | CPU firmware build number | uint32 |
| `0x802800` | **DebugLen** | Debug buffer valid character count | uint32 |
| `0x802804` | **DebugBuffer** | Last 4K of debug output | String |
| `0x803804` | **DebugConfig** | BLE debug configuration | uint32 |
| `0x803808` | **FanThreshold** | Fan threshold temperature | uint32 |
| `0x80380C` | **TankTemp** | Tank water temperature threshold | uint32 |
| `0x803810` | **HeaterUp1Flow** | HeaterUp Phase 1 flow rate | uint32 |
| `0x803814` | **HeaterUp2Flow** | HeaterUp Phase 2 flow rate | uint32 |
| `0x803818` | **WaterHeaterIdleTemp** | Water heater idle temperature | uint32 |
| `0x80381C` | **GHCInfo** | Group Head Controller info bitmask | uint32 |
| `0x803820` | **PrefGHCMCI** | GHC MCI preference | uint32 |
| `0x803824` | **MaxShotPres** | Maximum shot pressure | uint32 |
| `0x803828` | **TargetSteamFlow** | Target steam flow rate | uint32 |
| `0x80382C` | **SteamStartSecs** | Steam start seconds * 100 | uint32 |
| `0x803830` | **SerialN** | Current serial number | String |
| `0x803834` | **HeaterV** | Nominal heater voltage (+1000 if set value) | uint32 |
| `0x803838` | **HeaterUp2Timeout** | HeaterUp Phase 2 timeout | uint32 |
| `0x80383C` | **CalFlowEst** | Flow estimation calibration | uint32 |
| `0x803840` | **FlushFlowRate** | Flush flow rate * 10 | uint32 |
| `0x803844` | **FlushTemp** | Flush temperature | uint32 |
| `0x803848` | **FlushTimeout** | Flush timeout * 10 | uint32 |
| `0x80384C` | **HotWaterFlowRate** | Hot water flow rate * 10 | uint32 |
| `0x803850` | **SteamPurgeMode** | Steam purge mode | uint32 |
| `0x803854` | **AllowUSBCharging** | Allow USB charging | uint32 |
| `0x803858` | **AppFeatureFlags** | App feature flags | uint32 |
| `0x80385C` | **RefillKitPresent** | Refill kit present (0=None, 1=Manual, 2=AutoDetect) | uint32 |

## Communication Protocol

### MMR Read/Write Protocol

The DE1 uses a custom protocol for reading and writing memory-mapped registers through the `ReadFromMMR` and `WriteToMMR` characteristics.

#### MMR Read Request Format

```
Byte 0-3:   Address (big-endian uint32)
Byte 0:     Length (overwrites first byte of address)
Byte 4-19:  Padding (zeros)
```

#### MMR Read Response Format

```
Byte 0:     Length indicator ((length + 1) * 4 bytes follow)
Byte 1-3:   Address (big-endian uint32)
Byte 4+:    Data (length * 4 bytes)
```

#### MMR Write Request Format

```
Byte 0-3:   Address (big-endian uint32)
Byte 0:     Data length (overwrites first byte of address)
Byte 4-19:  Data (up to 16 bytes)
```

### Characteristic Data Formats

#### StateInfo Characteristic

```
Byte 0: Major state (uint8)
Byte 1: Minor state (uint8)
```

#### Temperatures Characteristic

```
Byte 0-1:   Water heater temperature (uint16, big-endian, divide by 256)
Byte 2-3:   Steam heater temperature (uint16, big-endian, divide by 256)
Byte 4-5:   Group heater temperature (uint16, big-endian, divide by 256)
Byte 6-7:   Cold water temperature (uint16, big-endian, divide by 256)
Byte 8-9:   Target water heater temperature (uint16, big-endian, divide by 256)
Byte 10-11: Target steam heater temperature (uint16, big-endian, divide by 256)
Byte 12-13: Target group heater temperature (uint16, big-endian, divide by 256)
Byte 14-15: Target cold water temperature (uint16, big-endian, divide by 256)
```

#### ShotSample Characteristic

Wire format uses **big-endian** multi-byte integers. Values below match `main/de1_ble_client.cpp` and the ESP32 `deui_ble_parse_shot_sample` decoder (first 12 bytes).

```
Byte 0-1:   Sample time (uint16, big-endian), milliseconds
Byte 2-3:   Group pressure (uint16, big-endian, divide by 4096 → bar)
Byte 4-5:   Group flow (uint16, big-endian, divide by 4096 → ml/s)
Byte 6-7:   Mix temperature (uint16, big-endian, divide by 256 → °C)
Byte 8-11:  Head temperature (uint32, big-endian, divide by 65536)
Byte 12-13: Set mix temperature (uint16, big-endian, divide by 256)
Byte 14-15: Set head temperature (uint16, big-endian, divide by 256)
Byte 16:    Set group pressure (uint8, divide by 16)
Byte 17:    Set group flow (uint8, divide by 16)
Byte 18:    Frame number (uint8)
Byte 19:    Steam temperature (uint8)
```

Full notifications are typically **≥ 19 bytes**; decoding pressure/flow/mix/head only requires **≥ 12 bytes**.

Some older reference parsers treat head temperature as four bytes ending at index 11 but then read set mix from indices 11–12, which double-uses byte 11; treat extended fields (setpoints onward) as best-effort unless verified against your firmware stream.

#### WaterLevels Characteristic

```
Byte 0-1: Minimum water level (uint16, big-endian, toU16P8 format)
Byte 2-3: Maximum water level (uint16, big-endian, toU16P8 format)
```

#### ShotSettings Characteristic

```
Byte 0: Steam settings (bitmap)
Byte 1: Target steam temperature (uint8, 140-160 range)
Byte 2: Target steam length (uint8, seconds)
Byte 3: Target hot water temperature (uint8)
Byte 4: Target hot water volume (uint8)
Byte 5: Target hot water length (uint8, seconds)
Byte 6: Target espresso volume (uint8)
Byte 7-8: Target group temperature (uint16, big-endian, U16P8 format)
```

## Error Handling

### Connection Errors

| Error Code | Description |
|------------|-------------|
| `NotPoweredOn` | Bluetooth adapter not powered on |
| `AlreadyScanning` | Already scanning for devices |
| `AlreadyConnecting` | Already connecting to a device |
| `AlreadyConnected` | Already connected to a device |
| `NotConnected` | Not connected to any device |
| `UnknownCharacteristic` | Characteristic not found |
| `AlreadyWritingShot` | Shot write operation in progress |
| `Locked` | Operation locked by another process |

### Communication Timeouts

- **MMR Operations**: 10-second timeout
- **Characteristic Operations**: 5-second timeout
- **Connection Setup**: 30-second timeout

### Error Recovery

1. **Automatic Reconnection**: Attempts to reconnect on connection loss
2. **State Recovery**: Reads current state after reconnection
3. **Error Logging**: Comprehensive error logging for debugging
4. **Graceful Degradation**: Continues operation with reduced functionality on non-critical errors

## Connection Lifecycle

### 1. Device Discovery

```typescript
// Scan for DE1 devices
const devices = await bluetoothService.scanForDevices({
  serviceUUIDs: ['0000a000-0000-1000-8000-00805f9b34fb']
});
```

### 2. Connection Establishment

```typescript
// Connect to DE1 device
await bluetoothService.connect(deviceId);

// Discover services and characteristics
await bluetoothService.discoverAllServicesAndCharacteristics();
```

### 3. Characteristic Subscription

```typescript
// Subscribe to critical characteristics
const criticalCharacteristics = [
  CharAddr.ReadFromMMR,    // MMR read responses
  CharAddr.StateInfo,      // Machine state updates
  CharAddr.WaterLevels,    // Water level notifications
  CharAddr.Temperatures,   // Temperature readings
  CharAddr.ShotSample,     // Real-time shot data
  CharAddr.ShotSettings,   // Shot configuration updates
  CharAddr.HeaderWrite,    // Shot header data
  CharAddr.FrameWrite,     // Shot frame data
];

for (const charAddr of criticalCharacteristics) {
  await bluetoothService.subscribeToCharacteristic(charAddr);
}
```

### 4. Device Setup Sequence

```typescript
// Perform complete device setup
await de1Service.performDeviceSetup();

// Setup sequence includes:
// 1. Write default profile
// 2. Configure fan threshold
// 3. Write shot settings
// 4. Configure steam settings
// 5. Set water levels
// 6. Tweak heaters
// 7. Configure refill kit
// 8. Wait for stabilization
```

### 5. Normal Operation

```typescript
// Monitor state changes
de1Service.onStateChange((state) => {
  console.log(`Machine state: ${state.majorState} - ${state.minorState}`);
});

// Read MMR data
const heaterVoltage = await de1Service.Mmr.read(MMRAddr.HeaterV, 1);

// Write MMR data
await de1Service.Mmr.write(MMRAddr.FanThreshold, formatUint32(80));
```

### 6. Disconnection

```typescript
// Clean disconnect
await bluetoothService.disconnect();
await de1Service.cleanup();
```

## Best Practices

### Connection Management

1. **Always check connection state** before operations
2. **Use connection pooling** for multiple operations
3. **Implement automatic reconnection** with exponential backoff
4. **Monitor connection quality** and log issues

### Data Handling

1. **Validate all incoming data** before processing
2. **Use proper data conversion** (endianness, scaling)
3. **Handle missing or corrupted data** gracefully
4. **Log data anomalies** for debugging

### Error Recovery

1. **Implement retry logic** for transient errors
2. **Provide user feedback** for connection issues
3. **Maintain operation state** during reconnections
4. **Log detailed error information** for troubleshooting

### Performance Optimization

1. **Batch MMR operations** when possible
2. **Use appropriate timeouts** for different operations
3. **Implement connection pooling** for high-frequency operations
4. **Monitor memory usage** for long-running connections

## Troubleshooting

### Common Issues

1. **Connection Drops**: Check Bluetooth adapter power and interference
2. **MMR Read Failures**: Verify address validity and device state
3. **State Sync Issues**: Re-read state after reconnection
4. **Performance Problems**: Check for excessive characteristic subscriptions

### Debug Information

Enable debug logging to capture:
- Raw characteristic data
- MMR operation details
- State transition logs
- Error conditions and recovery attempts

### Support Resources

- DE1 firmware documentation
- Bluetooth Low Energy specification
- Machine state diagrams
- Error code reference 
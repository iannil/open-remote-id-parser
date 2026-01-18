# ORIP - Open Remote ID Parser

Python bindings for the ORIP library - a cross-platform Remote ID protocol parser for drone detection.

## Installation

```bash
# From source (requires building the C++ library first)
cd python
pip install -e .

# Or install with dev dependencies
pip install -e ".[dev]"
```

## Prerequisites

The Python bindings require the compiled ORIP shared library (`liborip.so` on Linux, `liborip.dylib` on macOS, `orip.dll` on Windows).

Build the C++ library first:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DORIP_BUILD_SHARED=ON
make
```

Then either:
1. Install the library to system paths, or
2. Set `LD_LIBRARY_PATH` (Linux) / `DYLD_LIBRARY_PATH` (macOS) to point to the build directory

## Quick Start

```python
from orip import RemoteIDParser, TransportType

# Create parser
parser = RemoteIDParser()

# Parse BLE advertisement data
result = parser.parse(ble_scan_data, rssi=-70, transport=TransportType.BT_LEGACY)

if result.success:
    uav = result.uav
    print(f"Detected drone: {uav.id}")
    print(f"Position: {uav.location.latitude}, {uav.location.longitude}")
    print(f"Altitude: {uav.location.altitude_baro}m")

# Get all active drones
for uav in parser.get_active_uavs():
    print(f"{uav.id}: RSSI {uav.rssi} dBm")

# Clean up
parser.close()
```

## Using Callbacks

```python
from orip import RemoteIDParser

def on_new_drone(uav):
    print(f"New drone detected: {uav.id}")

def on_drone_update(uav):
    print(f"Drone {uav.id} at ({uav.location.latitude}, {uav.location.longitude})")

with RemoteIDParser() as parser:
    parser.set_on_new_uav(on_new_drone)
    parser.set_on_uav_update(on_drone_update)

    # Parse incoming data...
    while running:
        data = receive_ble_data()
        parser.parse(data, rssi)
```

## API Reference

### RemoteIDParser

- `parse(payload, rssi, transport)` - Parse raw BLE/WiFi data
- `get_active_count()` - Get number of active drones
- `get_active_uavs()` - Get list of all active drones
- `get_uav(id)` - Get specific drone by ID
- `clear()` - Clear all tracked drones
- `cleanup()` - Remove timed-out drones
- `set_on_new_uav(callback)` - Set callback for new drone detection
- `set_on_uav_update(callback)` - Set callback for drone updates
- `set_on_uav_timeout(callback)` - Set callback for drone timeout
- `close()` - Release resources

### Data Types

- `UAVObject` - Complete drone information
- `LocationData` - Position and velocity
- `SystemInfo` - Operator/pilot location
- `ParseResult` - Parsing result with status

### Enums

- `ProtocolType` - ASTM_F3411, ASD_STAN, CN_RID
- `TransportType` - BT_LEGACY, BT_EXTENDED, WIFI_BEACON, WIFI_NAN
- `UAVIdType` - SERIAL_NUMBER, CAA_REGISTRATION, etc.
- `UAVType` - HELICOPTER_OR_MULTIROTOR, AEROPLANE, etc.
- `UAVStatus` - UNDECLARED, GROUND, AIRBORNE, EMERGENCY

## Running Tests

```bash
cd python
pytest tests/
```

## License

MIT License

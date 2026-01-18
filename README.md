# Open Remote ID Parser (ORIP)

<p align="center">
  <strong>A high-performance, cross-platform library for parsing drone Remote ID signals</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#quick-start">Quick Start</a> •
  <a href="#api-reference">API</a> •
  <a href="#building">Building</a> •
  <a href="#contributing">Contributing</a>
</p>

---

**Open Remote ID Parser** is a lightweight C++ library for decoding drone Remote ID broadcasts. It supports multiple protocols (ASTM F3411, ASD-STAN) and transport layers (Bluetooth Legacy/Extended, WiFi Beacon/NAN), making it ideal for building drone detection applications on mobile devices, embedded systems, or desktop platforms.

> Remote ID is the "digital license plate" for drones, mandated by regulations worldwide (FAA in the US, EASA in Europe). This library enables anyone to build drone detection solutions using commodity hardware like smartphones or Raspberry Pi.

## Features

- **Multi-Protocol Support**
  - ASTM F3411-22a (USA/International)
  - ASD-STAN EN 4709-002 (European Union)
  - GB/T (China) - Interface reserved

- **Multi-Transport Support**
  - Bluetooth 4.x Legacy Advertising
  - Bluetooth 5.x Extended Advertising / Long Range (Coded PHY)
  - WiFi Beacon (802.11 Vendor Specific IE)
  - WiFi NAN (Neighbor Awareness Networking)

- **Advanced Analysis**
  - Anomaly Detection (spoofing, replay attacks, impossible speeds)
  - Trajectory Analysis (smoothing, prediction, pattern classification)
  - Session Management (deduplication, timeout handling)

- **Cross-Platform Bindings**
  - C++ (core library)
  - C API (for FFI integration)
  - Android/Kotlin (via JNI)
  - Python (via ctypes)

- **Performance**
  - Zero-copy parsing with bit fields
  - Minimal memory allocations
  - Suitable for real-time processing on mobile devices

## Supported Message Types

| Message Type | Description |
|--------------|-------------|
| Basic ID (0x0) | Drone serial number, registration ID |
| Location (0x1) | Latitude, longitude, altitude, speed, heading |
| Authentication (0x2) | Cryptographic authentication data |
| Self-ID (0x3) | Operator-defined description text |
| System (0x4) | Operator location, area of operation |
| Operator ID (0x5) | Operator registration number |
| Message Pack (0xF) | Multiple messages in one broadcast |

## Installation

### C++ (CMake)

```cmake
include(FetchContent)
FetchContent_Declare(
    orip
    GIT_REPOSITORY https://github.com/user/open-remote-id-parser.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(orip)

target_link_libraries(your_target PRIVATE orip)
```

### Python

```bash
cd python
pip install .

# Or for development
pip install -e ".[dev]"
```

### Android (Gradle)

```kotlin
// settings.gradle.kts
include(":orip")
project(":orip").projectDir = file("path/to/open-remote-id-parser/android/orip")

// app/build.gradle.kts
dependencies {
    implementation(project(":orip"))
}
```

## Quick Start

### C++

```cpp
#include <orip/orip.h>

int main() {
    orip::RemoteIDParser parser;
    parser.init();

    // Set up callbacks
    parser.setOnNewUAV([](const orip::UAVObject& uav) {
        std::cout << "New drone: " << uav.id << std::endl;
    });

    // Parse incoming BLE advertisement
    std::vector<uint8_t> ble_data = /* from scanner */;
    auto result = parser.parse(ble_data, rssi, orip::TransportType::BT_LEGACY);

    if (result.success) {
        std::cout << "Drone ID: " << result.uav.id << std::endl;
        std::cout << "Location: " << result.uav.location.latitude
                  << ", " << result.uav.location.longitude << std::endl;
    }

    return 0;
}
```

### Python

```python
from orip import RemoteIDParser, TransportType

with RemoteIDParser() as parser:
    parser.set_on_new_uav(lambda uav: print(f"New drone: {uav.id}"))

    # Parse BLE advertisement data
    result = parser.parse(ble_data, rssi=-70, transport=TransportType.BT_LEGACY)

    if result.success:
        print(f"Drone: {result.uav.id}")
        print(f"Location: {result.uav.location.latitude}, {result.uav.location.longitude}")
```

### Kotlin (Android)

```kotlin
import com.orip.RemoteIDParser
import com.orip.TransportType

class DroneScanner {
    private val parser = RemoteIDParser()

    init {
        parser.setOnNewUAV { uav ->
            Log.d("DroneScanner", "New drone: ${uav.id}")
        }
    }

    // In BLE scan callback
    fun onScanResult(result: ScanResult) {
        val scanRecord = result.scanRecord ?: return

        val parseResult = parser.parse(
            scanRecord.bytes,
            result.rssi,
            TransportType.BT_LEGACY
        )

        if (parseResult.success) {
            updateMap(parseResult.uav)
        }
    }

    fun cleanup() {
        parser.close()
    }
}
```

### C API

```c
#include <orip/orip_c.h>

int main() {
    orip_parser_t* parser = orip_create();
    orip_result_t result;

    uint8_t payload[] = { /* BLE data */ };

    orip_parse(parser, payload, sizeof(payload), -70,
               ORIP_TRANSPORT_BT_LEGACY, &result);

    if (result.success) {
        printf("Drone: %s\n", result.uav.id);
        printf("Lat: %f, Lon: %f\n",
               result.uav.location.latitude,
               result.uav.location.longitude);
    }

    orip_destroy(parser);
    return 0;
}
```

## Advanced Features

### Anomaly Detection

Detect spoofing attempts and impossible flight patterns:

```cpp
#include <orip/anomaly_detector.h>

orip::analysis::AnomalyDetector detector;

// Analyze each UAV update
auto anomalies = detector.analyze(uav, rssi);

for (const auto& anomaly : anomalies) {
    switch (anomaly.type) {
        case AnomalyType::REPLAY_ATTACK:
            std::cerr << "Warning: Possible replay attack detected!" << std::endl;
            break;
        case AnomalyType::SPEED_IMPOSSIBLE:
            std::cerr << "Warning: Impossible speed detected!" << std::endl;
            break;
        // ...
    }
}
```

### Trajectory Analysis

Track flight paths and predict future positions:

```cpp
#include <orip/trajectory_analyzer.h>

orip::analysis::TrajectoryAnalyzer analyzer;

// Add position updates
analyzer.addPosition(uav.id, uav.location);

// Get flight pattern
auto pattern = analyzer.classifyPattern(uav.id);
// Returns: LINEAR, CIRCULAR, PATROL, STATIONARY, etc.

// Predict position 5 seconds ahead
auto prediction = analyzer.predictPosition(uav.id, 5000);
std::cout << "Predicted location: " << prediction.latitude
          << ", " << prediction.longitude << std::endl;

// Get trajectory statistics
auto stats = analyzer.getStats(uav.id);
std::cout << "Total distance: " << stats.total_distance_m << " m" << std::endl;
std::cout << "Max speed: " << stats.max_speed_mps << " m/s" << std::endl;
```

## API Reference

### Core Classes

| Class | Description |
|-------|-------------|
| `RemoteIDParser` | Main parser class, handles all protocols |
| `UAVObject` | Complete drone data (ID, location, operator info) |
| `ParseResult` | Result of parsing operation |
| `LocationVector` | Position, altitude, speed, heading |
| `SystemInfo` | Operator location, area of operation |

### Analysis Classes

| Class | Description |
|-------|-------------|
| `AnomalyDetector` | Detects spoofing and impossible patterns |
| `TrajectoryAnalyzer` | Tracks flight paths, predicts positions |

### Protocol Decoders

| Class | Description |
|-------|-------------|
| `ASTM_F3411_Decoder` | ASTM F3411-22a (USA/International) |
| `ASD_STAN_Decoder` | ASD-STAN EN 4709-002 (EU) |
| `WiFiDecoder` | WiFi Beacon and NAN frames |
| `CN_RID_Decoder` | GB/T Chinese standard (placeholder) |

## Building from Source

### Requirements

- CMake 3.16+
- C++17 compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- (Optional) Android NDK for Android builds
- (Optional) Python 3.8+ for Python bindings

### Build Steps

```bash
# Clone the repository
git clone https://github.com/user/open-remote-id-parser.git
cd open-remote-id-parser

# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --parallel

# Run tests
ctest --output-on-failure

# Install (optional)
sudo cmake --install .
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `ORIP_BUILD_TESTS` | ON | Build unit tests |
| `ORIP_BUILD_EXAMPLES` | ON | Build example programs |
| `ORIP_BUILD_SHARED` | OFF | Build shared library (.so/.dll) |

### Android Build

```bash
cd android
./gradlew :orip:assembleRelease
```

The AAR will be generated at `android/orip/build/outputs/aar/`.

### Python Build

```bash
cd python
pip install build
python -m build
```

## Project Structure

```
open-remote-id-parser/
├── include/orip/           # Public C++ headers
│   ├── orip.h              # Main include file
│   ├── parser.h            # RemoteIDParser class
│   ├── types.h             # Data structures
│   ├── astm_f3411.h        # ASTM decoder
│   ├── asd_stan.h          # ASD-STAN decoder
│   ├── wifi_decoder.h      # WiFi decoder
│   ├── anomaly_detector.h  # Anomaly detection
│   ├── trajectory_analyzer.h # Trajectory analysis
│   └── orip_c.h            # C API
├── src/
│   ├── core/               # Core implementation
│   ├── protocols/          # Protocol decoders
│   ├── analysis/           # Analysis modules
│   └── utils/              # Utilities
├── tests/                  # Unit tests
├── examples/               # Example programs
├── android/                # Android library
│   └── orip/
│       ├── src/main/java/  # Kotlin classes
│       └── src/main/cpp/   # JNI wrapper
├── python/                 # Python bindings
│   ├── orip/               # Python package
│   ├── tests/              # Python tests
│   └── examples/           # Python examples
└── docs/                   # Documentation
```

## Hardware Recommendations

### Entry Level (Mobile Detection)

- Any Android phone with Bluetooth 5.0+
- Detection range: 300-800m (depending on conditions)

### Professional (Fixed Station)

- Raspberry Pi 4 + ESP32-C3 (BLE sniffer)
- External high-gain antenna
- Detection range: 2-5km

### Enterprise

- Software Defined Radio (SDR) setup
- Multiple receivers for triangulation
- Integration with existing security systems

## Contributing

Contributions are welcome! Please read our contributing guidelines before submitting PRs.

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Development Setup

```bash
# Install development dependencies
pip install -e "python/.[dev]"

# Run C++ tests
cd build && ctest

# Run Python tests
cd python && pytest

# Code formatting (if clang-format is installed)
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [ASTM F3411](https://www.astm.org/f3411-22a.html) - Standard Specification for Remote ID
- [ASD-STAN EN 4709-002](https://asd-stan.org/) - European Standard for Remote ID
- [OpenDroneID](https://github.com/opendroneid) - Reference implementations

## Related Projects

- [DroneScanner](https://github.com/example/dronescanner) - Android app using ORIP
- [SkySentry](https://github.com/example/skysentry) - Linux daemon for fixed installations

---

<p align="center">
  <sub>Built with passion for airspace safety</sub>
</p>

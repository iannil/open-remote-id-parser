# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-01-18

### Added

#### Core Library
- **RemoteIDParser**: Main parser class with session management
- **ASTM F3411 Decoder**: Full support for ASTM F3411-22a standard
  - Basic ID (0x0): Serial number, registration ID
  - Location/Vector (0x1): Position, altitude, speed, heading
  - Authentication (0x2): Cryptographic data
  - Self-ID (0x3): Operator description
  - System (0x4): Operator location, area of operation
  - Operator ID (0x5): Operator registration
  - Message Pack (0xF): Multi-message parsing
- **ASD-STAN Decoder**: European standard (EN 4709-002) support
  - EU Operator ID validation
  - Country code extraction
  - EU classification types
- **GB/T Decoder**: Chinese standard interface (placeholder)
- **WiFi Decoder**: WiFi Remote ID support
  - WiFi Beacon (802.11 Vendor Specific IE)
  - WiFi NAN (Neighbor Awareness Networking)
- **Bluetooth Support**:
  - BT 4.x Legacy Advertising
  - BT 5.x Extended Advertising
  - BT 5.x Long Range (Coded PHY)

#### Analysis Features
- **AnomalyDetector**: Detect spoofing and attacks
  - Speed/position anomaly detection
  - Replay attack detection
  - Signal strength analysis
  - Altitude spike detection
- **TrajectoryAnalyzer**: Flight path analysis
  - Trajectory smoothing (exponential filter)
  - Position prediction
  - Flight pattern classification (LINEAR, CIRCULAR, PATROL, etc.)
  - Statistics (distance, speed, duration)

#### Platform Bindings
- **C API** (`orip_c.h`): Pure C interface for FFI
- **Android/Kotlin**: JNI bindings with Kotlin data classes
- **Python**: ctypes-based bindings with dataclasses

#### Session Management
- UAV deduplication by ID
- Automatic timeout handling
- Event callbacks (new UAV, update, timeout)

### Documentation
- Comprehensive README with examples
- API reference documentation
- Contributing guidelines
- MIT License

### Build System
- CMake build system (3.16+)
- Google Test integration
- Android Gradle build (Kotlin DSL)
- Python setuptools packaging

---

## Version History Summary

| Version | Date | Highlights |
|---------|------|------------|
| 0.1.0 | 2026-01-18 | Initial release with full ASTM/ASD-STAN support |

---

## Migration Guide

### From Pre-release to 0.1.0

This is the initial public release. No migration required.

---

## Roadmap

### Planned for 0.2.0
- [ ] Performance optimizations
- [ ] Additional anomaly detection algorithms
- [ ] Geofencing integration
- [ ] Database storage backend

### Planned for 0.3.0
- [ ] GB/T (Chinese standard) full implementation
- [ ] Multi-receiver triangulation
- [ ] REST API server component

---

[Unreleased]: https://github.com/user/open-remote-id-parser/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/user/open-remote-id-parser/releases/tag/v0.1.0

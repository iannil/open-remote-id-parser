# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |

## Reporting a Vulnerability

We take the security of ORIP (Open-RemoteID-Parser) seriously. If you discover a security vulnerability, please report it responsibly.

### How to Report

1. **DO NOT** create a public GitHub issue for security vulnerabilities
2. Send a detailed report to: [security@your-domain.com] (or create a private security advisory on GitHub)
3. Include the following information:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if any)

### What to Expect

- **Acknowledgment**: We will acknowledge receipt within 48 hours
- **Initial Assessment**: We will provide an initial assessment within 7 days
- **Resolution Timeline**: Critical vulnerabilities will be addressed within 30 days
- **Disclosure**: We follow coordinated disclosure practices

---

## Security Practices

### Code Security Measures

ORIP implements multiple layers of security to ensure safe parsing of potentially malicious Remote ID data:

#### 1. Input Validation

- **Payload Size Limits**: All inputs are validated against maximum size limits before processing
- **Boundary Checks**: Pointer arithmetic is protected with bounds checking
- **Integer Overflow Protection**: Critical integer operations are guarded against overflow/underflow

```cpp
// Example: Safe length check before processing
if (payload.size() < MIN_MESSAGE_LENGTH || payload.size() > MAX_MESSAGE_LENGTH) {
    return ParseResult::error("Invalid payload size");
}
```

#### 2. Memory Safety

- **No Raw Pointer Arithmetic**: Uses `std::vector` and safe accessors
- **RAII Patterns**: Resources are managed through RAII to prevent leaks
- **No Manual Memory Management**: Avoids `new`/`delete` in favor of smart pointers

#### 3. Fuzz Testing

ORIP is continuously fuzz-tested using libFuzzer to discover edge cases:

```bash
# Run fuzz tests
CC=clang CXX=clang++ cmake .. -DORIP_BUILD_FUZZ=ON
cmake --build . --target fuzz_parser
./fuzz_parser ../fuzz/corpus -max_len=512 -max_total_time=60
```

#### 4. Sanitizer Integration

CI pipeline includes multiple sanitizers:

| Sanitizer | Purpose | CI Status |
|-----------|---------|-----------|
| AddressSanitizer (ASan) | Memory errors | :white_check_mark: Enabled |
| UndefinedBehaviorSanitizer (UBSan) | Undefined behavior | :white_check_mark: Enabled |
| ThreadSanitizer (TSan) | Data races | :white_check_mark: Enabled |
| LeakSanitizer (LSan) | Memory leaks | :white_check_mark: Enabled |

---

## Security Considerations for Users

### Threat Model

ORIP parses Remote ID broadcasts which may come from:
- Legitimate UAVs (drones)
- Malicious actors attempting to spoof or disrupt
- Corrupted or malformed transmissions

### Potential Attack Vectors

| Attack Vector | Risk Level | Mitigation |
|---------------|------------|------------|
| Malformed payloads | Medium | Input validation, fuzz testing |
| Integer overflow | Medium | Bounds checking, sanitizers |
| Buffer overflow | High | No raw pointers, std::vector |
| Replay attacks | Medium | Timestamp validation, anomaly detection |
| ID spoofing | High | Anomaly detection, signal analysis |

### Best Practices for Integration

1. **Isolate the Parser**
   ```cpp
   // Run parser in a sandboxed environment if possible
   RemoteIDParser parser(config);
   // Process untrusted input
   auto result = parser.parse(untrusted_data, rssi, transport);
   ```

2. **Validate Results**
   ```cpp
   if (!result.success) {
       // Handle parsing failure gracefully
       log_warning("Parse failed: " + result.error_message);
       return;
   }
   ```

3. **Use Anomaly Detection**
   ```cpp
   // Enable anomaly detection to identify suspicious patterns
   AnomalyDetector detector(config);
   auto anomalies = detector.analyze(uav, rssi);
   for (const auto& a : anomalies) {
       if (a.confidence > 0.8) {
           log_alert("High-confidence anomaly: " + a.description);
       }
   }
   ```

4. **Limit Resource Usage**
   ```cpp
   // Configure session manager with reasonable limits
   ParserConfig config;
   config.session_timeout_ms = 30000;  // 30 seconds
   config.max_uav_count = 1000;        // Limit tracked UAVs
   ```

---

## Security Audit Status

### Internal Audit (v0.1.0)

| Component | Status | Notes |
|-----------|--------|-------|
| Protocol Parsing (ASTM F3411) | :white_check_mark: Audited | Integer bounds verified |
| Protocol Parsing (ASD-STAN) | :white_check_mark: Audited | Input validation verified |
| WiFi Decoder | :white_check_mark: Audited | Pointer safety verified |
| Session Manager | :white_check_mark: Audited | Thread safety with TSan |
| Anomaly Detector | :white_check_mark: Audited | Floating-point precision fixed |
| C API Bindings | :white_check_mark: Audited | Memory ownership clear |
| Python Bindings | :white_check_mark: Audited | GIL handling verified |
| Android JNI | :yellow_circle: Partial | Requires Android testing environment |

### Known Security Fixes

| Version | Issue | Fix |
|---------|-------|-----|
| 0.1.0 | Integer underflow in message length calculation | Added pre-check for minimum length |
| 0.1.0 | Floating-point precision in anomaly detection | Added epsilon comparison |
| 0.1.0 | Pointer bounds in ASTM parser | Added boundary validation |

---

## Malicious Input Protection

ORIP includes comprehensive protection against malicious inputs:

### Tested Attack Patterns

- **Empty payloads** - Gracefully rejected
- **Oversized payloads** (10KB, 1MB) - Rejected with size limit
- **All-zero payloads** - Handled without crash
- **All-0xFF payloads** - Handled without crash
- **Random byte sequences** - No undefined behavior
- **Format string attacks** (%n, %s) - No injection possible
- **Truncated messages** - Detected and rejected
- **Invalid message types** - Rejected with error
- **Protocol confusion attacks** - Handled by protocol detection
- **Replay attacks** - Detected by anomaly detector
- **MessagePack nesting attacks** - Depth-limited

### Test Coverage

```
Malicious input tests: 40+ test cases
Boundary condition tests: 25+ test cases
Thread safety tests: 20+ test cases
```

---

## Dependency Security

### Core Dependencies

| Dependency | Version | Security Status |
|------------|---------|-----------------|
| C++ Standard Library | C++17 | N/A (compiler-provided) |
| Google Test | 1.11+ | :white_check_mark: No known vulnerabilities |
| Google Benchmark | 1.8+ | :white_check_mark: No known vulnerabilities |

### Build Dependencies

- CMake 3.14+ (build only)
- Python 3.8+ (testing only)
- Clang (fuzzing only)

---

## Security Checklist for Contributors

When contributing code, please ensure:

- [ ] No use of raw `new`/`delete`
- [ ] All pointer arithmetic has bounds checks
- [ ] All integer arithmetic considers overflow
- [ ] All external inputs are validated
- [ ] No format string vulnerabilities
- [ ] Thread-safe if accessed from multiple threads
- [ ] Tests pass with ASan and UBSan enabled
- [ ] Fuzz tests run without crashes

---

## Contact

For security concerns, please contact:
- **Security Issues**: Create a private security advisory on GitHub
- **General Questions**: Open a GitHub issue

---

> **Last Updated**: 2026-01-23
> **Security Policy Version**: 1.0

# ORIP Fuzz Testing

This directory contains fuzz testing infrastructure using libFuzzer.

## Prerequisites

- Clang compiler with libFuzzer support
- CMake 3.16+

## Building

```bash
# Configure with fuzzing enabled
CC=clang CXX=clang++ cmake -B build -DORIP_BUILD_FUZZ=ON -DORIP_BUILD_TESTS=OFF

# Build fuzz target
cmake --build build --target fuzz_parser
```

## Running

### Quick Test (60 seconds)

```bash
./build/fuzz_parser fuzz/corpus -max_len=512 -timeout=5 -max_total_time=60
```

### Extended Test (10 minutes)

```bash
./build/fuzz_parser fuzz/corpus -max_len=512 -timeout=5 -max_total_time=600 -jobs=4
```

### Using CMake Target

```bash
cmake --build build --target run_fuzz
```

## Corpus

The `corpus/` directory contains seed inputs:

- `seed_basic_id.bin` - ASTM Basic ID message
- `seed_location.bin` - ASTM Location message
- `seed_serial.bin` - Serial number format
- `seed_wifi_beacon.bin` - WiFi beacon frame
- `seed_all_00.bin` - All zeros boundary test
- `seed_all_ff.bin` - All 0xFF boundary test

## Crash Analysis

If the fuzzer finds a crash, it will save the input to files like:
- `crash-<hash>` - Crashes
- `leak-<hash>` - Memory leaks
- `timeout-<hash>` - Timeouts

Reproduce a crash:
```bash
./build/fuzz_parser crash-xxx
```

## Coverage

To run with coverage:
```bash
CC=clang CXX=clang++ cmake -B build -DORIP_BUILD_FUZZ=ON -DORIP_ENABLE_COVERAGE=ON
cmake --build build --target fuzz_parser
./build/fuzz_parser fuzz/corpus -max_len=512 -max_total_time=60
llvm-profdata merge -sparse default.profraw -o default.profdata
llvm-cov show ./build/fuzz_parser -instr-profile=default.profdata
```

## CI Integration

Fuzzing runs automatically in CI for 60 seconds on each push. Crashes are uploaded as artifacts.

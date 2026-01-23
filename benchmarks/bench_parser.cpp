// =============================================================================
// ORIP Performance Benchmarks
// =============================================================================
// This file contains performance benchmarks for critical parsing paths
// using Google Benchmark framework.
//
// Run with: ./orip_benchmarks --benchmark_format=console
// =============================================================================

#include <benchmark/benchmark.h>
#include "orip/parser.h"
#include "orip/astm_f3411.h"
#include "orip/asd_stan.h"
#include <vector>
#include <random>
#include <chrono>

using namespace orip;
using namespace orip::astm;

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

// Create a valid BLE advertisement with ODID Basic ID message
std::vector<uint8_t> createBasicIDPacket(const std::string& serial = "BENCH-UAV-001") {
    std::vector<uint8_t> adv;

    // BLE Service Data AD structure
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
    msg[0] = 0x02;  // Basic ID, version 2
    msg[1] = 0x12;  // Serial Number, Multirotor

    for (size_t i = 0; i < serial.size() && i < BASIC_ID_LENGTH; i++) {
        msg[2 + i] = static_cast<uint8_t>(serial[i]);
    }

    uint8_t len = static_cast<uint8_t>(3 + 1 + msg.size());
    adv.push_back(len);
    adv.push_back(0x16);  // Service Data AD type
    adv.push_back(0xFA);  // UUID low (0xFFFA)
    adv.push_back(0xFF);  // UUID high
    adv.push_back(0x00);  // Message counter
    adv.insert(adv.end(), msg.begin(), msg.end());

    return adv;
}

// Create a valid BLE advertisement with ODID Location message
std::vector<uint8_t> createLocationPacket(double lat = 37.7749, double lon = -122.4194, float alt = 100.0f) {
    std::vector<uint8_t> adv;

    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
    msg[0] = 0x12;  // Location type (0x1), version 2
    msg[1] = 0x20;  // Airborne status

    int32_t lat_enc = static_cast<int32_t>(lat * 1e7);
    int32_t lon_enc = static_cast<int32_t>(lon * 1e7);

    msg[5] = lat_enc & 0xFF;
    msg[6] = (lat_enc >> 8) & 0xFF;
    msg[7] = (lat_enc >> 16) & 0xFF;
    msg[8] = (lat_enc >> 24) & 0xFF;

    msg[9] = lon_enc & 0xFF;
    msg[10] = (lon_enc >> 8) & 0xFF;
    msg[11] = (lon_enc >> 16) & 0xFF;
    msg[12] = (lon_enc >> 24) & 0xFF;

    uint16_t alt_enc = static_cast<uint16_t>((alt + 1000.0f) / 0.5f);
    msg[13] = alt_enc & 0xFF;
    msg[14] = (alt_enc >> 8) & 0xFF;
    msg[15] = alt_enc & 0xFF;
    msg[16] = (alt_enc >> 8) & 0xFF;

    uint8_t len = static_cast<uint8_t>(3 + 1 + msg.size());
    adv.push_back(len);
    adv.push_back(0x16);
    adv.push_back(0xFA);
    adv.push_back(0xFF);
    adv.push_back(0x00);
    adv.insert(adv.end(), msg.begin(), msg.end());

    return adv;
}

// Create random payload for stress testing
std::vector<uint8_t> createRandomPayload(size_t size, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);

    std::vector<uint8_t> payload(size);
    for (size_t i = 0; i < size; i++) {
        payload[i] = static_cast<uint8_t>(dist(rng));
    }
    return payload;
}

}  // namespace

// =============================================================================
// Parser Initialization Benchmarks
// =============================================================================

static void BM_ParserCreation(benchmark::State& state) {
    for (auto _ : state) {
        RemoteIDParser parser;
        benchmark::DoNotOptimize(parser);
    }
}
BENCHMARK(BM_ParserCreation);

static void BM_ParserInit(benchmark::State& state) {
    for (auto _ : state) {
        RemoteIDParser parser;
        parser.init();
        benchmark::DoNotOptimize(parser);
    }
}
BENCHMARK(BM_ParserInit);

static void BM_ParserWithConfig(benchmark::State& state) {
    for (auto _ : state) {
        ParserConfig config;
        config.enable_astm = true;
        config.enable_asd = true;
        config.enable_cn = false;
        config.enable_deduplication = true;

        RemoteIDParser parser(config);
        parser.init();
        benchmark::DoNotOptimize(parser);
    }
}
BENCHMARK(BM_ParserWithConfig);

// =============================================================================
// Basic ID Parsing Benchmarks
// =============================================================================

static void BM_ParseBasicID(benchmark::State& state) {
    RemoteIDParser parser;
    parser.init();

    auto packet = createBasicIDPacket("BENCH-UAV-001");

    for (auto _ : state) {
        auto result = parser.parse(packet, -60, TransportType::BT_LEGACY);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * packet.size());
}
BENCHMARK(BM_ParseBasicID);

static void BM_ParseBasicID_NoDedup(benchmark::State& state) {
    ParserConfig config;
    config.enable_deduplication = false;

    RemoteIDParser parser(config);
    parser.init();

    auto packet = createBasicIDPacket("BENCH-UAV-002");

    for (auto _ : state) {
        auto result = parser.parse(packet, -60, TransportType::BT_LEGACY);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ParseBasicID_NoDedup);

// =============================================================================
// Location Message Parsing Benchmarks
// =============================================================================

static void BM_ParseLocation(benchmark::State& state) {
    RemoteIDParser parser;
    parser.init();

    auto packet = createLocationPacket(37.7749, -122.4194, 100.0f);

    for (auto _ : state) {
        auto result = parser.parse(packet, -60, TransportType::BT_LEGACY);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * packet.size());
}
BENCHMARK(BM_ParseLocation);

// =============================================================================
// Protocol Detection Benchmarks
// =============================================================================

static void BM_ProtocolDetection_ASTM(benchmark::State& state) {
    ParserConfig config;
    config.enable_astm = true;
    config.enable_asd = true;
    config.enable_cn = true;

    RemoteIDParser parser(config);
    parser.init();

    auto packet = createBasicIDPacket("PROTO-TEST-001");

    for (auto _ : state) {
        auto result = parser.parse(packet, -60, TransportType::BT_LEGACY);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_ProtocolDetection_ASTM);

static void BM_ProtocolDetection_OnlyAST(benchmark::State& state) {
    ParserConfig config;
    config.enable_astm = true;
    config.enable_asd = false;
    config.enable_cn = false;

    RemoteIDParser parser(config);
    parser.init();

    auto packet = createBasicIDPacket("PROTO-TEST-002");

    for (auto _ : state) {
        auto result = parser.parse(packet, -60, TransportType::BT_LEGACY);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_ProtocolDetection_OnlyAST);

// =============================================================================
// Invalid/Rejection Benchmarks
// =============================================================================

static void BM_RejectEmpty(benchmark::State& state) {
    RemoteIDParser parser;
    parser.init();

    std::vector<uint8_t> empty;

    for (auto _ : state) {
        auto result = parser.parse(empty, -60, TransportType::BT_LEGACY);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RejectEmpty);

static void BM_RejectRandom(benchmark::State& state) {
    RemoteIDParser parser;
    parser.init();

    auto random = createRandomPayload(50);

    for (auto _ : state) {
        auto result = parser.parse(random, -60, TransportType::BT_LEGACY);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RejectRandom);

static void BM_RejectTooShort(benchmark::State& state) {
    RemoteIDParser parser;
    parser.init();

    std::vector<uint8_t> short_payload = {0x01, 0x02, 0x03};

    for (auto _ : state) {
        auto result = parser.parse(short_payload, -60, TransportType::BT_LEGACY);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RejectTooShort);

// =============================================================================
// Multi-UAV Tracking Benchmarks
// =============================================================================

static void BM_TrackMultipleUAVs(benchmark::State& state) {
    const int num_uavs = state.range(0);

    ParserConfig config;
    config.enable_deduplication = true;

    RemoteIDParser parser(config);
    parser.init();

    // Pre-create packets for different UAVs
    std::vector<std::vector<uint8_t>> packets;
    for (int i = 0; i < num_uavs; i++) {
        packets.push_back(createBasicIDPacket("UAV-" + std::to_string(i)));
    }

    for (auto _ : state) {
        for (const auto& packet : packets) {
            auto result = parser.parse(packet, -60, TransportType::BT_LEGACY);
            benchmark::DoNotOptimize(result);
        }
    }

    state.SetItemsProcessed(state.iterations() * num_uavs);
}
BENCHMARK(BM_TrackMultipleUAVs)->Arg(10)->Arg(50)->Arg(100);

static void BM_GetActiveUAVs(benchmark::State& state) {
    const int num_uavs = state.range(0);

    ParserConfig config;
    config.enable_deduplication = true;

    RemoteIDParser parser(config);
    parser.init();

    // Pre-populate parser with UAVs
    for (int i = 0; i < num_uavs; i++) {
        auto packet = createBasicIDPacket("UAV-" + std::to_string(i));
        parser.parse(packet, -60, TransportType::BT_LEGACY);
    }

    for (auto _ : state) {
        auto uavs = parser.getActiveUAVs();
        benchmark::DoNotOptimize(uavs);
    }
}
BENCHMARK(BM_GetActiveUAVs)->Arg(10)->Arg(50)->Arg(100);

// =============================================================================
// RawFrame API Benchmark
// =============================================================================

static void BM_ParseRawFrame(benchmark::State& state) {
    RemoteIDParser parser;
    parser.init();

    auto packet = createBasicIDPacket("RAWFRAME-TEST");

    RawFrame frame;
    frame.payload = packet;
    frame.rssi = -60;
    frame.transport = TransportType::BT_LEGACY;
    frame.timestamp = std::chrono::steady_clock::now();

    for (auto _ : state) {
        auto result = parser.parse(frame);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ParseRawFrame);

// =============================================================================
// Throughput Benchmark
// =============================================================================

static void BM_Throughput(benchmark::State& state) {
    RemoteIDParser parser;
    parser.init();

    // Simulate realistic traffic: mix of Basic ID and Location messages
    std::vector<std::vector<uint8_t>> packets;
    for (int i = 0; i < 100; i++) {
        if (i % 3 == 0) {
            packets.push_back(createBasicIDPacket("UAV-" + std::to_string(i / 3)));
        } else {
            double lat = 37.0 + (i % 10) * 0.01;
            double lon = -122.0 + (i % 10) * 0.01;
            packets.push_back(createLocationPacket(lat, lon, 100.0f + i));
        }
    }

    size_t total_bytes = 0;
    for (const auto& p : packets) {
        total_bytes += p.size();
    }

    for (auto _ : state) {
        for (const auto& packet : packets) {
            auto result = parser.parse(packet, -60 - (rand() % 30), TransportType::BT_LEGACY);
            benchmark::DoNotOptimize(result);
        }
    }

    state.SetItemsProcessed(state.iterations() * packets.size());
    state.SetBytesProcessed(state.iterations() * total_bytes);
}
BENCHMARK(BM_Throughput);

// =============================================================================
// Main
// =============================================================================

BENCHMARK_MAIN();

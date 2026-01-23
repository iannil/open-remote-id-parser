// =============================================================================
// ORIP Fuzz Target - libFuzzer entry point
// =============================================================================
// Build with: clang++ -g -O1 -fno-omit-frame-pointer -fsanitize=fuzzer,address
//             fuzz/fuzz_parser.cpp -I include src/*.cpp -o fuzz_parser
//
// Run with: ./fuzz_parser fuzz/corpus -max_len=512 -timeout=5
// =============================================================================

#include <cstdint>
#include <cstddef>
#include <vector>

#include "orip/orip.h"
#include "orip/parser.h"
#include "orip/astm_f3411.h"
#include "orip/wifi_decoder.h"

using namespace orip;

// Global parser instance (initialized once)
static RemoteIDParser* g_parser = nullptr;
static astm::ASTM_F3411_Decoder* g_astm_decoder = nullptr;
static wifi::WiFiDecoder* g_wifi_decoder = nullptr;

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
    (void)argc;
    (void)argv;

    // Initialize global instances
    g_parser = new RemoteIDParser();
    g_parser->init();

    g_astm_decoder = new astm::ASTM_F3411_Decoder();
    g_wifi_decoder = new wifi::WiFiDecoder();

    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > 1024) {
        return 0;
    }

    std::vector<uint8_t> payload(data, data + size);

    // Test main parser with different transport types
    {
        auto result = g_parser->parse(payload, -70, TransportType::BT_LEGACY);
        (void)result;
    }

    {
        auto result = g_parser->parse(payload, -70, TransportType::BT_EXTENDED);
        (void)result;
    }

    {
        auto result = g_parser->parse(payload, -70, TransportType::WIFI_BEACON);
        (void)result;
    }

    // Test ASTM decoder directly
    {
        UAVObject uav;
        auto result = g_astm_decoder->decode(payload, uav);
        (void)result;
    }

    // Test ASTM message decoder with raw data
    if (size >= astm::MESSAGE_SIZE) {
        UAVObject uav;
        auto result = g_astm_decoder->decodeMessage(data, size, uav);
        (void)result;
    }

    // Test WiFi decoder
    {
        UAVObject uav;
        auto result = g_wifi_decoder->decodeBeacon(payload, uav);
        (void)result;
    }

    {
        UAVObject uav;
        auto result = g_wifi_decoder->decodeNAN(payload, uav);
        (void)result;
    }

    {
        UAVObject uav;
        auto result = g_wifi_decoder->decodeVendorIE(payload, uav);
        (void)result;
    }

    return 0;
}

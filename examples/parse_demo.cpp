#include <iostream>
#include <iomanip>
#include "orip/orip.h"

// Example: Parsing a simulated Remote ID advertisement

int main() {
    std::cout << "Open Remote ID Parser v" << orip::VERSION << std::endl;
    std::cout << "========================================" << std::endl;

    // Create parser with default config
    orip::RemoteIDParser parser;
    parser.init();

    // Set callbacks for UAV events
    parser.setOnNewUAV([](const orip::UAVObject& uav) {
        std::cout << "[NEW UAV] ID: " << uav.id << std::endl;
    });

    parser.setOnUAVUpdate([](const orip::UAVObject& uav) {
        std::cout << "[UPDATE] ID: " << uav.id
                  << " Lat: " << std::fixed << std::setprecision(6) << uav.location.latitude
                  << " Lon: " << uav.location.longitude
                  << std::endl;
    });

    // Simulated BLE advertisement with ODID Basic ID message
    // This represents a DJI drone broadcasting its serial number
    std::vector<uint8_t> sample_advertisement = {
        // AD Structure header
        0x1E,  // Length (30 bytes)
        0x16,  // AD Type: Service Data - 16-bit UUID
        0xFA, 0xFF,  // UUID: 0xFFFA (ASTM Remote ID)
        0x00,  // Message counter

        // Basic ID Message (25 bytes)
        0x02,  // Message type (0x0) | Proto version (0x2)
        0x12,  // ID type: Serial (1) | UA type: Multirotor (2)
        // Serial number: "DJI1234567890ABCD"
        'D', 'J', 'I', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', '0', 'A', 'B', 'C', 'D', 0, 0, 0,
        0, 0, 0, 0  // Padding
    };

    std::cout << "\nParsing sample advertisement..." << std::endl;

    auto result = parser.parse(sample_advertisement, -65, orip::TransportType::BT_LEGACY);

    if (result.success) {
        std::cout << "\n[SUCCESS] Remote ID detected!" << std::endl;
        std::cout << "  Protocol: ASTM F3411" << std::endl;
        std::cout << "  UAV ID: " << result.uav.id << std::endl;
        std::cout << "  UAV Type: " << static_cast<int>(result.uav.uav_type) << std::endl;
        std::cout << "  RSSI: " << static_cast<int>(result.uav.rssi) << " dBm" << std::endl;
    } else {
        std::cout << "[FAILED] " << result.error << std::endl;
    }

    std::cout << "\nActive UAVs: " << parser.getActiveCount() << std::endl;

    return 0;
}

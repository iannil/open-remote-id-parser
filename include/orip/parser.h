#ifndef ORIP_PARSER_H
#define ORIP_PARSER_H

#include "orip/types.h"
#include <memory>
#include <vector>
#include <functional>

namespace orip {

// Forward declarations
class SessionManager;
class ProtocolDecoder;

// Configuration options for the parser
struct ParserConfig {
    // Session management
    uint32_t uav_timeout_ms = 30000;    // Remove UAV after no signal (30s default)
    bool enable_deduplication = true;    // Deduplicate by UAV ID

    // Protocol options
    bool enable_astm = true;             // Enable ASTM F3411 parsing
    bool enable_asd = false;             // Enable ASD-STAN parsing (not yet implemented)
    bool enable_cn = false;              // Enable CN-RID parsing (not yet implemented)
};

// Callback for UAV events
using UAVCallback = std::function<void(const UAVObject&)>;

// Main parser class
class RemoteIDParser {
public:
    RemoteIDParser();
    explicit RemoteIDParser(const ParserConfig& config);
    ~RemoteIDParser();

    // Non-copyable
    RemoteIDParser(const RemoteIDParser&) = delete;
    RemoteIDParser& operator=(const RemoteIDParser&) = delete;

    // Movable
    RemoteIDParser(RemoteIDParser&&) noexcept;
    RemoteIDParser& operator=(RemoteIDParser&&) noexcept;

    // Initialize the parser
    void init();

    // Parse a raw frame
    // Returns ParseResult with success/failure and parsed UAV data
    ParseResult parse(const RawFrame& frame);

    // Convenience overload: parse raw bytes with RSSI
    ParseResult parse(const std::vector<uint8_t>& payload, int8_t rssi,
                      TransportType transport = TransportType::BT_LEGACY);

    // Get list of currently active UAVs
    std::vector<UAVObject> getActiveUAVs() const;

    // Get a specific UAV by ID (returns nullptr if not found)
    const UAVObject* getUAV(const std::string& id) const;

    // Get count of active UAVs
    size_t getActiveCount() const;

    // Clear all tracked UAVs
    void clear();

    // Set callback for new UAV detection
    void setOnNewUAV(UAVCallback callback);

    // Set callback for UAV update
    void setOnUAVUpdate(UAVCallback callback);

    // Set callback for UAV timeout (removed)
    void setOnUAVTimeout(UAVCallback callback);

    // Manually trigger cleanup of timed-out UAVs
    void cleanup();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace orip

#endif // ORIP_PARSER_H

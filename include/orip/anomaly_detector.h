#ifndef ORIP_ANOMALY_DETECTOR_H
#define ORIP_ANOMALY_DETECTOR_H

#include "orip/types.h"
#include <deque>
#include <unordered_map>
#include <chrono>
#include <cmath>

namespace orip {
namespace analysis {

// Anomaly types that can be detected
enum class AnomalyType : uint8_t {
    NONE = 0,
    SPEED_IMPOSSIBLE = 1,       // Speed exceeds physical limits
    POSITION_JUMP = 2,          // Position jumped impossibly fast
    ALTITUDE_SPIKE = 3,         // Sudden altitude change
    REPLAY_ATTACK = 4,          // Duplicate message detected
    SIGNAL_ANOMALY = 5,         // RSSI inconsistent with distance
    TIMESTAMP_ANOMALY = 6,      // Timestamp out of sequence
    ID_SPOOF = 7                // Multiple locations for same ID
};

// Severity levels
enum class AnomalySeverity : uint8_t {
    INFO = 0,       // Informational, might be normal
    WARNING = 1,    // Suspicious, needs attention
    CRITICAL = 2    // Definite anomaly, likely spoofing
};

// Detected anomaly record
struct Anomaly {
    AnomalyType type = AnomalyType::NONE;
    AnomalySeverity severity = AnomalySeverity::INFO;
    std::string uav_id;
    std::string description;
    double confidence = 0.0;    // 0.0 - 1.0
    std::chrono::steady_clock::time_point detected_at;

    // Context data
    double expected_value = 0.0;
    double actual_value = 0.0;
};

// Configuration for anomaly detection
struct AnomalyConfig {
    // Speed thresholds (m/s)
    float max_horizontal_speed = 150.0f;    // ~540 km/h
    float max_vertical_speed = 50.0f;       // 50 m/s
    float max_acceleration = 30.0f;         // m/s^2

    // Position thresholds
    double max_position_jump_m = 1000.0;    // 1km max jump
    float max_altitude_change_rate = 100.0f; // m/s

    // Replay detection
    uint32_t replay_window_ms = 5000;       // 5 second window
    size_t min_duplicate_count = 3;         // Duplicates to flag

    // Signal analysis
    float rssi_distance_tolerance = 0.3f;   // 30% tolerance
    int8_t min_rssi_change = 20;            // dB change threshold

    // Timing
    uint32_t max_timestamp_gap_ms = 10000;  // 10 second max gap
};

// History entry for a UAV
struct UAVHistory {
    std::string id;
    std::deque<LocationVector> positions;
    std::deque<int8_t> rssi_history;
    std::deque<std::chrono::steady_clock::time_point> timestamps;
    std::deque<uint32_t> message_hashes;

    size_t max_history = 100;

    void addPosition(const LocationVector& loc,
                     int8_t rssi,
                     std::chrono::steady_clock::time_point time,
                     uint32_t msg_hash);

    void trim();
};

/**
 * Anomaly Detector
 *
 * Detects suspicious patterns in Remote ID data that may indicate:
 * - Spoofed signals
 * - Replay attacks
 * - Signal manipulation
 * - Physical impossibilities
 */
class AnomalyDetector {
public:
    AnomalyDetector();
    explicit AnomalyDetector(const AnomalyConfig& config);

    /**
     * Analyze a UAV update for anomalies
     * @param uav Current UAV data
     * @param rssi Signal strength
     * @return List of detected anomalies (may be empty)
     */
    std::vector<Anomaly> analyze(const UAVObject& uav, int8_t rssi);

    /**
     * Check for speed anomalies
     */
    std::vector<Anomaly> checkSpeedAnomalies(const std::string& id,
                                              const LocationVector& current,
                                              const LocationVector& previous,
                                              double time_delta_s);

    /**
     * Check for position anomalies
     */
    std::vector<Anomaly> checkPositionAnomalies(const std::string& id,
                                                 const LocationVector& current,
                                                 const LocationVector& previous,
                                                 double time_delta_s);

    /**
     * Check for replay attacks
     */
    std::vector<Anomaly> checkReplayAttack(const std::string& id,
                                            uint32_t message_hash);

    /**
     * Check RSSI consistency
     */
    std::vector<Anomaly> checkSignalAnomaly(const std::string& id,
                                             int8_t current_rssi,
                                             const LocationVector& location);

    /**
     * Get anomaly statistics
     */
    size_t getTotalAnomalies() const { return total_anomalies_; }
    size_t getAnomalyCount(AnomalyType type) const;

    /**
     * Clear history for all UAVs
     */
    void clear();

    /**
     * Clear history for specific UAV
     */
    void clearUAV(const std::string& id);

    /**
     * Get configuration
     */
    const AnomalyConfig& config() const { return config_; }

private:
    AnomalyConfig config_;
    std::unordered_map<std::string, UAVHistory> history_;
    std::unordered_map<AnomalyType, size_t> anomaly_counts_;
    size_t total_anomalies_ = 0;

    // Helper functions
    double calculateDistance(double lat1, double lon1, double lat2, double lon2) const;
    uint32_t hashMessage(const UAVObject& uav) const;
    double estimateDistanceFromRSSI(int8_t rssi) const;
};

} // namespace analysis
} // namespace orip

#endif // ORIP_ANOMALY_DETECTOR_H

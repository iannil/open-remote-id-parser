#include "orip/anomaly_detector.h"
#include <cmath>
#include <functional>
#include <algorithm>

namespace orip {
namespace analysis {

// Earth radius in meters
constexpr double EARTH_RADIUS_M = 6371000.0;

// RSSI to distance approximation constants (path loss model)
constexpr double RSSI_REF = -50.0;      // Reference RSSI at 1m
constexpr double PATH_LOSS_EXP = 2.5;   // Path loss exponent

void UAVHistory::addPosition(const LocationVector& loc,
                              int8_t rssi,
                              std::chrono::steady_clock::time_point time,
                              uint32_t msg_hash) {
    positions.push_back(loc);
    rssi_history.push_back(rssi);
    timestamps.push_back(time);
    message_hashes.push_back(msg_hash);
    trim();
}

void UAVHistory::trim() {
    while (positions.size() > max_history) {
        positions.pop_front();
        rssi_history.pop_front();
        timestamps.pop_front();
        message_hashes.pop_front();
    }
}

AnomalyDetector::AnomalyDetector() : AnomalyDetector(AnomalyConfig{}) {}

AnomalyDetector::AnomalyDetector(const AnomalyConfig& config) : config_(config) {}

std::vector<Anomaly> AnomalyDetector::analyze(const UAVObject& uav, int8_t rssi) {
    std::vector<Anomaly> anomalies;

    if (uav.id.empty()) {
        return anomalies;
    }

    auto now = std::chrono::steady_clock::now();
    uint32_t msg_hash = hashMessage(uav);

    // Get or create history
    auto& hist = history_[uav.id];
    hist.id = uav.id;

    // Check replay attack first
    auto replay = checkReplayAttack(uav.id, msg_hash);
    anomalies.insert(anomalies.end(), replay.begin(), replay.end());

    // If we have previous data, check for other anomalies
    if (!hist.positions.empty() && uav.location.valid) {
        const auto& prev = hist.positions.back();
        auto prev_time = hist.timestamps.back();

        double time_delta = std::chrono::duration<double>(now - prev_time).count();

        if (time_delta > 0.0 && time_delta < config_.max_timestamp_gap_ms / 1000.0) {
            // Check speed anomalies
            auto speed_anomalies = checkSpeedAnomalies(uav.id, uav.location, prev, time_delta);
            anomalies.insert(anomalies.end(), speed_anomalies.begin(), speed_anomalies.end());

            // Check position anomalies
            auto pos_anomalies = checkPositionAnomalies(uav.id, uav.location, prev, time_delta);
            anomalies.insert(anomalies.end(), pos_anomalies.begin(), pos_anomalies.end());
        }

        // Check signal anomaly
        auto sig_anomalies = checkSignalAnomaly(uav.id, rssi, uav.location);
        anomalies.insert(anomalies.end(), sig_anomalies.begin(), sig_anomalies.end());
    }

    // Add to history
    if (uav.location.valid) {
        hist.addPosition(uav.location, rssi, now, msg_hash);
    }

    // Update counts
    for (const auto& a : anomalies) {
        anomaly_counts_[a.type]++;
        total_anomalies_++;
    }

    return anomalies;
}

std::vector<Anomaly> AnomalyDetector::checkSpeedAnomalies(
    const std::string& id,
    const LocationVector& current,
    const LocationVector& previous,
    double time_delta_s) {

    std::vector<Anomaly> anomalies;

    if (time_delta_s <= 0.0) {
        return anomalies;
    }

    // Check horizontal speed
    double distance = calculateDistance(
        previous.latitude, previous.longitude,
        current.latitude, current.longitude
    );

    double calculated_speed = distance / time_delta_s;

    if (calculated_speed > config_.max_horizontal_speed) {
        Anomaly a;
        a.type = AnomalyType::SPEED_IMPOSSIBLE;
        a.severity = calculated_speed > config_.max_horizontal_speed * 2 ?
                     AnomalySeverity::CRITICAL : AnomalySeverity::WARNING;
        a.uav_id = id;
        a.description = "Calculated horizontal speed exceeds physical limits";
        a.expected_value = config_.max_horizontal_speed;
        a.actual_value = calculated_speed;
        a.confidence = std::min(1.0, calculated_speed / (config_.max_horizontal_speed * 3));
        a.detected_at = std::chrono::steady_clock::now();

        anomalies.push_back(a);
    }

    // Check vertical speed
    float altitude_change = std::abs(current.altitude_geo - previous.altitude_geo);
    float vertical_speed = altitude_change / static_cast<float>(time_delta_s);

    if (vertical_speed > config_.max_vertical_speed) {
        Anomaly a;
        a.type = AnomalyType::ALTITUDE_SPIKE;
        a.severity = vertical_speed > config_.max_vertical_speed * 2 ?
                     AnomalySeverity::CRITICAL : AnomalySeverity::WARNING;
        a.uav_id = id;
        a.description = "Vertical speed exceeds physical limits";
        a.expected_value = config_.max_vertical_speed;
        a.actual_value = vertical_speed;
        a.confidence = std::min(1.0, static_cast<double>(vertical_speed) /
                                      (config_.max_vertical_speed * 3));
        a.detected_at = std::chrono::steady_clock::now();

        anomalies.push_back(a);
    }

    // Check acceleration (if we have reported speed)
    if (current.speed_horizontal >= 0 && previous.speed_horizontal >= 0) {
        float speed_change = std::abs(current.speed_horizontal - previous.speed_horizontal);
        float acceleration = speed_change / static_cast<float>(time_delta_s);

        if (acceleration > config_.max_acceleration) {
            Anomaly a;
            a.type = AnomalyType::SPEED_IMPOSSIBLE;
            a.severity = AnomalySeverity::WARNING;
            a.uav_id = id;
            a.description = "Acceleration exceeds reasonable limits";
            a.expected_value = config_.max_acceleration;
            a.actual_value = acceleration;
            a.confidence = std::min(1.0, static_cast<double>(acceleration) /
                                          (config_.max_acceleration * 2));
            a.detected_at = std::chrono::steady_clock::now();

            anomalies.push_back(a);
        }
    }

    return anomalies;
}

std::vector<Anomaly> AnomalyDetector::checkPositionAnomalies(
    const std::string& id,
    const LocationVector& current,
    const LocationVector& previous,
    double time_delta_s) {

    std::vector<Anomaly> anomalies;

    double distance = calculateDistance(
        previous.latitude, previous.longitude,
        current.latitude, current.longitude
    );

    // Check for impossible position jump
    double max_possible_distance = config_.max_horizontal_speed * time_delta_s;

    if (distance > config_.max_position_jump_m &&
        distance > max_possible_distance * 1.5) {

        Anomaly a;
        a.type = AnomalyType::POSITION_JUMP;
        a.severity = AnomalySeverity::CRITICAL;
        a.uav_id = id;
        a.description = "Position jumped impossibly far";
        a.expected_value = max_possible_distance;
        a.actual_value = distance;
        a.confidence = std::min(1.0, distance / (max_possible_distance * 3));
        a.detected_at = std::chrono::steady_clock::now();

        anomalies.push_back(a);
    }

    return anomalies;
}

std::vector<Anomaly> AnomalyDetector::checkReplayAttack(
    const std::string& id,
    uint32_t message_hash) {

    std::vector<Anomaly> anomalies;

    auto it = history_.find(id);
    if (it == history_.end()) {
        return anomalies;
    }

    auto& hist = it->second;
    auto now = std::chrono::steady_clock::now();

    // Count recent duplicates within replay window
    size_t duplicate_count = 0;
    for (size_t i = 0; i < hist.message_hashes.size(); i++) {
        if (hist.message_hashes[i] == message_hash) {
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - hist.timestamps[i]).count();

            if (age < config_.replay_window_ms) {
                duplicate_count++;
            }
        }
    }

    if (duplicate_count >= config_.min_duplicate_count) {
        Anomaly a;
        a.type = AnomalyType::REPLAY_ATTACK;
        a.severity = AnomalySeverity::CRITICAL;
        a.uav_id = id;
        a.description = "Duplicate messages detected (possible replay attack)";
        a.expected_value = 0;
        a.actual_value = static_cast<double>(duplicate_count);
        a.confidence = std::min(1.0, static_cast<double>(duplicate_count) / 10.0);
        a.detected_at = now;

        anomalies.push_back(a);
    }

    return anomalies;
}

std::vector<Anomaly> AnomalyDetector::checkSignalAnomaly(
    const std::string& id,
    int8_t current_rssi,
    const LocationVector& location) {

    std::vector<Anomaly> anomalies;

    auto it = history_.find(id);
    if (it == history_.end() || it->second.rssi_history.size() < 3) {
        return anomalies;
    }

    auto& hist = it->second;

    // Calculate average RSSI
    double avg_rssi = 0;
    for (int8_t r : hist.rssi_history) {
        avg_rssi += r;
    }
    avg_rssi /= hist.rssi_history.size();

    // Check for sudden RSSI change
    double rssi_diff = std::abs(current_rssi - avg_rssi);
    if (rssi_diff > config_.min_rssi_change) {
        // Check if position changed accordingly
        if (!hist.positions.empty()) {
            const auto& prev_pos = hist.positions.back();
            double distance = calculateDistance(
                prev_pos.latitude, prev_pos.longitude,
                location.latitude, location.longitude
            );

            // RSSI should correlate with distance change
            double expected_rssi_change = 10.0 * PATH_LOSS_EXP *
                                          std::log10(std::max(1.0, distance));

            if (rssi_diff > expected_rssi_change * (1.0 + config_.rssi_distance_tolerance)) {
                Anomaly a;
                a.type = AnomalyType::SIGNAL_ANOMALY;
                a.severity = AnomalySeverity::WARNING;
                a.uav_id = id;
                a.description = "RSSI change inconsistent with position change";
                a.expected_value = expected_rssi_change;
                a.actual_value = rssi_diff;
                a.confidence = std::min(1.0, rssi_diff / 40.0);
                a.detected_at = std::chrono::steady_clock::now();

                anomalies.push_back(a);
            }
        }
    }

    return anomalies;
}

size_t AnomalyDetector::getAnomalyCount(AnomalyType type) const {
    auto it = anomaly_counts_.find(type);
    return it != anomaly_counts_.end() ? it->second : 0;
}

void AnomalyDetector::clear() {
    history_.clear();
    anomaly_counts_.clear();
    total_anomalies_ = 0;
}

void AnomalyDetector::clearUAV(const std::string& id) {
    history_.erase(id);
}

double AnomalyDetector::calculateDistance(double lat1, double lon1,
                                           double lat2, double lon2) const {
    // Haversine formula
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;

    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(lat1_rad) * std::cos(lat2_rad) *
               std::sin(dlon / 2) * std::sin(dlon / 2);

    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return EARTH_RADIUS_M * c;
}

uint32_t AnomalyDetector::hashMessage(const UAVObject& uav) const {
    // Simple hash combining key fields
    std::hash<std::string> str_hash;
    std::hash<double> dbl_hash;
    std::hash<float> flt_hash;

    uint32_t h = static_cast<uint32_t>(str_hash(uav.id));
    h ^= static_cast<uint32_t>(dbl_hash(uav.location.latitude)) << 1;
    h ^= static_cast<uint32_t>(dbl_hash(uav.location.longitude)) << 2;
    h ^= static_cast<uint32_t>(flt_hash(uav.location.altitude_geo)) << 3;
    h ^= static_cast<uint32_t>(flt_hash(uav.location.speed_horizontal)) << 4;

    return h;
}

double AnomalyDetector::estimateDistanceFromRSSI(int8_t rssi) const {
    // Path loss model: RSSI = RSSI_ref - 10 * n * log10(d)
    // Solving for d: d = 10^((RSSI_ref - RSSI) / (10 * n))
    double exponent = (RSSI_REF - rssi) / (10.0 * PATH_LOSS_EXP);
    return std::pow(10.0, exponent);
}

} // namespace analysis
} // namespace orip

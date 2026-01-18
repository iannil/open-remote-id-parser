#include "orip/trajectory_analyzer.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace orip {
namespace analysis {

constexpr double EARTH_RADIUS_M = 6371000.0;
constexpr double DEG_TO_RAD = M_PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / M_PI;

void Trajectory::addPoint(const TrajectoryPoint& point, size_t max_size) {
    points.push_back(point);
    while (points.size() > max_size) {
        points.pop_front();
    }
}

void Trajectory::calculateStats() {
    if (points.empty()) {
        stats = TrajectoryStats{};
        return;
    }

    stats.point_count = points.size();
    stats.max_altitude_m = points.front().altitude;
    stats.min_altitude_m = points.front().altitude;
    stats.max_speed_mps = 0.0;
    stats.total_distance_m = 0.0;

    double speed_sum = 0.0;

    for (size_t i = 0; i < points.size(); i++) {
        const auto& p = points[i];

        stats.max_altitude_m = std::max(stats.max_altitude_m, p.altitude);
        stats.min_altitude_m = std::min(stats.min_altitude_m, p.altitude);
        stats.max_speed_mps = std::max(stats.max_speed_mps, static_cast<double>(p.speed));
        speed_sum += p.speed;

        if (i > 0) {
            const auto& prev = points[i - 1];
            stats.total_distance_m += TrajectoryAnalyzer::calculateDistance(
                prev.latitude, prev.longitude,
                p.latitude, p.longitude
            );
        }
    }

    stats.avg_speed_mps = points.empty() ? 0.0 : speed_sum / points.size();

    if (points.size() >= 2) {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            points.back().timestamp - points.front().timestamp
        );
        stats.duration = duration;
    }
}

TrajectoryAnalyzer::TrajectoryAnalyzer()
    : TrajectoryAnalyzer(TrajectoryConfig{}) {}

TrajectoryAnalyzer::TrajectoryAnalyzer(const TrajectoryConfig& config)
    : config_(config) {}

void TrajectoryAnalyzer::addPosition(const std::string& uav_id,
                                      const LocationVector& location) {
    if (!location.valid) {
        return;
    }

    auto& traj = trajectories_[uav_id];
    traj.uav_id = uav_id;

    TrajectoryPoint point(
        location.latitude,
        location.longitude,
        location.altitude_geo,
        location.speed_horizontal,
        location.direction
    );

    // Check minimum movement
    if (!traj.points.empty()) {
        const auto& last = traj.points.back();
        double dist = calculateDistance(
            last.latitude, last.longitude,
            point.latitude, point.longitude
        );

        if (dist < config_.min_movement_m) {
            // Update timestamp but don't add new point
            return;
        }
    }

    traj.addPoint(point, config_.max_history_points);

    // Apply smoothing
    if (traj.smoothed_points.empty()) {
        traj.smoothed_points.push_back(point);
    } else {
        auto smoothed = smoothPoint(point, traj.smoothed_points.back());
        traj.smoothed_points.push_back(smoothed);
        while (traj.smoothed_points.size() > config_.max_history_points) {
            traj.smoothed_points.pop_front();
        }
    }

    // Update stats periodically
    if (traj.points.size() % 10 == 0) {
        traj.calculateStats();
        traj.pattern = analyzePattern(traj);
    }
}

const Trajectory* TrajectoryAnalyzer::getTrajectory(const std::string& uav_id) const {
    auto it = trajectories_.find(uav_id);
    return it != trajectories_.end() ? &it->second : nullptr;
}

std::vector<std::string> TrajectoryAnalyzer::getActiveUAVs() const {
    std::vector<std::string> ids;
    ids.reserve(trajectories_.size());
    for (const auto& pair : trajectories_) {
        ids.push_back(pair.first);
    }
    return ids;
}

PredictedPosition TrajectoryAnalyzer::predictPosition(const std::string& uav_id,
                                                        uint32_t time_ahead_ms) const {
    PredictedPosition pred;
    pred.prediction_time = std::chrono::steady_clock::now() +
                           std::chrono::milliseconds(time_ahead_ms);

    auto it = trajectories_.find(uav_id);
    if (it == trajectories_.end() || it->second.points.size() < 2) {
        pred.confidence = 0.0;
        return pred;
    }

    const auto& traj = it->second;
    const auto& points = traj.smoothed_points.empty() ?
                         traj.points : traj.smoothed_points;

    if (points.size() < 2) {
        pred.confidence = 0.0;
        return pred;
    }

    // Use last two points to estimate velocity
    const auto& p1 = points[points.size() - 2];
    const auto& p2 = points.back();

    auto time_diff = std::chrono::duration<double>(
        p2.timestamp - p1.timestamp).count();

    if (time_diff <= 0) {
        pred.latitude = p2.latitude;
        pred.longitude = p2.longitude;
        pred.altitude = p2.altitude;
        pred.confidence = 0.5;
        return pred;
    }

    // Calculate velocity components
    double bearing = calculateBearing(p1.latitude, p1.longitude,
                                       p2.latitude, p2.longitude);
    double distance = calculateDistance(p1.latitude, p1.longitude,
                                         p2.latitude, p2.longitude);
    double speed_mps = distance / time_diff;
    double alt_rate = (p2.altitude - p1.altitude) / time_diff;

    // Project forward
    double prediction_time_s = time_ahead_ms / 1000.0;
    double predicted_distance = speed_mps * prediction_time_s;

    projectPosition(p2.latitude, p2.longitude,
                    bearing, predicted_distance,
                    pred.latitude, pred.longitude);

    pred.altitude = p2.altitude + static_cast<float>(alt_rate * prediction_time_s);

    // Confidence decreases with prediction time
    pred.confidence = std::max(0.0, 1.0 - (prediction_time_s / 30.0));

    // Error radius grows with time
    pred.error_radius_m = speed_mps * prediction_time_s * 0.1 +  // Speed uncertainty
                          prediction_time_s * 2.0;  // Base uncertainty

    return pred;
}

std::vector<TrajectoryPoint> TrajectoryAnalyzer::getSmoothedTrajectory(
    const std::string& uav_id) const {

    auto it = trajectories_.find(uav_id);
    if (it == trajectories_.end()) {
        return {};
    }

    return std::vector<TrajectoryPoint>(
        it->second.smoothed_points.begin(),
        it->second.smoothed_points.end()
    );
}

FlightPattern TrajectoryAnalyzer::classifyPattern(const std::string& uav_id) const {
    auto it = trajectories_.find(uav_id);
    if (it == trajectories_.end()) {
        return FlightPattern::UNKNOWN;
    }
    return it->second.pattern;
}

TrajectoryStats TrajectoryAnalyzer::getStats(const std::string& uav_id) const {
    auto it = trajectories_.find(uav_id);
    if (it == trajectories_.end()) {
        return TrajectoryStats{};
    }
    return it->second.stats;
}

double TrajectoryAnalyzer::calculateDistance(double lat1, double lon1,
                                              double lat2, double lon2) {
    double lat1_rad = lat1 * DEG_TO_RAD;
    double lat2_rad = lat2 * DEG_TO_RAD;
    double dlat = (lat2 - lat1) * DEG_TO_RAD;
    double dlon = (lon2 - lon1) * DEG_TO_RAD;

    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(lat1_rad) * std::cos(lat2_rad) *
               std::sin(dlon / 2) * std::sin(dlon / 2);

    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return EARTH_RADIUS_M * c;
}

double TrajectoryAnalyzer::calculateBearing(double lat1, double lon1,
                                             double lat2, double lon2) {
    double lat1_rad = lat1 * DEG_TO_RAD;
    double lat2_rad = lat2 * DEG_TO_RAD;
    double dlon = (lon2 - lon1) * DEG_TO_RAD;

    double x = std::sin(dlon) * std::cos(lat2_rad);
    double y = std::cos(lat1_rad) * std::sin(lat2_rad) -
               std::sin(lat1_rad) * std::cos(lat2_rad) * std::cos(dlon);

    double bearing = std::atan2(x, y) * RAD_TO_DEG;

    // Normalize to 0-360
    return std::fmod(bearing + 360.0, 360.0);
}

void TrajectoryAnalyzer::projectPosition(double lat, double lon,
                                          double bearing_deg, double distance_m,
                                          double& new_lat, double& new_lon) {
    double lat_rad = lat * DEG_TO_RAD;
    double lon_rad = lon * DEG_TO_RAD;
    double bearing_rad = bearing_deg * DEG_TO_RAD;
    double angular_dist = distance_m / EARTH_RADIUS_M;

    new_lat = std::asin(
        std::sin(lat_rad) * std::cos(angular_dist) +
        std::cos(lat_rad) * std::sin(angular_dist) * std::cos(bearing_rad)
    );

    new_lon = lon_rad + std::atan2(
        std::sin(bearing_rad) * std::sin(angular_dist) * std::cos(lat_rad),
        std::cos(angular_dist) - std::sin(lat_rad) * std::sin(new_lat)
    );

    new_lat *= RAD_TO_DEG;
    new_lon *= RAD_TO_DEG;
}

void TrajectoryAnalyzer::clear() {
    trajectories_.clear();
}

void TrajectoryAnalyzer::clearUAV(const std::string& uav_id) {
    trajectories_.erase(uav_id);
}

TrajectoryPoint TrajectoryAnalyzer::smoothPoint(const TrajectoryPoint& raw,
                                                  const TrajectoryPoint& prev_smooth) const {
    double alpha = config_.smoothing_factor;

    TrajectoryPoint smoothed;
    smoothed.latitude = alpha * raw.latitude + (1 - alpha) * prev_smooth.latitude;
    smoothed.longitude = alpha * raw.longitude + (1 - alpha) * prev_smooth.longitude;
    smoothed.altitude = static_cast<float>(
        alpha * raw.altitude + (1 - alpha) * prev_smooth.altitude);
    smoothed.speed = static_cast<float>(
        alpha * raw.speed + (1 - alpha) * prev_smooth.speed);
    smoothed.heading = static_cast<float>(
        alpha * raw.heading + (1 - alpha) * prev_smooth.heading);
    smoothed.timestamp = raw.timestamp;

    return smoothed;
}

FlightPattern TrajectoryAnalyzer::analyzePattern(const Trajectory& traj) const {
    if (traj.points.size() < 5) {
        return FlightPattern::UNKNOWN;
    }

    const auto& points = traj.points;

    // Calculate average speed
    double avg_speed = 0.0;
    for (const auto& p : points) {
        avg_speed += p.speed;
    }
    avg_speed /= points.size();

    // Check for stationary
    if (avg_speed < config_.stationary_speed_threshold) {
        return FlightPattern::STATIONARY;
    }

    // Calculate altitude trend
    float alt_start = points.front().altitude;
    float alt_end = points.back().altitude;
    float alt_diff = alt_end - alt_start;

    if (alt_diff < -10.0f && avg_speed < 5.0f) {
        return FlightPattern::LANDING;
    }

    if (alt_diff > 10.0f && avg_speed < 5.0f) {
        return FlightPattern::TAKEOFF;
    }

    // Calculate heading variance
    double heading_var = calculateHeadingVariance(points);

    // Low heading variance = linear flight
    if (heading_var < 15.0) {
        return FlightPattern::LINEAR;
    }

    // Check for circular pattern (consistent turning)
    bool consistent_turn = true;
    double total_turn = 0.0;
    for (size_t i = 1; i < points.size(); i++) {
        double heading_diff = points[i].heading - points[i-1].heading;
        // Normalize to -180 to 180
        while (heading_diff > 180) heading_diff -= 360;
        while (heading_diff < -180) heading_diff += 360;

        total_turn += heading_diff;
    }

    double avg_turn = total_turn / (points.size() - 1);
    if (std::abs(avg_turn) > 5.0 && heading_var < 30.0) {
        return FlightPattern::CIRCULAR;
    }

    // Check for patrol pattern (reversing direction)
    int direction_changes = 0;
    double prev_heading_diff = 0.0;
    for (size_t i = 2; i < points.size(); i++) {
        double h1 = points[i-1].heading - points[i-2].heading;
        double h2 = points[i].heading - points[i-1].heading;

        // Normalize
        while (h1 > 180) h1 -= 360;
        while (h1 < -180) h1 += 360;
        while (h2 > 180) h2 -= 360;
        while (h2 < -180) h2 += 360;

        if (std::abs(h2 - h1) > 90) {
            direction_changes++;
        }
    }

    if (direction_changes >= 2 && direction_changes <= points.size() / 5) {
        return FlightPattern::PATROL;
    }

    // High variance = erratic
    if (heading_var > 60.0) {
        return FlightPattern::ERRATIC;
    }

    return FlightPattern::UNKNOWN;
}

double TrajectoryAnalyzer::calculateHeadingVariance(
    const std::deque<TrajectoryPoint>& points) const {

    if (points.size() < 2) {
        return 0.0;
    }

    std::vector<double> headings;
    headings.reserve(points.size());
    for (const auto& p : points) {
        headings.push_back(p.heading);
    }

    // Calculate mean (circular mean for angles)
    double sin_sum = 0.0, cos_sum = 0.0;
    for (double h : headings) {
        sin_sum += std::sin(h * DEG_TO_RAD);
        cos_sum += std::cos(h * DEG_TO_RAD);
    }
    double mean = std::atan2(sin_sum, cos_sum) * RAD_TO_DEG;

    // Calculate variance
    double var_sum = 0.0;
    for (double h : headings) {
        double diff = h - mean;
        while (diff > 180) diff -= 360;
        while (diff < -180) diff += 360;
        var_sum += diff * diff;
    }

    return std::sqrt(var_sum / headings.size());
}

} // namespace analysis
} // namespace orip

#ifndef ORIP_TRAJECTORY_ANALYZER_H
#define ORIP_TRAJECTORY_ANALYZER_H

#include "orip/types.h"
#include <deque>
#include <unordered_map>
#include <chrono>
#include <vector>

namespace orip {
namespace analysis {

// Single trajectory point
struct TrajectoryPoint {
    double latitude = 0.0;
    double longitude = 0.0;
    float altitude = 0.0f;
    float speed = 0.0f;
    float heading = 0.0f;
    std::chrono::steady_clock::time_point timestamp;

    TrajectoryPoint() : timestamp(std::chrono::steady_clock::now()) {}

    TrajectoryPoint(double lat, double lon, float alt, float spd, float hdg)
        : latitude(lat), longitude(lon), altitude(alt),
          speed(spd), heading(hdg),
          timestamp(std::chrono::steady_clock::now()) {}
};

// Predicted position
struct PredictedPosition {
    double latitude = 0.0;
    double longitude = 0.0;
    float altitude = 0.0f;
    double confidence = 0.0;  // 0.0 - 1.0
    double error_radius_m = 0.0;  // Estimated error radius
    std::chrono::steady_clock::time_point prediction_time;
};

// Trajectory statistics
struct TrajectoryStats {
    double total_distance_m = 0.0;
    double max_speed_mps = 0.0;
    double avg_speed_mps = 0.0;
    float max_altitude_m = 0.0f;
    float min_altitude_m = 0.0f;
    double heading_variance = 0.0;  // Indicates turning behavior
    std::chrono::seconds duration{0};
    size_t point_count = 0;
};

// Flight pattern classification
enum class FlightPattern : uint8_t {
    UNKNOWN = 0,
    STATIONARY = 1,      // Hovering in place
    LINEAR = 2,          // Straight line flight
    CIRCULAR = 3,        // Circular pattern
    PATROL = 4,          // Back-and-forth pattern
    ERRATIC = 5,         // No discernible pattern
    LANDING = 6,         // Descending pattern
    TAKEOFF = 7          // Ascending pattern
};

// Configuration
struct TrajectoryConfig {
    size_t max_history_points = 1000;
    double smoothing_factor = 0.3;           // For exponential smoothing
    uint32_t prediction_horizon_ms = 5000;   // How far to predict
    double min_movement_m = 1.0;             // Minimum movement to record
    float stationary_speed_threshold = 0.5f; // m/s
};

// Complete trajectory for a UAV
struct Trajectory {
    std::string uav_id;
    std::deque<TrajectoryPoint> points;
    std::deque<TrajectoryPoint> smoothed_points;
    TrajectoryStats stats;
    FlightPattern pattern = FlightPattern::UNKNOWN;

    void addPoint(const TrajectoryPoint& point, size_t max_size);
    void calculateStats();
};

/**
 * Trajectory Analyzer
 *
 * Provides trajectory analysis capabilities:
 * - Historical trajectory storage
 * - Trajectory smoothing (Kalman-like filter)
 * - Position prediction
 * - Flight pattern classification
 * - Statistics calculation
 */
class TrajectoryAnalyzer {
public:
    TrajectoryAnalyzer();
    explicit TrajectoryAnalyzer(const TrajectoryConfig& config);

    /**
     * Add a new position for a UAV
     */
    void addPosition(const std::string& uav_id, const LocationVector& location);

    /**
     * Get trajectory for a UAV
     */
    const Trajectory* getTrajectory(const std::string& uav_id) const;

    /**
     * Get all active trajectories
     */
    std::vector<std::string> getActiveUAVs() const;

    /**
     * Predict future position
     * @param uav_id UAV identifier
     * @param time_ahead_ms How far ahead to predict (ms)
     */
    PredictedPosition predictPosition(const std::string& uav_id,
                                       uint32_t time_ahead_ms) const;

    /**
     * Get smoothed trajectory points
     */
    std::vector<TrajectoryPoint> getSmoothedTrajectory(const std::string& uav_id) const;

    /**
     * Classify flight pattern
     */
    FlightPattern classifyPattern(const std::string& uav_id) const;

    /**
     * Get trajectory statistics
     */
    TrajectoryStats getStats(const std::string& uav_id) const;

    /**
     * Calculate distance between two points (Haversine)
     */
    static double calculateDistance(double lat1, double lon1,
                                     double lat2, double lon2);

    /**
     * Calculate bearing between two points
     */
    static double calculateBearing(double lat1, double lon1,
                                    double lat2, double lon2);

    /**
     * Project position given start point, bearing, and distance
     */
    static void projectPosition(double lat, double lon,
                                 double bearing_deg, double distance_m,
                                 double& new_lat, double& new_lon);

    /**
     * Clear all trajectories
     */
    void clear();

    /**
     * Clear trajectory for specific UAV
     */
    void clearUAV(const std::string& uav_id);

    /**
     * Get configuration
     */
    const TrajectoryConfig& config() const { return config_; }

private:
    TrajectoryConfig config_;
    std::unordered_map<std::string, Trajectory> trajectories_;

    // Apply smoothing to new point
    TrajectoryPoint smoothPoint(const TrajectoryPoint& raw,
                                 const TrajectoryPoint& prev_smooth) const;

    // Analyze pattern from trajectory
    FlightPattern analyzePattern(const Trajectory& traj) const;

    // Calculate heading variance
    double calculateHeadingVariance(const std::deque<TrajectoryPoint>& points) const;
};

} // namespace analysis
} // namespace orip

#endif // ORIP_TRAJECTORY_ANALYZER_H

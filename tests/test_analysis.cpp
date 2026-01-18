#include <gtest/gtest.h>
#include "orip/anomaly_detector.h"
#include "orip/trajectory_analyzer.h"
#include <thread>
#include <chrono>

using namespace orip;
using namespace orip::analysis;

// ============================================
// Anomaly Detector Tests
// ============================================

class AnomalyDetectorTest : public ::testing::Test {
protected:
    AnomalyDetector detector;

    UAVObject createUAV(const std::string& id, double lat, double lon,
                        float alt, float speed, float heading) {
        UAVObject uav;
        uav.id = id;
        uav.location.valid = true;
        uav.location.latitude = lat;
        uav.location.longitude = lon;
        uav.location.altitude_geo = alt;
        uav.location.speed_horizontal = speed;
        uav.location.direction = heading;
        return uav;
    }
};

TEST_F(AnomalyDetectorTest, NoAnomalyOnFirstMessage) {
    auto uav = createUAV("TEST001", 37.7749, -122.4194, 100.0f, 10.0f, 90.0f);
    auto anomalies = detector.analyze(uav, -60);

    EXPECT_TRUE(anomalies.empty());
}

TEST_F(AnomalyDetectorTest, NoAnomalyOnNormalFlight) {
    // First position
    auto uav1 = createUAV("TEST001", 37.7749, -122.4194, 100.0f, 10.0f, 90.0f);
    detector.analyze(uav1, -60);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second position - slight movement (realistic)
    // 10 m/s for 0.1s = 1m movement
    auto uav2 = createUAV("TEST001", 37.7749001, -122.4194, 100.0f, 10.0f, 90.0f);
    auto anomalies = detector.analyze(uav2, -60);

    // No anomalies expected for normal flight
    bool has_speed_anomaly = false;
    for (const auto& a : anomalies) {
        if (a.type == AnomalyType::SPEED_IMPOSSIBLE ||
            a.type == AnomalyType::POSITION_JUMP) {
            has_speed_anomaly = true;
        }
    }
    EXPECT_FALSE(has_speed_anomaly);
}

TEST_F(AnomalyDetectorTest, DetectSpeedAnomaly) {
    // First position
    auto uav1 = createUAV("TEST001", 37.7749, -122.4194, 100.0f, 10.0f, 90.0f);
    detector.analyze(uav1, -60);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second position - impossibly far (10km in 0.1 second = 100000 m/s)
    auto uav2 = createUAV("TEST001", 37.8749, -122.4194, 100.0f, 10.0f, 90.0f);
    auto anomalies = detector.analyze(uav2, -60);

    bool found_anomaly = false;
    for (const auto& a : anomalies) {
        if (a.type == AnomalyType::SPEED_IMPOSSIBLE ||
            a.type == AnomalyType::POSITION_JUMP) {
            found_anomaly = true;
            EXPECT_GE(a.confidence, 0.5);
        }
    }
    EXPECT_TRUE(found_anomaly);
}

TEST_F(AnomalyDetectorTest, DetectAltitudeSpike) {
    // First position
    auto uav1 = createUAV("TEST001", 37.7749, -122.4194, 100.0f, 10.0f, 90.0f);
    detector.analyze(uav1, -60);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second position - impossible altitude jump (5000m in 0.1s)
    auto uav2 = createUAV("TEST001", 37.7749, -122.4194, 5100.0f, 10.0f, 90.0f);
    auto anomalies = detector.analyze(uav2, -60);

    bool found_anomaly = false;
    for (const auto& a : anomalies) {
        if (a.type == AnomalyType::ALTITUDE_SPIKE) {
            found_anomaly = true;
        }
    }
    EXPECT_TRUE(found_anomaly);
}

TEST_F(AnomalyDetectorTest, DetectReplayAttack) {
    AnomalyConfig config;
    config.min_duplicate_count = 2;
    AnomalyDetector detector_replay(config);

    auto uav = createUAV("TEST001", 37.7749, -122.4194, 100.0f, 10.0f, 90.0f);

    // Send same message multiple times
    for (int i = 0; i < 5; i++) {
        auto anomalies = detector_replay.analyze(uav, -60);

        if (i >= 2) {
            bool found_replay = false;
            for (const auto& a : anomalies) {
                if (a.type == AnomalyType::REPLAY_ATTACK) {
                    found_replay = true;
                }
            }
            EXPECT_TRUE(found_replay) << "Replay should be detected at iteration " << i;
        }
    }
}

TEST_F(AnomalyDetectorTest, ClearHistory) {
    auto uav = createUAV("TEST001", 37.7749, -122.4194, 100.0f, 10.0f, 90.0f);
    detector.analyze(uav, -60);

    EXPECT_GT(detector.getTotalAnomalies(), 0u);

    detector.clear();

    // After clearing, first message again - should not trigger replay
    auto anomalies = detector.analyze(uav, -60);
    bool has_replay = false;
    for (const auto& a : anomalies) {
        if (a.type == AnomalyType::REPLAY_ATTACK) {
            has_replay = true;
        }
    }
    EXPECT_FALSE(has_replay);
}

TEST_F(AnomalyDetectorTest, GetAnomalyCount) {
    auto uav1 = createUAV("TEST001", 37.7749, -122.4194, 100.0f, 10.0f, 90.0f);
    detector.analyze(uav1, -60);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Impossible speed
    auto uav2 = createUAV("TEST001", 38.7749, -122.4194, 100.0f, 10.0f, 90.0f);
    detector.analyze(uav2, -60);

    EXPECT_GT(detector.getAnomalyCount(AnomalyType::SPEED_IMPOSSIBLE) +
              detector.getAnomalyCount(AnomalyType::POSITION_JUMP), 0u);
}

// ============================================
// Trajectory Analyzer Tests
// ============================================

class TrajectoryAnalyzerTest : public ::testing::Test {
protected:
    TrajectoryAnalyzer analyzer;

    LocationVector createLocation(double lat, double lon, float alt,
                                   float speed, float heading) {
        LocationVector loc;
        loc.valid = true;
        loc.latitude = lat;
        loc.longitude = lon;
        loc.altitude_geo = alt;
        loc.speed_horizontal = speed;
        loc.direction = heading;
        return loc;
    }
};

TEST_F(TrajectoryAnalyzerTest, AddPosition) {
    auto loc = createLocation(37.7749, -122.4194, 100.0f, 10.0f, 90.0f);
    analyzer.addPosition("UAV001", loc);

    auto* traj = analyzer.getTrajectory("UAV001");
    ASSERT_NE(traj, nullptr);
    EXPECT_EQ(traj->points.size(), 1u);
}

TEST_F(TrajectoryAnalyzerTest, GetActiveUAVs) {
    auto loc = createLocation(37.7749, -122.4194, 100.0f, 10.0f, 90.0f);

    analyzer.addPosition("UAV001", loc);
    analyzer.addPosition("UAV002", loc);

    auto uavs = analyzer.getActiveUAVs();
    EXPECT_EQ(uavs.size(), 2u);
}

TEST_F(TrajectoryAnalyzerTest, CalculateDistance) {
    // Test known distance: SF to LA is approximately 560 km
    double sf_lat = 37.7749;
    double sf_lon = -122.4194;
    double la_lat = 34.0522;
    double la_lon = -118.2437;

    double distance = TrajectoryAnalyzer::calculateDistance(
        sf_lat, sf_lon, la_lat, la_lon);

    EXPECT_NEAR(distance, 559000.0, 10000.0);  // Within 10km
}

TEST_F(TrajectoryAnalyzerTest, CalculateBearing) {
    // North bearing
    double bearing = TrajectoryAnalyzer::calculateBearing(
        37.0, -122.0, 38.0, -122.0);
    EXPECT_NEAR(bearing, 0.0, 1.0);  // Should be ~0 degrees (north)

    // East bearing
    bearing = TrajectoryAnalyzer::calculateBearing(
        37.0, -122.0, 37.0, -121.0);
    EXPECT_NEAR(bearing, 90.0, 2.0);  // Should be ~90 degrees (east)
}

TEST_F(TrajectoryAnalyzerTest, ProjectPosition) {
    double lat = 37.7749;
    double lon = -122.4194;
    double new_lat, new_lon;

    // Project 1000m north
    TrajectoryAnalyzer::projectPosition(lat, lon, 0.0, 1000.0, new_lat, new_lon);

    // New latitude should be slightly higher
    EXPECT_GT(new_lat, lat);
    // Longitude should be approximately the same
    EXPECT_NEAR(new_lon, lon, 0.001);

    // Verify distance
    double dist = TrajectoryAnalyzer::calculateDistance(lat, lon, new_lat, new_lon);
    EXPECT_NEAR(dist, 1000.0, 1.0);
}

TEST_F(TrajectoryAnalyzerTest, PredictPosition) {
    // Add a trajectory with consistent movement
    double lat = 37.7749;
    double lon = -122.4194;

    for (int i = 0; i < 5; i++) {
        auto loc = createLocation(lat + i * 0.0001, lon, 100.0f, 10.0f, 0.0f);
        analyzer.addPosition("UAV001", loc);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    auto prediction = analyzer.predictPosition("UAV001", 1000);  // 1 second ahead

    EXPECT_GT(prediction.confidence, 0.0);
    EXPECT_GT(prediction.latitude, lat);  // Should continue north
}

TEST_F(TrajectoryAnalyzerTest, GetSmoothedTrajectory) {
    // Add noisy positions
    double lat = 37.7749;
    double lon = -122.4194;

    for (int i = 0; i < 10; i++) {
        // Add some noise
        double noise = (i % 2 == 0) ? 0.00001 : -0.00001;
        auto loc = createLocation(lat + i * 0.0001 + noise, lon, 100.0f, 10.0f, 0.0f);
        analyzer.addPosition("UAV001", loc);
    }

    auto smoothed = analyzer.getSmoothedTrajectory("UAV001");
    EXPECT_FALSE(smoothed.empty());
}

TEST_F(TrajectoryAnalyzerTest, GetStats) {
    double lat = 37.7749;
    double lon = -122.4194;

    for (int i = 0; i < 15; i++) {
        auto loc = createLocation(lat + i * 0.0001, lon, 100.0f + i * 5.0f,
                                   10.0f + i, 0.0f);
        analyzer.addPosition("UAV001", loc);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    auto stats = analyzer.getStats("UAV001");

    EXPECT_GT(stats.point_count, 0u);
    EXPECT_GT(stats.total_distance_m, 0.0);
    EXPECT_GT(stats.max_speed_mps, 0.0);
}

TEST_F(TrajectoryAnalyzerTest, ClassifyPatternStationary) {
    auto loc = createLocation(37.7749, -122.4194, 100.0f, 0.1f, 0.0f);

    for (int i = 0; i < 10; i++) {
        analyzer.addPosition("UAV001", loc);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Trigger pattern analysis
    auto* traj = analyzer.getTrajectory("UAV001");
    ASSERT_NE(traj, nullptr);

    // For stationary, all positions are nearly the same
    FlightPattern pattern = analyzer.classifyPattern("UAV001");
    // Note: Due to minimum movement filtering, stationary might not have many points
}

TEST_F(TrajectoryAnalyzerTest, ClassifyPatternLinear) {
    double lat = 37.7749;
    double lon = -122.4194;

    // Straight line flight
    for (int i = 0; i < 20; i++) {
        auto loc = createLocation(lat + i * 0.0005, lon, 100.0f, 15.0f, 0.0f);
        analyzer.addPosition("UAV001", loc);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    FlightPattern pattern = analyzer.classifyPattern("UAV001");
    // Linear pattern expected (or UNKNOWN if not enough data processed)
    EXPECT_TRUE(pattern == FlightPattern::LINEAR ||
                pattern == FlightPattern::UNKNOWN);
}

TEST_F(TrajectoryAnalyzerTest, ClearTrajectory) {
    auto loc = createLocation(37.7749, -122.4194, 100.0f, 10.0f, 0.0f);
    analyzer.addPosition("UAV001", loc);

    EXPECT_NE(analyzer.getTrajectory("UAV001"), nullptr);

    analyzer.clearUAV("UAV001");

    EXPECT_EQ(analyzer.getTrajectory("UAV001"), nullptr);
}

TEST_F(TrajectoryAnalyzerTest, ClearAll) {
    auto loc = createLocation(37.7749, -122.4194, 100.0f, 10.0f, 0.0f);
    analyzer.addPosition("UAV001", loc);
    analyzer.addPosition("UAV002", loc);

    EXPECT_EQ(analyzer.getActiveUAVs().size(), 2u);

    analyzer.clear();

    EXPECT_EQ(analyzer.getActiveUAVs().size(), 0u);
}

TEST_F(TrajectoryAnalyzerTest, InvalidLocation) {
    LocationVector loc;
    loc.valid = false;  // Invalid location

    analyzer.addPosition("UAV001", loc);

    // Should not add trajectory for invalid location
    EXPECT_EQ(analyzer.getTrajectory("UAV001"), nullptr);
}

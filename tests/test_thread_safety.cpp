#include <gtest/gtest.h>
#include "orip/parser.h"
#include "orip/session_manager.h"
#include "orip/anomaly_detector.h"
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <set>
#include <mutex>

using namespace orip;
using namespace orip::analysis;

// =============================================================================
// Thread Safety Test Fixture
// =============================================================================

class ThreadSafetyTest : public ::testing::Test {
protected:
    // Create a mock BLE advertisement with Basic ID
    std::vector<uint8_t> createBasicIDAdv(const std::string& uav_id) {
        std::vector<uint8_t> adv;

        // AD length
        adv.push_back(0x1E);  // 30 bytes
        adv.push_back(0x16);  // Service Data
        adv.push_back(0xFA);  // UUID low (0xFFFA)
        adv.push_back(0xFF);  // UUID high
        adv.push_back(0x00);  // Counter

        // Basic ID message (25 bytes)
        std::vector<uint8_t> msg(25, 0);
        msg[0] = 0x02;  // Message type 0 | version 2
        msg[1] = 0x12;  // Serial number type | Multirotor

        // Copy UAV ID
        for (size_t i = 0; i < uav_id.size() && i < 20; i++) {
            msg[2 + i] = static_cast<uint8_t>(uav_id[i]);
        }

        adv.insert(adv.end(), msg.begin(), msg.end());
        return adv;
    }

    // Create a location message for anomaly detection
    LocationVector createLocation(double lat, double lon, float alt, float speed) {
        LocationVector loc;
        loc.valid = true;
        loc.latitude = lat;
        loc.longitude = lon;
        loc.altitude_geo = alt;
        loc.altitude_baro = alt;
        loc.speed_horizontal = speed;
        loc.speed_vertical = 0;
        loc.direction = 0;
        return loc;
    }
};

// =============================================================================
// SessionManager Concurrent Access Tests
// =============================================================================

TEST_F(ThreadSafetyTest, SessionManager_ConcurrentUpdates) {
    // Note: SessionManager is NOT currently thread-safe
    // This test documents expected behavior and may detect races with TSan

    SessionManager manager(30000);
    std::atomic<int> update_count{0};
    std::atomic<int> new_uav_count{0};

    const int num_threads = 4;
    const int updates_per_thread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < updates_per_thread; i++) {
                UAVObject uav;
                uav.id = "UAV-" + std::to_string(t) + "-" + std::to_string(i % 10);
                uav.rssi = -60 - (i % 40);
                uav.last_seen = std::chrono::steady_clock::now();
                uav.location = createLocation(37.0 + t * 0.1, -122.0 + i * 0.001, 100.0f, 5.0f);

                bool is_new = manager.update(uav);
                if (is_new) {
                    new_uav_count++;
                }
                update_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(update_count.load(), num_threads * updates_per_thread);
    // Expect at most num_threads * 10 unique UAVs (10 unique IDs per thread)
    EXPECT_LE(manager.count(), static_cast<size_t>(num_threads * 10));
}

TEST_F(ThreadSafetyTest, SessionManager_ConcurrentReads) {
    SessionManager manager(30000);

    // Pre-populate with some UAVs
    for (int i = 0; i < 50; i++) {
        UAVObject uav;
        uav.id = "PRESET-" + std::to_string(i);
        uav.rssi = -70;
        uav.last_seen = std::chrono::steady_clock::now();
        manager.update(uav);
    }

    std::atomic<int> read_count{0};
    const int num_threads = 4;
    const int reads_per_thread = 200;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < reads_per_thread; i++) {
                // Mix of different read operations
                if (i % 3 == 0) {
                    auto uavs = manager.getActiveUAVs();
                    EXPECT_GE(uavs.size(), 0u);
                } else if (i % 3 == 1) {
                    auto* uav = manager.getUAV("PRESET-" + std::to_string(i % 50));
                    // May or may not find it
                    (void)uav;
                } else {
                    auto count = manager.count();
                    EXPECT_GE(count, 0u);
                }
                read_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(read_count.load(), num_threads * reads_per_thread);
}

TEST_F(ThreadSafetyTest, SessionManager_ConcurrentReadWrite) {
    SessionManager manager(30000);

    std::atomic<bool> running{true};
    std::atomic<int> write_count{0};
    std::atomic<int> read_count{0};

    // Writer threads
    std::vector<std::thread> writers;
    for (int w = 0; w < 2; w++) {
        writers.emplace_back([&, w]() {
            int i = 0;
            while (running.load()) {
                UAVObject uav;
                uav.id = "WRITER-" + std::to_string(w) + "-" + std::to_string(i % 20);
                uav.rssi = -50 - (i % 50);
                uav.last_seen = std::chrono::steady_clock::now();

                manager.update(uav);
                write_count++;
                i++;

                std::this_thread::yield();
            }
        });
    }

    // Reader threads
    std::vector<std::thread> readers;
    for (int r = 0; r < 2; r++) {
        readers.emplace_back([&]() {
            while (running.load()) {
                auto uavs = manager.getActiveUAVs();
                for (const auto& u : uavs) {
                    EXPECT_FALSE(u.id.empty());
                }
                read_count++;

                std::this_thread::yield();
            }
        });
    }

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running.store(false);

    for (auto& t : writers) t.join();
    for (auto& t : readers) t.join();

    EXPECT_GT(write_count.load(), 0);
    EXPECT_GT(read_count.load(), 0);
}

TEST_F(ThreadSafetyTest, SessionManager_CleanupDuringUpdates) {
    SessionManager manager(10);  // Very short timeout (10ms)

    std::atomic<bool> running{true};
    std::atomic<int> cleanup_count{0};

    // Update thread
    std::thread updater([&]() {
        int i = 0;
        while (running.load()) {
            UAVObject uav;
            uav.id = "CLEANUP-TEST-" + std::to_string(i % 10);
            uav.rssi = -60;
            uav.last_seen = std::chrono::steady_clock::now();

            manager.update(uav);
            i++;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Cleanup thread
    std::thread cleaner([&]() {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            auto removed = manager.cleanup();
            cleanup_count += removed.size();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running.store(false);

    updater.join();
    cleaner.join();

    // We should have done some cleanups
    // (Not asserting exact numbers due to timing variability)
    SUCCEED();
}

// =============================================================================
// AnomalyDetector Concurrent Access Tests
// =============================================================================

TEST_F(ThreadSafetyTest, AnomalyDetector_ConcurrentAnalysis) {
    AnomalyConfig config;
    AnomalyDetector detector(config);

    std::atomic<int> analysis_count{0};
    const int num_threads = 4;
    const int analyses_per_thread = 50;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> lat_dist(37.0, 38.0);
            std::uniform_real_distribution<> lon_dist(-123.0, -122.0);

            for (int i = 0; i < analyses_per_thread; i++) {
                UAVObject uav;
                uav.id = "ANOMALY-" + std::to_string(t);  // Same ID per thread
                uav.location = createLocation(
                    lat_dist(gen), lon_dist(gen),
                    100.0f + i * 0.5f, 5.0f + i * 0.1f
                );
                uav.last_seen = std::chrono::steady_clock::now();

                auto anomalies = detector.analyze(uav, -60);
                // May or may not detect anomalies
                (void)anomalies.size();

                analysis_count++;

                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(analysis_count.load(), num_threads * analyses_per_thread);

    // Verify we can still query
    auto total = detector.getTotalAnomalyCount();
    EXPECT_GE(total, 0u);
}

TEST_F(ThreadSafetyTest, AnomalyDetector_ConcurrentClear) {
    AnomalyConfig config;
    AnomalyDetector detector(config);

    std::atomic<bool> running{true};

    // Analysis thread
    std::thread analyzer([&]() {
        int i = 0;
        while (running.load()) {
            UAVObject uav;
            uav.id = "CLEAR-TEST-" + std::to_string(i % 5);
            uav.location = createLocation(37.0 + i * 0.001, -122.0, 100.0f, 10.0f);
            uav.last_seen = std::chrono::steady_clock::now();

            detector.analyze(uav, -70);
            i++;

            std::this_thread::yield();
        }
    });

    // Clear thread
    std::thread clearer([&]() {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            detector.clear();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running.store(false);

    analyzer.join();
    clearer.join();

    SUCCEED();
}

// =============================================================================
// RemoteIDParser Concurrent Access Tests
// =============================================================================

TEST_F(ThreadSafetyTest, Parser_ConcurrentParse) {
    ParserConfig config;
    config.enable_deduplication = true;
    RemoteIDParser parser(config);
    parser.init();

    std::atomic<int> parse_count{0};
    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int parses_per_thread = 50;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < parses_per_thread; i++) {
                std::string uav_id = "PARSE-" + std::to_string(t) + "-" + std::to_string(i % 5);
                auto adv = createBasicIDAdv(uav_id);

                auto result = parser.parse(adv, -60 - (i % 30), TransportType::BT_LEGACY);
                if (result.success) {
                    success_count++;
                }
                parse_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(parse_count.load(), num_threads * parses_per_thread);
    EXPECT_GT(success_count.load(), 0);
}

TEST_F(ThreadSafetyTest, Parser_ConcurrentGetActiveUAVs) {
    ParserConfig config;
    RemoteIDParser parser(config);
    parser.init();

    // Pre-parse some UAVs
    for (int i = 0; i < 20; i++) {
        auto adv = createBasicIDAdv("INITIAL-" + std::to_string(i));
        parser.parse(adv, -50, TransportType::BT_LEGACY);
    }

    std::atomic<bool> running{true};
    std::atomic<int> get_count{0};

    // Parse thread
    std::thread parser_thread([&]() {
        int i = 0;
        while (running.load()) {
            auto adv = createBasicIDAdv("DYNAMIC-" + std::to_string(i % 10));
            parser.parse(adv, -60, TransportType::BT_LEGACY);
            i++;
            std::this_thread::yield();
        }
    });

    // GetActiveUAVs threads
    std::vector<std::thread> getters;
    for (int g = 0; g < 2; g++) {
        getters.emplace_back([&]() {
            while (running.load()) {
                auto uavs = parser.getActiveUAVs();
                for (const auto& u : uavs) {
                    EXPECT_FALSE(u.id.empty());
                }
                get_count++;
                std::this_thread::yield();
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running.store(false);

    parser_thread.join();
    for (auto& t : getters) t.join();

    EXPECT_GT(get_count.load(), 0);
}

// =============================================================================
// Stress Tests
// =============================================================================

TEST_F(ThreadSafetyTest, StressTest_HighConcurrency) {
    ParserConfig config;
    RemoteIDParser parser(config);
    parser.init();

    const int num_threads = 8;
    const int ops_per_thread = 100;
    std::atomic<int> total_ops{0};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> op_dist(0, 2);

            for (int i = 0; i < ops_per_thread; i++) {
                int op = op_dist(gen);

                switch (op) {
                    case 0: {
                        // Parse
                        auto adv = createBasicIDAdv("STRESS-" + std::to_string(t) + "-" + std::to_string(i % 5));
                        parser.parse(adv, -50 - (i % 50), TransportType::BT_LEGACY);
                        break;
                    }
                    case 1: {
                        // Get active UAVs
                        auto uavs = parser.getActiveUAVs();
                        (void)uavs.size();
                        break;
                    }
                    case 2: {
                        // Get specific UAV
                        auto* uav = parser.getUAV("STRESS-" + std::to_string(t) + "-0");
                        (void)uav;
                        break;
                    }
                }

                total_ops++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_ops.load(), num_threads * ops_per_thread);
}

// =============================================================================
// Callback Thread Safety Tests
// =============================================================================

TEST_F(ThreadSafetyTest, Callbacks_ConcurrentInvocation) {
    SessionManager manager(30000);

    std::atomic<int> new_callback_count{0};
    std::atomic<int> update_callback_count{0};

    manager.setOnNewUAV([&](const UAVObject& uav) {
        EXPECT_FALSE(uav.id.empty());
        new_callback_count++;
    });

    manager.setOnUAVUpdate([&](const UAVObject& uav) {
        EXPECT_FALSE(uav.id.empty());
        update_callback_count++;
    });

    const int num_threads = 4;
    const int updates_per_thread = 50;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < updates_per_thread; i++) {
                UAVObject uav;
                // Use same ID to trigger updates after first creation
                uav.id = "CALLBACK-" + std::to_string(t);
                uav.rssi = -60 - (i % 40);
                uav.last_seen = std::chrono::steady_clock::now();

                manager.update(uav);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Each thread should create 1 new UAV, rest are updates
    EXPECT_EQ(new_callback_count.load(), num_threads);
    EXPECT_EQ(update_callback_count.load(), num_threads * (updates_per_thread - 1));
}

// =============================================================================
// Additional SessionManager Concurrent Tests (P2-TEST-012)
// =============================================================================

TEST_F(ThreadSafetyTest, SessionManager_ConcurrentAddAndClear) {
    // Test race condition between adding UAVs and clearing all
    SessionManager manager(30000);

    std::atomic<bool> running{true};
    std::atomic<int> add_count{0};
    std::atomic<int> clear_count{0};

    // Multiple threads adding UAVs
    std::vector<std::thread> adders;
    for (int t = 0; t < 3; t++) {
        adders.emplace_back([&, t]() {
            int i = 0;
            while (running.load()) {
                UAVObject uav;
                uav.id = "ADD-" + std::to_string(t) + "-" + std::to_string(i % 50);
                uav.rssi = -60;
                uav.last_seen = std::chrono::steady_clock::now();
                uav.location = createLocation(37.0, -122.0, 100.0f, 5.0f);

                manager.update(uav);
                add_count++;
                i++;
                std::this_thread::yield();
            }
        });
    }

    // Thread that periodically clears all UAVs
    std::thread clearer([&]() {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            manager.clear();
            clear_count++;
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running.store(false);

    for (auto& t : adders) t.join();
    clearer.join();

    EXPECT_GT(add_count.load(), 0);
    EXPECT_GT(clear_count.load(), 0);
    // No crash = success
    SUCCEED();
}

TEST_F(ThreadSafetyTest, SessionManager_ConcurrentAddAndRemoveSpecific) {
    // Test race between adding UAVs and removing specific ones
    SessionManager manager(30000);

    std::atomic<bool> running{true};
    std::atomic<int> add_count{0};
    std::atomic<int> remove_count{0};

    // Adding thread
    std::thread adder([&]() {
        int i = 0;
        while (running.load()) {
            UAVObject uav;
            uav.id = "TARGET-" + std::to_string(i % 10);  // Only 10 unique IDs
            uav.rssi = -50 - (i % 30);
            uav.last_seen = std::chrono::steady_clock::now();

            manager.update(uav);
            add_count++;
            i++;
            std::this_thread::yield();
        }
    });

    // Removing thread - removes by ID
    std::thread remover([&]() {
        int i = 0;
        while (running.load()) {
            std::string id = "TARGET-" + std::to_string(i % 10);
            manager.remove(id);
            remove_count++;
            i++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running.store(false);

    adder.join();
    remover.join();

    EXPECT_GT(add_count.load(), 0);
    EXPECT_GT(remove_count.load(), 0);
    SUCCEED();
}

TEST_F(ThreadSafetyTest, SessionManager_LargeUAVStress) {
    // Test with 100+ concurrent UAVs
    SessionManager manager(60000);  // Long timeout

    const int num_threads = 4;
    const int uavs_per_thread = 50;  // Total 200 UAVs

    std::atomic<int> update_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < uavs_per_thread; i++) {
                UAVObject uav;
                uav.id = "LARGE-" + std::to_string(t * 1000 + i);  // Unique IDs
                uav.rssi = -50 - (i % 50);
                uav.last_seen = std::chrono::steady_clock::now();
                uav.location = createLocation(
                    37.0 + t * 0.01 + i * 0.0001,
                    -122.0 + i * 0.0001,
                    100.0f + i,
                    5.0f + (i % 10)
                );
                uav.id_type = UAVIdType::SERIAL_NUMBER;
                uav.uav_type = UAVType::HELICOPTER_OR_MULTIROTOR;

                manager.update(uav);
                update_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(update_count.load(), num_threads * uavs_per_thread);
    EXPECT_EQ(manager.count(), static_cast<size_t>(num_threads * uavs_per_thread));

    // Verify all UAVs can be retrieved
    auto all_uavs = manager.getActiveUAVs();
    EXPECT_EQ(all_uavs.size(), static_cast<size_t>(num_threads * uavs_per_thread));

    // Verify specific UAV lookup
    for (int t = 0; t < num_threads; t++) {
        for (int i = 0; i < uavs_per_thread; i++) {
            std::string id = "LARGE-" + std::to_string(t * 1000 + i);
            auto* uav = manager.getUAV(id);
            ASSERT_NE(uav, nullptr) << "UAV " << id << " not found";
        }
    }
}

TEST_F(ThreadSafetyTest, SessionManager_CallbackOrderingTest) {
    // Test that callback ordering is consistent
    SessionManager manager(30000);

    std::vector<std::string> new_order;
    std::vector<std::string> update_order;
    std::mutex order_mutex;

    manager.setOnNewUAV([&](const UAVObject& uav) {
        std::lock_guard<std::mutex> lock(order_mutex);
        new_order.push_back(uav.id);
    });

    manager.setOnUAVUpdate([&](const UAVObject& uav) {
        std::lock_guard<std::mutex> lock(order_mutex);
        update_order.push_back(uav.id);
    });

    const int num_threads = 4;
    const int updates_per_thread = 20;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < updates_per_thread; i++) {
                UAVObject uav;
                uav.id = "ORDER-" + std::to_string(t);  // One ID per thread
                uav.rssi = -60 - (i % 30);
                uav.last_seen = std::chrono::steady_clock::now();

                manager.update(uav);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have exactly num_threads new UAV callbacks
    EXPECT_EQ(new_order.size(), static_cast<size_t>(num_threads));

    // All updates except first of each thread should be update callbacks
    EXPECT_EQ(update_order.size(), static_cast<size_t>(num_threads * (updates_per_thread - 1)));

    // Each thread's ID should appear in new_order exactly once
    std::set<std::string> unique_new(new_order.begin(), new_order.end());
    EXPECT_EQ(unique_new.size(), static_cast<size_t>(num_threads));
}

TEST_F(ThreadSafetyTest, SessionManager_TimeoutCallbackDuringUpdates) {
    // Test timeout callbacks firing while updates are happening
    SessionManager manager(50);  // 50ms timeout

    std::atomic<int> timeout_count{0};
    std::atomic<int> new_count{0};
    std::vector<std::string> timed_out_ids;
    std::mutex timeout_mutex;

    manager.setOnNewUAV([&](const UAVObject&) {
        new_count++;
    });

    manager.setOnUAVTimeout([&](const UAVObject& uav) {
        std::lock_guard<std::mutex> lock(timeout_mutex);
        timed_out_ids.push_back(uav.id);
        timeout_count++;
    });

    std::atomic<bool> running{true};

    // Thread that adds UAVs and lets them timeout
    std::thread adder([&]() {
        int batch = 0;
        while (running.load()) {
            // Add a batch of UAVs
            for (int i = 0; i < 5; i++) {
                UAVObject uav;
                uav.id = "TIMEOUT-" + std::to_string(batch) + "-" + std::to_string(i);
                uav.rssi = -70;
                uav.last_seen = std::chrono::steady_clock::now();

                manager.update(uav);
            }
            batch++;

            // Wait for them to timeout
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }
    });

    // Thread that triggers cleanup
    std::thread cleaner([&]() {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            manager.cleanup();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    running.store(false);

    adder.join();
    cleaner.join();

    EXPECT_GT(new_count.load(), 0);
    // Should have some timeouts
    EXPECT_GT(timeout_count.load(), 0);
}

TEST_F(ThreadSafetyTest, SessionManager_RapidUpdateSameUAV) {
    // Test rapid updates to the same UAV from multiple threads
    SessionManager manager(30000);

    const std::string target_id = "RAPID-UPDATE-TARGET";
    std::atomic<int> update_count{0};
    std::atomic<int> callback_count{0};

    manager.setOnUAVUpdate([&](const UAVObject& uav) {
        EXPECT_EQ(uav.id, target_id);
        callback_count++;
    });

    const int num_threads = 8;
    const int updates_per_thread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < updates_per_thread; i++) {
                UAVObject uav;
                uav.id = target_id;
                uav.rssi = -40 - (t * 5) - (i % 10);
                uav.last_seen = std::chrono::steady_clock::now();
                uav.location = createLocation(
                    37.0 + t * 0.0001 + i * 0.00001,
                    -122.0,
                    100.0f + (i % 50),
                    5.0f + (i % 20)
                );

                manager.update(uav);
                update_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(update_count.load(), num_threads * updates_per_thread);

    // Should have only 1 UAV
    EXPECT_EQ(manager.count(), 1u);

    // Get the final UAV and check it's valid
    auto* final_uav = manager.getUAV(target_id);
    ASSERT_NE(final_uav, nullptr);
    EXPECT_EQ(final_uav->id, target_id);
}

TEST_F(ThreadSafetyTest, Parser_ConcurrentCallbackModification) {
    // Test modifying callbacks while parsing is happening
    ParserConfig config;
    RemoteIDParser parser(config);
    parser.init();

    std::atomic<bool> running{true};
    std::atomic<int> callback_version{0};
    std::atomic<int> v1_calls{0};
    std::atomic<int> v2_calls{0};

    // Parse thread
    std::thread parser_thread([&]() {
        int i = 0;
        while (running.load()) {
            auto adv = createBasicIDAdv("CALLBACK-MOD-" + std::to_string(i % 50));
            parser.parse(adv, -60, TransportType::BT_LEGACY);
            i++;
            std::this_thread::yield();
        }
    });

    // Callback modification thread
    std::thread callback_thread([&]() {
        while (running.load()) {
            // Set callback version 1
            parser.setOnNewUAV([&](const UAVObject&) {
                v1_calls++;
            });
            callback_version = 1;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // Set callback version 2
            parser.setOnNewUAV([&](const UAVObject&) {
                v2_calls++;
            });
            callback_version = 2;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // Clear callback
            parser.setOnNewUAV(nullptr);
            callback_version = 0;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running.store(false);

    parser_thread.join();
    callback_thread.join();

    // Both callback versions should have been called at some point
    // (exact counts depend on timing)
    SUCCEED();
}

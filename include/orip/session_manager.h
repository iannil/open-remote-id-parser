#ifndef ORIP_SESSION_MANAGER_H
#define ORIP_SESSION_MANAGER_H

#include "orip/types.h"
#include <unordered_map>
#include <vector>
#include <chrono>
#include <functional>

namespace orip {

using UAVCallback = std::function<void(const UAVObject&)>;

class SessionManager {
public:
    explicit SessionManager(uint32_t timeout_ms = 30000);

    // Update or add a UAV
    // Returns true if this is a new UAV
    bool update(const UAVObject& uav);

    // Get all active UAVs
    std::vector<UAVObject> getActiveUAVs() const;

    // Get a specific UAV by ID
    const UAVObject* getUAV(const std::string& id) const;

    // Get count of active UAVs
    size_t count() const;

    // Remove timed-out UAVs
    // Returns list of removed UAV IDs
    std::vector<std::string> cleanup();

    // Clear all UAVs
    void clear();

    // Set callbacks
    void setOnNewUAV(UAVCallback callback);
    void setOnUAVUpdate(UAVCallback callback);
    void setOnUAVTimeout(UAVCallback callback);

private:
    std::unordered_map<std::string, UAVObject> uavs_;
    std::chrono::milliseconds timeout_;

    UAVCallback on_new_uav_;
    UAVCallback on_uav_update_;
    UAVCallback on_uav_timeout_;
};

} // namespace orip

#endif // ORIP_SESSION_MANAGER_H

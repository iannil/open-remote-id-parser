#include "orip/session_manager.h"
#include <algorithm>

namespace orip {

SessionManager::SessionManager(uint32_t timeout_ms)
    : timeout_(timeout_ms) {}

bool SessionManager::update(const UAVObject& uav) {
    if (uav.id.empty()) {
        return false;
    }

    auto it = uavs_.find(uav.id);
    bool is_new = (it == uavs_.end());

    if (is_new) {
        uavs_[uav.id] = uav;
        if (on_new_uav_) {
            on_new_uav_(uav);
        }
    } else {
        // Merge new data into existing UAV
        UAVObject& existing = it->second;

        // Update signal info
        existing.rssi = uav.rssi;
        existing.last_seen = uav.last_seen;
        existing.message_count++;

        // Update location if valid
        if (uav.location.valid) {
            existing.location = uav.location;
        }

        // Update system info if valid
        if (uav.system.valid) {
            existing.system = uav.system;
        }

        // Update self-ID if valid
        if (uav.self_id.valid) {
            existing.self_id = uav.self_id;
        }

        // Update operator ID if valid
        if (uav.operator_id.valid) {
            existing.operator_id = uav.operator_id;
        }

        // Update auth data if present
        if (!uav.auth_data.empty()) {
            existing.auth_data = uav.auth_data;
        }

        if (on_uav_update_) {
            on_uav_update_(existing);
        }
    }

    return is_new;
}

std::vector<UAVObject> SessionManager::getActiveUAVs() const {
    std::vector<UAVObject> result;
    result.reserve(uavs_.size());

    for (const auto& pair : uavs_) {
        result.push_back(pair.second);
    }

    // Sort by last seen (most recent first)
    std::sort(result.begin(), result.end(),
              [](const UAVObject& a, const UAVObject& b) {
                  return a.last_seen > b.last_seen;
              });

    return result;
}

const UAVObject* SessionManager::getUAV(const std::string& id) const {
    auto it = uavs_.find(id);
    return (it != uavs_.end()) ? &it->second : nullptr;
}

size_t SessionManager::count() const {
    return uavs_.size();
}

std::vector<std::string> SessionManager::cleanup() {
    std::vector<std::string> removed;
    auto now = std::chrono::steady_clock::now();

    for (auto it = uavs_.begin(); it != uavs_.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.last_seen);

        if (elapsed > timeout_) {
            if (on_uav_timeout_) {
                on_uav_timeout_(it->second);
            }
            removed.push_back(it->first);
            it = uavs_.erase(it);
        } else {
            ++it;
        }
    }

    return removed;
}

void SessionManager::clear() {
    uavs_.clear();
}

void SessionManager::setOnNewUAV(UAVCallback callback) {
    on_new_uav_ = std::move(callback);
}

void SessionManager::setOnUAVUpdate(UAVCallback callback) {
    on_uav_update_ = std::move(callback);
}

void SessionManager::setOnUAVTimeout(UAVCallback callback) {
    on_uav_timeout_ = std::move(callback);
}

} // namespace orip

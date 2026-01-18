#include "orip/orip_c.h"
#include "orip/orip.h"
#include <cstring>
#include <chrono>

/* ============================================================================
 * Internal wrapper structure
 * ============================================================================ */

struct orip_parser_t {
    orip::RemoteIDParser parser;
    orip_uav_callback_t on_new_uav;
    orip_uav_callback_t on_uav_update;
    orip_uav_callback_t on_uav_timeout;
    void* new_uav_user_data;
    void* update_user_data;
    void* timeout_user_data;

    orip_parser_t() : on_new_uav(nullptr), on_uav_update(nullptr),
                      on_uav_timeout(nullptr), new_uav_user_data(nullptr),
                      update_user_data(nullptr), timeout_user_data(nullptr) {}

    explicit orip_parser_t(const orip::ParserConfig& config)
        : parser(config), on_new_uav(nullptr), on_uav_update(nullptr),
          on_uav_timeout(nullptr), new_uav_user_data(nullptr),
          update_user_data(nullptr), timeout_user_data(nullptr) {}
};

/* ============================================================================
 * Helper functions
 * ============================================================================ */

static void convert_uav_to_c(const orip::UAVObject& src, orip_uav_t* dest) {
    std::memset(dest, 0, sizeof(orip_uav_t));

    // ID
    std::strncpy(dest->id, src.id.c_str(), ORIP_MAX_ID_LENGTH - 1);
    dest->id_type = static_cast<orip_id_type_t>(src.id_type);
    dest->uav_type = static_cast<orip_uav_type_t>(src.uav_type);

    // Protocol
    dest->protocol = static_cast<orip_protocol_t>(src.protocol);
    dest->transport = static_cast<orip_transport_t>(src.transport);

    // Signal
    dest->rssi = src.rssi;
    auto duration = src.last_seen.time_since_epoch();
    dest->last_seen_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());

    // Location
    if (src.location.valid) {
        dest->location.valid = 1;
        dest->location.latitude = src.location.latitude;
        dest->location.longitude = src.location.longitude;
        dest->location.altitude_baro = src.location.altitude_baro;
        dest->location.altitude_geo = src.location.altitude_geo;
        dest->location.height = src.location.height;
        dest->location.speed_horizontal = src.location.speed_horizontal;
        dest->location.speed_vertical = src.location.speed_vertical;
        dest->location.direction = src.location.direction;
        dest->location.status = static_cast<orip_uav_status_t>(src.location.status);
    }

    // System
    if (src.system.valid) {
        dest->system.valid = 1;
        dest->system.operator_latitude = src.system.operator_latitude;
        dest->system.operator_longitude = src.system.operator_longitude;
        dest->system.area_ceiling = src.system.area_ceiling;
        dest->system.area_floor = src.system.area_floor;
        dest->system.area_count = src.system.area_count;
        dest->system.area_radius = src.system.area_radius;
        dest->system.timestamp = src.system.timestamp;
    }

    // Self-ID
    if (src.self_id.valid) {
        dest->has_self_id = 1;
        std::strncpy(dest->self_id_description, src.self_id.description.c_str(),
                     ORIP_MAX_DESCRIPTION_LENGTH - 1);
    }

    // Operator ID
    if (src.operator_id.valid) {
        dest->has_operator_id = 1;
        std::strncpy(dest->operator_id, src.operator_id.id.c_str(),
                     ORIP_MAX_ID_LENGTH - 1);
    }

    // Stats
    dest->message_count = src.message_count;
}

static orip::ParserConfig convert_config_from_c(const orip_config_t* config) {
    orip::ParserConfig cfg;
    cfg.uav_timeout_ms = config->uav_timeout_ms;
    cfg.enable_deduplication = config->enable_deduplication != 0;
    cfg.enable_astm = config->enable_astm != 0;
    cfg.enable_asd = config->enable_asd != 0;
    cfg.enable_cn = config->enable_cn != 0;
    return cfg;
}

/* ============================================================================
 * Library functions implementation
 * ============================================================================ */

extern "C" {

const char* orip_version(void) {
    return orip::VERSION;
}

orip_config_t orip_default_config(void) {
    orip_config_t config;
    config.uav_timeout_ms = 30000;
    config.enable_deduplication = 1;
    config.enable_astm = 1;
    config.enable_asd = 0;
    config.enable_cn = 0;
    return config;
}

orip_parser_t* orip_create(void) {
    try {
        auto* parser = new orip_parser_t();
        parser->parser.init();
        return parser;
    } catch (...) {
        return nullptr;
    }
}

orip_parser_t* orip_create_with_config(const orip_config_t* config) {
    if (!config) {
        return orip_create();
    }

    try {
        auto cfg = convert_config_from_c(config);
        auto* parser = new orip_parser_t(cfg);
        parser->parser.init();
        return parser;
    } catch (...) {
        return nullptr;
    }
}

void orip_destroy(orip_parser_t* parser) {
    delete parser;
}

int orip_parse(orip_parser_t* parser,
               const uint8_t* payload,
               size_t payload_len,
               int8_t rssi,
               orip_transport_t transport,
               orip_result_t* result) {
    if (!parser || !payload || !result) {
        return -1;
    }

    std::memset(result, 0, sizeof(orip_result_t));

    try {
        std::vector<uint8_t> data(payload, payload + payload_len);
        auto cpp_result = parser->parser.parse(
            data, rssi, static_cast<orip::TransportType>(transport));

        result->success = cpp_result.success ? 1 : 0;
        result->is_remote_id = cpp_result.is_remote_id ? 1 : 0;
        result->protocol = static_cast<orip_protocol_t>(cpp_result.protocol);

        if (!cpp_result.error.empty()) {
            std::strncpy(result->error, cpp_result.error.c_str(),
                         sizeof(result->error) - 1);
        }

        if (cpp_result.success) {
            convert_uav_to_c(cpp_result.uav, &result->uav);
        }

        return 0;
    } catch (...) {
        std::strncpy(result->error, "Internal error", sizeof(result->error) - 1);
        return -1;
    }
}

size_t orip_get_active_count(const orip_parser_t* parser) {
    if (!parser) {
        return 0;
    }
    return parser->parser.getActiveCount();
}

size_t orip_get_active_uavs(const orip_parser_t* parser,
                            orip_uav_t* uavs,
                            size_t max_count) {
    if (!parser || !uavs || max_count == 0) {
        return 0;
    }

    try {
        auto cpp_uavs = parser->parser.getActiveUAVs();
        size_t count = std::min(cpp_uavs.size(), max_count);

        for (size_t i = 0; i < count; i++) {
            convert_uav_to_c(cpp_uavs[i], &uavs[i]);
        }

        return count;
    } catch (...) {
        return 0;
    }
}

int orip_get_uav(const orip_parser_t* parser,
                 const char* id,
                 orip_uav_t* uav) {
    if (!parser || !id || !uav) {
        return -1;
    }

    const orip::UAVObject* cpp_uav = parser->parser.getUAV(id);
    if (!cpp_uav) {
        return -1;
    }

    convert_uav_to_c(*cpp_uav, uav);
    return 0;
}

void orip_clear(orip_parser_t* parser) {
    if (parser) {
        parser->parser.clear();
    }
}

size_t orip_cleanup(orip_parser_t* parser) {
    if (!parser) {
        return 0;
    }

    size_t before = parser->parser.getActiveCount();
    parser->parser.cleanup();
    size_t after = parser->parser.getActiveCount();

    return before - after;
}

void orip_set_on_new_uav(orip_parser_t* parser,
                         orip_uav_callback_t callback,
                         void* user_data) {
    if (!parser) {
        return;
    }

    parser->on_new_uav = callback;
    parser->new_uav_user_data = user_data;

    if (callback) {
        parser->parser.setOnNewUAV([parser](const orip::UAVObject& uav) {
            orip_uav_t c_uav;
            convert_uav_to_c(uav, &c_uav);
            parser->on_new_uav(&c_uav, parser->new_uav_user_data);
        });
    } else {
        parser->parser.setOnNewUAV(nullptr);
    }
}

void orip_set_on_uav_update(orip_parser_t* parser,
                            orip_uav_callback_t callback,
                            void* user_data) {
    if (!parser) {
        return;
    }

    parser->on_uav_update = callback;
    parser->update_user_data = user_data;

    if (callback) {
        parser->parser.setOnUAVUpdate([parser](const orip::UAVObject& uav) {
            orip_uav_t c_uav;
            convert_uav_to_c(uav, &c_uav);
            parser->on_uav_update(&c_uav, parser->update_user_data);
        });
    } else {
        parser->parser.setOnUAVUpdate(nullptr);
    }
}

void orip_set_on_uav_timeout(orip_parser_t* parser,
                             orip_uav_callback_t callback,
                             void* user_data) {
    if (!parser) {
        return;
    }

    parser->on_uav_timeout = callback;
    parser->timeout_user_data = user_data;

    if (callback) {
        parser->parser.setOnUAVTimeout([parser](const orip::UAVObject& uav) {
            orip_uav_t c_uav;
            convert_uav_to_c(uav, &c_uav);
            parser->on_uav_timeout(&c_uav, parser->timeout_user_data);
        });
    } else {
        parser->parser.setOnUAVTimeout(nullptr);
    }
}

} /* extern "C" */

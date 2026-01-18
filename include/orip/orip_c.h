#ifndef ORIP_C_H
#define ORIP_C_H

/**
 * ORIP C API - Pure C interface for cross-language bindings
 *
 * This header provides a C-compatible API for use with:
 * - JNI (Android/Java)
 * - Python ctypes/cffi
 * - Other FFI systems
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Opaque handle types
 * ============================================================================ */

typedef struct orip_parser_t orip_parser_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

typedef enum {
    ORIP_PROTOCOL_UNKNOWN = 0,
    ORIP_PROTOCOL_ASTM_F3411 = 1,
    ORIP_PROTOCOL_ASD_STAN = 2,
    ORIP_PROTOCOL_CN_RID = 3
} orip_protocol_t;

typedef enum {
    ORIP_TRANSPORT_UNKNOWN = 0,
    ORIP_TRANSPORT_BT_LEGACY = 1,
    ORIP_TRANSPORT_BT_EXTENDED = 2,
    ORIP_TRANSPORT_WIFI_BEACON = 3,
    ORIP_TRANSPORT_WIFI_NAN = 4
} orip_transport_t;

typedef enum {
    ORIP_ID_TYPE_NONE = 0,
    ORIP_ID_TYPE_SERIAL_NUMBER = 1,
    ORIP_ID_TYPE_CAA_REGISTRATION = 2,
    ORIP_ID_TYPE_UTM_ASSIGNED = 3,
    ORIP_ID_TYPE_SPECIFIC_SESSION = 4
} orip_id_type_t;

typedef enum {
    ORIP_UAV_TYPE_NONE = 0,
    ORIP_UAV_TYPE_AEROPLANE = 1,
    ORIP_UAV_TYPE_HELICOPTER_OR_MULTIROTOR = 2,
    ORIP_UAV_TYPE_GYROPLANE = 3,
    ORIP_UAV_TYPE_HYBRID_LIFT = 4,
    ORIP_UAV_TYPE_ORNITHOPTER = 5,
    ORIP_UAV_TYPE_GLIDER = 6,
    ORIP_UAV_TYPE_KITE = 7,
    ORIP_UAV_TYPE_FREE_BALLOON = 8,
    ORIP_UAV_TYPE_CAPTIVE_BALLOON = 9,
    ORIP_UAV_TYPE_AIRSHIP = 10,
    ORIP_UAV_TYPE_FREE_FALL_PARACHUTE = 11,
    ORIP_UAV_TYPE_ROCKET = 12,
    ORIP_UAV_TYPE_TETHERED_POWERED = 13,
    ORIP_UAV_TYPE_GROUND_OBSTACLE = 14,
    ORIP_UAV_TYPE_OTHER = 15
} orip_uav_type_t;

typedef enum {
    ORIP_STATUS_UNDECLARED = 0,
    ORIP_STATUS_GROUND = 1,
    ORIP_STATUS_AIRBORNE = 2,
    ORIP_STATUS_EMERGENCY = 3,
    ORIP_STATUS_REMOTE_ID_FAILURE = 4
} orip_uav_status_t;

/* ============================================================================
 * Data structures (C-compatible, fixed-size)
 * ============================================================================ */

#define ORIP_MAX_ID_LENGTH 64
#define ORIP_MAX_DESCRIPTION_LENGTH 64

typedef struct {
    int valid;
    double latitude;
    double longitude;
    float altitude_baro;
    float altitude_geo;
    float height;
    float speed_horizontal;
    float speed_vertical;
    float direction;
    orip_uav_status_t status;
} orip_location_t;

typedef struct {
    int valid;
    double operator_latitude;
    double operator_longitude;
    float area_ceiling;
    float area_floor;
    uint16_t area_count;
    uint16_t area_radius;
    uint32_t timestamp;
} orip_system_info_t;

typedef struct {
    /* Identification */
    char id[ORIP_MAX_ID_LENGTH];
    orip_id_type_t id_type;
    orip_uav_type_t uav_type;

    /* Protocol info */
    orip_protocol_t protocol;
    orip_transport_t transport;

    /* Signal info */
    int8_t rssi;
    uint64_t last_seen_ms;  /* Milliseconds since epoch */

    /* Location data */
    orip_location_t location;

    /* System data */
    orip_system_info_t system;

    /* Self-ID */
    int has_self_id;
    char self_id_description[ORIP_MAX_DESCRIPTION_LENGTH];

    /* Operator ID */
    int has_operator_id;
    char operator_id[ORIP_MAX_ID_LENGTH];

    /* Statistics */
    uint32_t message_count;
} orip_uav_t;

typedef struct {
    int success;
    int is_remote_id;
    orip_protocol_t protocol;
    char error[128];
    orip_uav_t uav;
} orip_result_t;

typedef struct {
    uint32_t uav_timeout_ms;
    int enable_deduplication;
    int enable_astm;
    int enable_asd;
    int enable_cn;
} orip_config_t;

/* ============================================================================
 * Callback types
 * ============================================================================ */

typedef void (*orip_uav_callback_t)(const orip_uav_t* uav, void* user_data);

/* ============================================================================
 * Library functions
 * ============================================================================ */

/**
 * Get library version string
 */
const char* orip_version(void);

/**
 * Get default configuration
 */
orip_config_t orip_default_config(void);

/**
 * Create a new parser instance with default configuration
 * @return Parser handle, or NULL on failure
 */
orip_parser_t* orip_create(void);

/**
 * Create a new parser instance with custom configuration
 * @param config Configuration options
 * @return Parser handle, or NULL on failure
 */
orip_parser_t* orip_create_with_config(const orip_config_t* config);

/**
 * Destroy a parser instance and free resources
 * @param parser Parser handle
 */
void orip_destroy(orip_parser_t* parser);

/**
 * Parse a raw Bluetooth/WiFi payload
 * @param parser Parser handle
 * @param payload Raw data bytes
 * @param payload_len Length of payload
 * @param rssi Signal strength (dBm)
 * @param transport Transport type
 * @param result Output result structure
 * @return 0 on success, non-zero on error
 */
int orip_parse(orip_parser_t* parser,
               const uint8_t* payload,
               size_t payload_len,
               int8_t rssi,
               orip_transport_t transport,
               orip_result_t* result);

/**
 * Get count of active UAVs
 * @param parser Parser handle
 * @return Number of active UAVs
 */
size_t orip_get_active_count(const orip_parser_t* parser);

/**
 * Get list of active UAVs
 * @param parser Parser handle
 * @param uavs Output array (caller allocated)
 * @param max_count Maximum number of UAVs to retrieve
 * @return Actual number of UAVs copied
 */
size_t orip_get_active_uavs(const orip_parser_t* parser,
                            orip_uav_t* uavs,
                            size_t max_count);

/**
 * Get a specific UAV by ID
 * @param parser Parser handle
 * @param id UAV ID string
 * @param uav Output UAV structure
 * @return 0 if found, non-zero if not found
 */
int orip_get_uav(const orip_parser_t* parser,
                 const char* id,
                 orip_uav_t* uav);

/**
 * Clear all tracked UAVs
 * @param parser Parser handle
 */
void orip_clear(orip_parser_t* parser);

/**
 * Trigger cleanup of timed-out UAVs
 * @param parser Parser handle
 * @return Number of UAVs removed
 */
size_t orip_cleanup(orip_parser_t* parser);

/**
 * Set callback for new UAV detection
 * @param parser Parser handle
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void orip_set_on_new_uav(orip_parser_t* parser,
                         orip_uav_callback_t callback,
                         void* user_data);

/**
 * Set callback for UAV update
 * @param parser Parser handle
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void orip_set_on_uav_update(orip_parser_t* parser,
                            orip_uav_callback_t callback,
                            void* user_data);

/**
 * Set callback for UAV timeout (removed)
 * @param parser Parser handle
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void orip_set_on_uav_timeout(orip_parser_t* parser,
                             orip_uav_callback_t callback,
                             void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* ORIP_C_H */

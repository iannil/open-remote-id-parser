package com.orip

/**
 * Protocol types supported by the parser
 */
enum class ProtocolType(val value: Int) {
    UNKNOWN(0),
    ASTM_F3411(1),
    ASD_STAN(2),
    CN_RID(3);

    companion object {
        fun fromValue(value: Int): ProtocolType =
            entries.find { it.value == value } ?: UNKNOWN
    }
}

/**
 * Transport layer type
 */
enum class TransportType(val value: Int) {
    UNKNOWN(0),
    BT_LEGACY(1),
    BT_EXTENDED(2),
    WIFI_BEACON(3),
    WIFI_NAN(4);

    companion object {
        fun fromValue(value: Int): TransportType =
            entries.find { it.value == value } ?: UNKNOWN
    }
}

/**
 * UAV identification type
 */
enum class UAVIdType(val value: Int) {
    NONE(0),
    SERIAL_NUMBER(1),
    CAA_REGISTRATION(2),
    UTM_ASSIGNED(3),
    SPECIFIC_SESSION(4);

    companion object {
        fun fromValue(value: Int): UAVIdType =
            entries.find { it.value == value } ?: NONE
    }
}

/**
 * UAV type classification
 */
enum class UAVType(val value: Int) {
    NONE(0),
    AEROPLANE(1),
    HELICOPTER_OR_MULTIROTOR(2),
    GYROPLANE(3),
    HYBRID_LIFT(4),
    ORNITHOPTER(5),
    GLIDER(6),
    KITE(7),
    FREE_BALLOON(8),
    CAPTIVE_BALLOON(9),
    AIRSHIP(10),
    FREE_FALL_PARACHUTE(11),
    ROCKET(12),
    TETHERED_POWERED(13),
    GROUND_OBSTACLE(14),
    OTHER(15);

    companion object {
        fun fromValue(value: Int): UAVType =
            entries.find { it.value == value } ?: NONE
    }
}

/**
 * UAV status
 */
enum class UAVStatus(val value: Int) {
    UNDECLARED(0),
    GROUND(1),
    AIRBORNE(2),
    EMERGENCY(3),
    REMOTE_ID_FAILURE(4);

    companion object {
        fun fromValue(value: Int): UAVStatus =
            entries.find { it.value == value } ?: UNDECLARED
    }
}

package com.orip

/**
 * Location and vector data for a UAV
 */
data class LocationData(
    val valid: Boolean = false,
    val latitude: Double = 0.0,
    val longitude: Double = 0.0,
    val altitudeBaro: Float = 0f,
    val altitudeGeo: Float = 0f,
    val height: Float = 0f,
    val speedHorizontal: Float = 0f,
    val speedVertical: Float = 0f,
    val direction: Float = 0f,
    val status: UAVStatus = UAVStatus.UNDECLARED
) {
    companion object {
        internal fun fromNative(
            valid: Boolean,
            latitude: Double,
            longitude: Double,
            altitudeBaro: Float,
            altitudeGeo: Float,
            height: Float,
            speedHorizontal: Float,
            speedVertical: Float,
            direction: Float,
            status: Int
        ): LocationData = LocationData(
            valid = valid,
            latitude = latitude,
            longitude = longitude,
            altitudeBaro = altitudeBaro,
            altitudeGeo = altitudeGeo,
            height = height,
            speedHorizontal = speedHorizontal,
            speedVertical = speedVertical,
            direction = direction,
            status = UAVStatus.fromValue(status)
        )
    }
}

/**
 * System information (operator/pilot location)
 */
data class SystemInfo(
    val valid: Boolean = false,
    val operatorLatitude: Double = 0.0,
    val operatorLongitude: Double = 0.0,
    val areaCeiling: Float = 0f,
    val areaFloor: Float = 0f,
    val areaCount: Int = 0,
    val areaRadius: Int = 0,
    val timestamp: Long = 0
) {
    companion object {
        internal fun fromNative(
            valid: Boolean,
            operatorLatitude: Double,
            operatorLongitude: Double,
            areaCeiling: Float,
            areaFloor: Float,
            areaCount: Int,
            areaRadius: Int,
            timestamp: Long
        ): SystemInfo = SystemInfo(
            valid = valid,
            operatorLatitude = operatorLatitude,
            operatorLongitude = operatorLongitude,
            areaCeiling = areaCeiling,
            areaFloor = areaFloor,
            areaCount = areaCount,
            areaRadius = areaRadius,
            timestamp = timestamp
        )
    }
}

/**
 * Complete UAV object containing all parsed data
 */
data class UAVObject(
    val id: String,
    val idType: UAVIdType,
    val uavType: UAVType,
    val protocol: ProtocolType,
    val transport: TransportType,
    val rssi: Int,
    val lastSeenMs: Long,
    val location: LocationData,
    val system: SystemInfo,
    val selfIdDescription: String?,
    val operatorId: String?,
    val messageCount: Int
) {
    companion object {
        internal fun fromNative(
            id: String,
            idType: Int,
            uavType: Int,
            protocol: Int,
            transport: Int,
            rssi: Int,
            lastSeenMs: Long,
            location: LocationData,
            system: SystemInfo,
            selfIdDescription: String?,
            operatorId: String?,
            messageCount: Int
        ): UAVObject = UAVObject(
            id = id,
            idType = UAVIdType.fromValue(idType),
            uavType = UAVType.fromValue(uavType),
            protocol = ProtocolType.fromValue(protocol),
            transport = TransportType.fromValue(transport),
            rssi = rssi,
            lastSeenMs = lastSeenMs,
            location = location,
            system = system,
            selfIdDescription = selfIdDescription,
            operatorId = operatorId,
            messageCount = messageCount
        )
    }
}

/**
 * Parse result returned by the parser
 */
data class ParseResult(
    val success: Boolean,
    val isRemoteId: Boolean,
    val protocol: ProtocolType,
    val error: String?,
    val uav: UAVObject?
)

/**
 * Parser configuration
 */
data class ParserConfig(
    val uavTimeoutMs: Int = 30000,
    val enableDeduplication: Boolean = true,
    val enableAstm: Boolean = true,
    val enableAsd: Boolean = false,
    val enableCn: Boolean = false
)

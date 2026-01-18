package com.orip

import java.io.Closeable

/**
 * Remote ID Parser for detecting and parsing drone identification signals.
 *
 * This class provides the main interface for parsing Bluetooth and WiFi
 * Remote ID advertisements as defined by ASTM F3411.
 *
 * Example usage:
 * ```kotlin
 * val parser = RemoteIDParser()
 *
 * // Set callbacks
 * parser.setOnNewUAV { uav ->
 *     println("New drone detected: ${uav.id}")
 * }
 *
 * // Parse BLE scan result
 * val result = parser.parse(scanRecord.bytes, rssi, TransportType.BT_LEGACY)
 * if (result.success) {
 *     println("Detected: ${result.uav?.id}")
 * }
 *
 * // Get all active drones
 * val drones = parser.getActiveUAVs()
 *
 * // Don't forget to close when done
 * parser.close()
 * ```
 */
class RemoteIDParser @JvmOverloads constructor(
    config: ParserConfig = ParserConfig()
) : Closeable {

    private var nativeHandle: Long = 0

    /**
     * Callback interface for UAV events
     */
    fun interface UAVCallback {
        fun onUAV(uav: UAVObject)
    }

    private var onNewUAV: UAVCallback? = null
    private var onUAVUpdate: UAVCallback? = null
    private var onUAVTimeout: UAVCallback? = null

    init {
        nativeHandle = nativeCreate(
            config.uavTimeoutMs,
            config.enableDeduplication,
            config.enableAstm,
            config.enableAsd,
            config.enableCn
        )
        if (nativeHandle == 0L) {
            throw RuntimeException("Failed to create native parser")
        }
    }

    /**
     * Parse a raw Bluetooth or WiFi payload.
     *
     * @param payload Raw advertisement/beacon data
     * @param rssi Signal strength in dBm
     * @param transport Transport type (BT_LEGACY, BT_EXTENDED, WIFI_BEACON, etc.)
     * @return ParseResult containing success status and parsed UAV data
     */
    fun parse(payload: ByteArray, rssi: Int, transport: TransportType): ParseResult {
        checkNotClosed()
        return nativeParse(nativeHandle, payload, rssi, transport.value)
    }

    /**
     * Get the number of currently active (recently seen) UAVs.
     */
    fun getActiveCount(): Int {
        checkNotClosed()
        return nativeGetActiveCount(nativeHandle)
    }

    /**
     * Get a list of all currently active UAVs.
     */
    fun getActiveUAVs(): List<UAVObject> {
        checkNotClosed()
        return nativeGetActiveUAVs(nativeHandle)
    }

    /**
     * Get a specific UAV by its ID.
     *
     * @param id The UAV identifier (serial number or registration)
     * @return The UAVObject if found, null otherwise
     */
    fun getUAV(id: String): UAVObject? {
        checkNotClosed()
        return nativeGetUAV(nativeHandle, id)
    }

    /**
     * Clear all tracked UAVs.
     */
    fun clear() {
        checkNotClosed()
        nativeClear(nativeHandle)
    }

    /**
     * Manually trigger cleanup of timed-out UAVs.
     *
     * @return Number of UAVs removed
     */
    fun cleanup(): Int {
        checkNotClosed()
        return nativeCleanup(nativeHandle)
    }

    /**
     * Set callback for new UAV detection.
     */
    fun setOnNewUAV(callback: UAVCallback?) {
        checkNotClosed()
        onNewUAV = callback
        nativeSetCallbacksEnabled(nativeHandle, callback != null, onUAVUpdate != null, onUAVTimeout != null)
    }

    /**
     * Set callback for UAV updates.
     */
    fun setOnUAVUpdate(callback: UAVCallback?) {
        checkNotClosed()
        onUAVUpdate = callback
        nativeSetCallbacksEnabled(nativeHandle, onNewUAV != null, callback != null, onUAVTimeout != null)
    }

    /**
     * Set callback for UAV timeout (removal).
     */
    fun setOnUAVTimeout(callback: UAVCallback?) {
        checkNotClosed()
        onUAVTimeout = callback
        nativeSetCallbacksEnabled(nativeHandle, onNewUAV != null, onUAVUpdate != null, callback != null)
    }

    /**
     * Called from JNI when a new UAV is detected.
     */
    @Suppress("unused")
    private fun onNativeNewUAV(uav: UAVObject) {
        onNewUAV?.onUAV(uav)
    }

    /**
     * Called from JNI when a UAV is updated.
     */
    @Suppress("unused")
    private fun onNativeUAVUpdate(uav: UAVObject) {
        onUAVUpdate?.onUAV(uav)
    }

    /**
     * Called from JNI when a UAV times out.
     */
    @Suppress("unused")
    private fun onNativeUAVTimeout(uav: UAVObject) {
        onUAVTimeout?.onUAV(uav)
    }

    private fun checkNotClosed() {
        if (nativeHandle == 0L) {
            throw IllegalStateException("Parser has been closed")
        }
    }

    override fun close() {
        if (nativeHandle != 0L) {
            nativeDestroy(nativeHandle)
            nativeHandle = 0
        }
    }

    protected fun finalize() {
        close()
    }

    companion object {
        init {
            System.loadLibrary("orip_jni")
        }

        /**
         * Get the library version string.
         */
        @JvmStatic
        external fun getVersion(): String
    }

    // Native methods
    private external fun nativeCreate(
        timeoutMs: Int,
        enableDeduplication: Boolean,
        enableAstm: Boolean,
        enableAsd: Boolean,
        enableCn: Boolean
    ): Long

    private external fun nativeDestroy(handle: Long)

    private external fun nativeParse(
        handle: Long,
        payload: ByteArray,
        rssi: Int,
        transport: Int
    ): ParseResult

    private external fun nativeGetActiveCount(handle: Long): Int

    private external fun nativeGetActiveUAVs(handle: Long): List<UAVObject>

    private external fun nativeGetUAV(handle: Long, id: String): UAVObject?

    private external fun nativeClear(handle: Long)

    private external fun nativeCleanup(handle: Long): Int

    private external fun nativeSetCallbacksEnabled(
        handle: Long,
        newUav: Boolean,
        update: Boolean,
        timeout: Boolean
    )
}

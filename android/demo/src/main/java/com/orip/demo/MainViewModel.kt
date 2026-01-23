package com.orip.demo

import androidx.lifecycle.ViewModel
import com.orip.ParserConfig
import com.orip.RemoteIDParser
import com.orip.TransportType
import com.orip.UAVObject
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update

/**
 * Data class representing scan statistics
 */
data class ScanStats(
    val packetsProcessed: Long = 0,
    val remoteIdPackets: Long = 0
)

/**
 * ViewModel for managing Remote ID parser and UAV data.
 *
 * This ViewModel:
 * - Holds the RemoteIDParser instance
 * - Processes incoming BLE packets
 * - Maintains the list of detected UAVs
 * - Tracks scan statistics
 */
class MainViewModel : ViewModel() {

    // Parser configuration - enable ASTM and ASD-STAN protocols
    private val parserConfig = ParserConfig(
        uavTimeoutMs = 30000,       // 30 second timeout
        enableDeduplication = true,
        enableAstm = true,          // US/International standard
        enableAsd = true,           // EU standard
        enableCn = false            // Not yet implemented
    )

    // The ORIP parser instance
    private val parser: RemoteIDParser = RemoteIDParser(parserConfig)

    // State flow for the list of UAVs
    private val _uavList = MutableStateFlow<List<UAVObject>>(emptyList())
    val uavList: StateFlow<List<UAVObject>> = _uavList.asStateFlow()

    // State flow for scan statistics
    private val _scanStats = MutableStateFlow(ScanStats())
    val scanStats: StateFlow<ScanStats> = _scanStats.asStateFlow()

    init {
        // Set up callbacks for UAV events
        parser.setOnNewUAV { uav ->
            updateUAVList()
        }

        parser.setOnUAVUpdate { uav ->
            updateUAVList()
        }

        parser.setOnUAVTimeout { uav ->
            updateUAVList()
        }
    }

    /**
     * Process a received BLE packet.
     *
     * @param bytes Raw advertisement data
     * @param rssi Signal strength in dBm
     * @param transport Transport type (BT_LEGACY or BT_EXTENDED)
     */
    fun processPacket(bytes: ByteArray, rssi: Int, transport: TransportType) {
        _scanStats.update { stats ->
            stats.copy(packetsProcessed = stats.packetsProcessed + 1)
        }

        val result = parser.parse(bytes, rssi, transport)

        if (result.isRemoteId) {
            _scanStats.update { stats ->
                stats.copy(remoteIdPackets = stats.remoteIdPackets + 1)
            }
        }

        // UAV list is updated via callbacks
    }

    /**
     * Clear all tracked UAVs and reset statistics.
     */
    fun clearUAVs() {
        parser.clear()
        _uavList.value = emptyList()
        _scanStats.value = ScanStats()
    }

    /**
     * Update the UAV list from the parser.
     */
    private fun updateUAVList() {
        _uavList.value = parser.getActiveUAVs()
            .sortedByDescending { it.rssi }  // Sort by signal strength
    }

    /**
     * Clean up resources.
     */
    fun cleanup() {
        parser.close()
    }

    override fun onCleared() {
        super.onCleared()
        cleanup()
    }
}

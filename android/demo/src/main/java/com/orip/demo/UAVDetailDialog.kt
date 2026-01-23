package com.orip.demo

import android.app.Dialog
import android.os.Bundle
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.FragmentManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.orip.UAVObject

/**
 * Dialog showing detailed information about a UAV.
 */
class UAVDetailDialog : DialogFragment() {

    private var uav: UAVObject? = null

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val uav = this.uav ?: return super.onCreateDialog(savedInstanceState)

        val details = buildString {
            appendLine("=== Basic Information ===")
            appendLine("ID: ${uav.id}")
            appendLine("ID Type: ${uav.idType.name}")
            appendLine("UAV Type: ${uav.uavType.name}")
            appendLine()

            appendLine("=== Protocol ===")
            appendLine("Protocol: ${uav.protocol.name}")
            appendLine("Transport: ${uav.transport.name}")
            appendLine()

            appendLine("=== Signal ===")
            appendLine("RSSI: ${uav.rssi} dBm")
            appendLine("Message Count: ${uav.messageCount}")
            appendLine()

            if (uav.location.valid) {
                appendLine("=== Location ===")
                appendLine("Latitude: ${uav.location.latitude}")
                appendLine("Longitude: ${uav.location.longitude}")
                appendLine("Altitude (Baro): ${uav.location.altitudeBaro} m")
                appendLine("Altitude (Geo): ${uav.location.altitudeGeo} m")
                appendLine("Height: ${uav.location.height} m")
                appendLine("Speed (H): ${uav.location.speedHorizontal} m/s")
                appendLine("Speed (V): ${uav.location.speedVertical} m/s")
                appendLine("Direction: ${uav.location.direction}°")
                appendLine("Status: ${uav.location.status.name}")
                appendLine()
            }

            if (uav.system.valid) {
                appendLine("=== System/Operator ===")
                appendLine("Operator Lat: ${uav.system.operatorLatitude}")
                appendLine("Operator Lon: ${uav.system.operatorLongitude}")
                appendLine("Area Ceiling: ${uav.system.areaCeiling} m")
                appendLine("Area Floor: ${uav.system.areaFloor} m")
                appendLine("Area Count: ${uav.system.areaCount}")
                appendLine("Area Radius: ${uav.system.areaRadius} m")
                appendLine()
            }

            uav.operatorId?.let {
                appendLine("=== Operator ID ===")
                appendLine(it)
                appendLine()
            }

            uav.selfIdDescription?.let {
                appendLine("=== Self-ID ===")
                appendLine(it)
            }
        }

        return MaterialAlertDialogBuilder(requireContext())
            .setTitle("UAV Details")
            .setMessage(details)
            .setPositiveButton("Close", null)
            .create()
    }

    companion object {
        private const val TAG = "UAVDetailDialog"

        fun show(fragmentManager: FragmentManager, uav: UAVObject) {
            val dialog = UAVDetailDialog().apply {
                this.uav = uav
            }
            dialog.show(fragmentManager, TAG)
        }
    }
}

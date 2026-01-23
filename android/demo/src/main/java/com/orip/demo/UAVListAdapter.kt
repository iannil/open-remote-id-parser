package com.orip.demo

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.orip.UAVObject
import com.orip.UAVStatus
import com.orip.demo.databinding.ItemUavBinding

/**
 * RecyclerView adapter for displaying a list of UAVs.
 */
class UAVListAdapter(
    private val onItemClick: (UAVObject) -> Unit
) : ListAdapter<UAVObject, UAVListAdapter.UAVViewHolder>(UAVDiffCallback()) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): UAVViewHolder {
        val binding = ItemUavBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return UAVViewHolder(binding)
    }

    override fun onBindViewHolder(holder: UAVViewHolder, position: Int) {
        holder.bind(getItem(position), onItemClick)
    }

    class UAVViewHolder(
        private val binding: ItemUavBinding
    ) : RecyclerView.ViewHolder(binding.root) {

        fun bind(uav: UAVObject, onItemClick: (UAVObject) -> Unit) {
            binding.apply {
                // ID and type
                textId.text = uav.id.ifEmpty { "Unknown ID" }
                textIdType.text = uav.idType.name.replace("_", " ")
                textUavType.text = uav.uavType.name.replace("_", " ")

                // Protocol and transport
                textProtocol.text = uav.protocol.name.replace("_", " ")
                textTransport.text = uav.transport.name.replace("_", " ")

                // Signal strength with color indicator
                textRssi.text = "${uav.rssi} dBm"
                val rssiColor = when {
                    uav.rssi >= -50 -> android.R.color.holo_green_dark
                    uav.rssi >= -70 -> android.R.color.holo_orange_dark
                    else -> android.R.color.holo_red_dark
                }
                textRssi.setTextColor(root.context.getColor(rssiColor))

                // Location if available
                if (uav.location.valid) {
                    textLocation.text = String.format(
                        "%.6f, %.6f",
                        uav.location.latitude,
                        uav.location.longitude
                    )
                    textAltitude.text = String.format(
                        "Alt: %.1f m",
                        uav.location.altitudeGeo
                    )
                    textSpeed.text = String.format(
                        "Speed: %.1f m/s",
                        uav.location.speedHorizontal
                    )

                    // Status indicator
                    val statusText = when (uav.location.status) {
                        UAVStatus.AIRBORNE -> "Airborne"
                        UAVStatus.ON_GROUND -> "On Ground"
                        else -> "Unknown"
                    }
                    textStatus.text = statusText
                } else {
                    textLocation.text = "Location unavailable"
                    textAltitude.text = ""
                    textSpeed.text = ""
                    textStatus.text = ""
                }

                // Operator ID if available
                textOperatorId.text = uav.operatorId?.let { "Operator: $it" } ?: ""

                // Click listener
                root.setOnClickListener { onItemClick(uav) }
            }
        }
    }

    /**
     * DiffUtil callback for efficient list updates.
     */
    class UAVDiffCallback : DiffUtil.ItemCallback<UAVObject>() {
        override fun areItemsTheSame(oldItem: UAVObject, newItem: UAVObject): Boolean {
            return oldItem.id == newItem.id
        }

        override fun areContentsTheSame(oldItem: UAVObject, newItem: UAVObject): Boolean {
            return oldItem == newItem
        }
    }
}

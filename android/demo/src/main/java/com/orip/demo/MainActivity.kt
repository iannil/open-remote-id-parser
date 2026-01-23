package com.orip.demo

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import com.orip.RemoteIDParser
import com.orip.TransportType
import com.orip.demo.databinding.ActivityMainBinding
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/**
 * Main activity demonstrating ORIP library integration with BLE scanning.
 *
 * This demo shows how to:
 * 1. Request necessary permissions for BLE scanning
 * 2. Set up BLE scanning with appropriate settings
 * 3. Parse Remote ID advertisements using ORIP
 * 4. Display detected drones in a RecyclerView
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val viewModel: MainViewModel by viewModels()

    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bluetoothLeScanner: BluetoothLeScanner? = null
    private var isScanning = false

    private lateinit var uavAdapter: UAVListAdapter

    // Permission request launcher
    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val allGranted = permissions.entries.all { it.value }
        if (allGranted) {
            startScanning()
        } else {
            Toast.makeText(this, "Permissions required for BLE scanning", Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupBluetooth()
        setupUI()
        observeViewModel()
    }

    private fun setupBluetooth() {
        val bluetoothManager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter
        bluetoothLeScanner = bluetoothAdapter?.bluetoothLeScanner
    }

    private fun setupUI() {
        // Setup RecyclerView
        uavAdapter = UAVListAdapter { uav ->
            // Handle UAV click - show details
            UAVDetailDialog.show(supportFragmentManager, uav)
        }
        binding.recyclerView.apply {
            layoutManager = LinearLayoutManager(this@MainActivity)
            adapter = uavAdapter
        }

        // Setup scan button
        binding.fabScan.setOnClickListener {
            if (isScanning) {
                stopScanning()
            } else {
                checkPermissionsAndScan()
            }
        }

        // Setup swipe refresh
        binding.swipeRefresh.setOnRefreshListener {
            viewModel.clearUAVs()
            binding.swipeRefresh.isRefreshing = false
        }

        // Show library version
        binding.textVersion.text = "ORIP ${RemoteIDParser.getVersion()}"
    }

    private fun observeViewModel() {
        lifecycleScope.launch {
            viewModel.uavList.collectLatest { uavs ->
                uavAdapter.submitList(uavs.toList())
                binding.textCount.text = getString(R.string.drone_count, uavs.size)

                // Show/hide empty state
                binding.emptyState.visibility = if (uavs.isEmpty()) {
                    android.view.View.VISIBLE
                } else {
                    android.view.View.GONE
                }
            }
        }

        lifecycleScope.launch {
            viewModel.scanStats.collectLatest { stats ->
                binding.textStats.text = getString(
                    R.string.scan_stats,
                    stats.packetsProcessed,
                    stats.remoteIdPackets
                )
            }
        }
    }

    private fun checkPermissionsAndScan() {
        val requiredPermissions = mutableListOf<String>()

        // Location permission (required for BLE on Android 6-11)
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
            != PackageManager.PERMISSION_GRANTED
        ) {
            requiredPermissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        // Bluetooth permissions for Android 12+
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN)
                != PackageManager.PERMISSION_GRANTED
            ) {
                requiredPermissions.add(Manifest.permission.BLUETOOTH_SCAN)
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED
            ) {
                requiredPermissions.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
        }

        if (requiredPermissions.isNotEmpty()) {
            permissionLauncher.launch(requiredPermissions.toTypedArray())
        } else {
            startScanning()
        }
    }

    private fun startScanning() {
        if (bluetoothAdapter?.isEnabled != true) {
            Toast.makeText(this, "Please enable Bluetooth", Toast.LENGTH_SHORT).show()
            return
        }

        val scanner = bluetoothLeScanner ?: run {
            Toast.makeText(this, "BLE Scanner not available", Toast.LENGTH_SHORT).show()
            return
        }

        // Configure scan settings for Remote ID detection
        val scanSettings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .setReportDelay(0)
            .build()

        try {
            scanner.startScan(null, scanSettings, scanCallback)
            isScanning = true
            updateScanUI(true)
            Toast.makeText(this, "Scanning for drones...", Toast.LENGTH_SHORT).show()
        } catch (e: SecurityException) {
            Toast.makeText(this, "Permission denied for BLE scan", Toast.LENGTH_SHORT).show()
        }
    }

    private fun stopScanning() {
        try {
            bluetoothLeScanner?.stopScan(scanCallback)
        } catch (e: SecurityException) {
            // Ignore - we're just stopping
        }
        isScanning = false
        updateScanUI(false)
    }

    private fun updateScanUI(scanning: Boolean) {
        binding.fabScan.setImageResource(
            if (scanning) R.drawable.ic_stop else R.drawable.ic_scan
        )
        binding.textScanStatus.text = if (scanning) {
            getString(R.string.scanning)
        } else {
            getString(R.string.tap_to_scan)
        }
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            result.scanRecord?.bytes?.let { bytes ->
                // Determine transport type based on advertisement type
                val transport = if (result.isLegacy) {
                    TransportType.BT_LEGACY
                } else {
                    TransportType.BT_EXTENDED
                }

                // Parse using ORIP
                viewModel.processPacket(bytes, result.rssi, transport)
            }
        }

        override fun onBatchScanResults(results: MutableList<ScanResult>) {
            results.forEach { result ->
                onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES, result)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            val message = when (errorCode) {
                SCAN_FAILED_ALREADY_STARTED -> "Scan already started"
                SCAN_FAILED_APPLICATION_REGISTRATION_FAILED -> "App registration failed"
                SCAN_FAILED_FEATURE_UNSUPPORTED -> "BLE scan not supported"
                SCAN_FAILED_INTERNAL_ERROR -> "Internal error"
                else -> "Scan failed: $errorCode"
            }
            Toast.makeText(this@MainActivity, message, Toast.LENGTH_SHORT).show()
            isScanning = false
            updateScanUI(false)
        }
    }

    override fun onPause() {
        super.onPause()
        if (isScanning) {
            stopScanning()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        viewModel.cleanup()
    }
}

#!/usr/bin/env python3
"""
ORIP Python Example - Drone Scanner Demo

This example demonstrates using the ORIP library to detect and track drones.
"""

from orip import RemoteIDParser, TransportType, UAVObject


def on_new_drone(uav: UAVObject):
    """Callback for new drone detection."""
    print(f"\n[NEW] Drone detected!")
    print(f"  ID: {uav.id}")
    print(f"  Type: {uav.uav_type.name}")
    print(f"  RSSI: {uav.rssi} dBm")


def on_drone_update(uav: UAVObject):
    """Callback for drone updates."""
    if uav.location.valid:
        print(f"[UPDATE] {uav.id}: "
              f"({uav.location.latitude:.6f}, {uav.location.longitude:.6f}) "
              f"Alt: {uav.location.altitude_baro:.1f}m")


def main():
    print(f"ORIP Python Demo v{RemoteIDParser.version()}")
    print("=" * 40)

    # Create parser with callbacks
    with RemoteIDParser() as parser:
        parser.set_on_new_uav(on_new_drone)
        parser.set_on_uav_update(on_drone_update)

        # Simulated BLE advertisement with ODID Basic ID message
        sample_advertisement = bytes([
            # AD Structure header
            0x1E,        # Length (30 bytes)
            0x16,        # AD Type: Service Data - 16-bit UUID
            0xFA, 0xFF,  # UUID: 0xFFFA (ASTM Remote ID)
            0x00,        # Message counter

            # Basic ID Message (25 bytes)
            0x02,        # Message type (0x0) | Proto version (0x2)
            0x12,        # ID type: Serial (1) | UA type: Multirotor (2)
            # Serial number: "DJI1234567890ABCD"
            ord('D'), ord('J'), ord('I'), ord('1'), ord('2'),
            ord('3'), ord('4'), ord('5'), ord('6'), ord('7'),
            ord('8'), ord('9'), ord('0'), ord('A'), ord('B'),
            ord('C'), ord('D'), 0, 0, 0,
            0, 0, 0, 0   # Padding
        ])

        print("\nParsing sample advertisement...")

        result = parser.parse(
            sample_advertisement,
            rssi=-65,
            transport=TransportType.BT_LEGACY
        )

        if result.success:
            print(f"\n[SUCCESS] Remote ID detected!")
            print(f"  Protocol: {result.protocol.name}")
            print(f"  UAV ID: {result.uav.id}")
            print(f"  UAV Type: {result.uav.uav_type.name}")
            print(f"  ID Type: {result.uav.id_type.name}")
        else:
            print(f"[FAILED] {result.error}")

        print(f"\nActive UAVs: {parser.get_active_count()}")

        # List all active drones
        for uav in parser.get_active_uavs():
            print(f"  - {uav.id} (RSSI: {uav.rssi} dBm, messages: {uav.message_count})")

    print("\nDone.")


if __name__ == "__main__":
    main()

"""
Tests for the ORIP Python library.

Run with: pytest python/tests/
"""

import pytest
from orip import (
    RemoteIDParser,
    TransportType,
    ProtocolType,
    UAVIdType,
    UAVType,
    ParserConfig,
)


def create_basic_id_advertisement(serial: str) -> bytes:
    """Create a BLE advertisement with ODID Basic ID message."""
    # AD structure: [length][type 0x16][UUID low][UUID high][counter][message...]
    message = bytearray(25)
    message[0] = 0x02  # Message type (0x0) | Proto version (0x2)
    message[1] = 0x12  # ID type: Serial (1) | UA type: Multirotor (2)

    # Serial number (20 bytes)
    serial_bytes = serial.encode("utf-8")[:20]
    for i, b in enumerate(serial_bytes):
        message[2 + i] = b

    # Build advertisement
    adv = bytearray()
    adv.append(30)     # Length
    adv.append(0x16)   # Service Data AD type
    adv.append(0xFA)   # UUID low (0xFFFA)
    adv.append(0xFF)   # UUID high
    adv.append(0x00)   # Message counter
    adv.extend(message)

    return bytes(adv)


class TestRemoteIDParser:
    """Tests for RemoteIDParser class."""

    def test_create_parser(self):
        """Test parser creation."""
        parser = RemoteIDParser()
        assert parser is not None
        parser.close()

    def test_context_manager(self):
        """Test using parser as context manager."""
        with RemoteIDParser() as parser:
            assert parser.get_active_count() == 0

    def test_version(self):
        """Test getting library version."""
        version = RemoteIDParser.version()
        assert version == "0.1.0"

    def test_parse_basic_id(self):
        """Test parsing a Basic ID message."""
        with RemoteIDParser() as parser:
            adv = create_basic_id_advertisement("TEST123")
            result = parser.parse(adv, rssi=-70, transport=TransportType.BT_LEGACY)

            assert result.success
            assert result.is_remote_id
            assert result.protocol == ProtocolType.ASTM_F3411
            assert result.uav is not None
            assert result.uav.id == "TEST123"
            assert result.uav.id_type == UAVIdType.SERIAL_NUMBER
            assert result.uav.uav_type == UAVType.HELICOPTER_OR_MULTIROTOR
            assert result.uav.rssi == -70

    def test_parse_invalid_payload(self):
        """Test parsing invalid payload."""
        with RemoteIDParser() as parser:
            result = parser.parse(b"\x01\x02\x03", rssi=-50)

            assert not result.success
            assert not result.is_remote_id

    def test_active_uav_tracking(self):
        """Test tracking of active UAVs."""
        with RemoteIDParser() as parser:
            assert parser.get_active_count() == 0

            adv1 = create_basic_id_advertisement("UAV001")
            adv2 = create_basic_id_advertisement("UAV002")

            parser.parse(adv1, rssi=-60)
            assert parser.get_active_count() == 1

            parser.parse(adv2, rssi=-70)
            assert parser.get_active_count() == 2

    def test_get_active_uavs(self):
        """Test getting list of active UAVs."""
        with RemoteIDParser() as parser:
            adv1 = create_basic_id_advertisement("DRONE_A")
            adv2 = create_basic_id_advertisement("DRONE_B")

            parser.parse(adv1, rssi=-60)
            parser.parse(adv2, rssi=-70)

            uavs = parser.get_active_uavs()
            assert len(uavs) == 2

            ids = {uav.id for uav in uavs}
            assert "DRONE_A" in ids
            assert "DRONE_B" in ids

    def test_get_uav_by_id(self):
        """Test getting specific UAV by ID."""
        with RemoteIDParser() as parser:
            adv = create_basic_id_advertisement("FINDME")
            parser.parse(adv, rssi=-55)

            uav = parser.get_uav("FINDME")
            assert uav is not None
            assert uav.id == "FINDME"
            assert uav.rssi == -55

            not_found = parser.get_uav("NOTEXIST")
            assert not_found is None

    def test_clear(self):
        """Test clearing all UAVs."""
        with RemoteIDParser() as parser:
            adv = create_basic_id_advertisement("TEMP")
            parser.parse(adv, rssi=-60)
            assert parser.get_active_count() == 1

            parser.clear()
            assert parser.get_active_count() == 0

    def test_custom_config(self):
        """Test creating parser with custom config."""
        config = ParserConfig(
            uav_timeout_ms=60000,
            enable_deduplication=False,
        )
        with RemoteIDParser(config) as parser:
            adv = create_basic_id_advertisement("CUSTOM")
            result = parser.parse(adv, rssi=-60)
            assert result.success

    def test_callback_new_uav(self):
        """Test new UAV callback."""
        detected = []

        def on_new(uav):
            detected.append(uav.id)

        with RemoteIDParser() as parser:
            parser.set_on_new_uav(on_new)

            adv = create_basic_id_advertisement("CALLBACK_TEST")
            parser.parse(adv, rssi=-60)

            assert len(detected) == 1
            assert detected[0] == "CALLBACK_TEST"

            # Clear callback
            parser.set_on_new_uav(None)


class TestTypes:
    """Tests for type enums and dataclasses."""

    def test_protocol_type_values(self):
        """Test ProtocolType enum values."""
        assert ProtocolType.UNKNOWN == 0
        assert ProtocolType.ASTM_F3411 == 1
        assert ProtocolType.ASD_STAN == 2
        assert ProtocolType.CN_RID == 3

    def test_transport_type_values(self):
        """Test TransportType enum values."""
        assert TransportType.UNKNOWN == 0
        assert TransportType.BT_LEGACY == 1
        assert TransportType.BT_EXTENDED == 2
        assert TransportType.WIFI_BEACON == 3
        assert TransportType.WIFI_NAN == 4

    def test_uav_type_values(self):
        """Test UAVType enum values."""
        assert UAVType.NONE == 0
        assert UAVType.HELICOPTER_OR_MULTIROTOR == 2
        assert UAVType.OTHER == 15

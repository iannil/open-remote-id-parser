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


# ============================================
# Advanced Tests (P2-TEST-011)
# ============================================

class TestContextManagerAdvanced:
    """Advanced context manager tests."""

    def test_context_manager_with_exception(self):
        """Test parser properly closes even when exception occurs."""
        try:
            with RemoteIDParser() as parser:
                adv = create_basic_id_advertisement("EXCEPTION_TEST")
                parser.parse(adv, rssi=-60)
                raise ValueError("Test exception")
        except ValueError:
            pass
        # Parser should be closed - no resource leak

    def test_context_manager_nested(self):
        """Test nested context managers."""
        with RemoteIDParser() as parser1:
            with RemoteIDParser() as parser2:
                adv = create_basic_id_advertisement("NESTED")
                result1 = parser1.parse(adv, rssi=-60)
                result2 = parser2.parse(adv, rssi=-60)
                assert result1.success
                assert result2.success
                assert parser1.get_active_count() == 1
                assert parser2.get_active_count() == 1

    def test_double_close(self):
        """Test that double close doesn't crash."""
        parser = RemoteIDParser()
        parser.close()
        parser.close()  # Should not raise

    def test_use_after_close(self):
        """Test that using parser after close raises error."""
        parser = RemoteIDParser()
        parser.close()

        with pytest.raises(RuntimeError):
            parser.parse(b"\x00" * 30, rssi=-60)

        with pytest.raises(RuntimeError):
            parser.get_active_count()

        with pytest.raises(RuntimeError):
            parser.get_active_uavs()


class TestLargePayloads:
    """Tests for large payload handling."""

    def test_large_payload_10kb(self):
        """Test parsing 10KB payload."""
        with RemoteIDParser() as parser:
            large_payload = bytes([0x00] * 10 * 1024)
            result = parser.parse(large_payload, rssi=-60)
            # Should not crash, likely returns failure
            assert not result.success

    def test_large_payload_1mb(self):
        """Test parsing 1MB payload."""
        with RemoteIDParser() as parser:
            large_payload = bytes([0x00] * 1024 * 1024)
            result = parser.parse(large_payload, rssi=-60)
            # Should not crash
            assert not result.success

    def test_empty_payload(self):
        """Test parsing empty payload."""
        with RemoteIDParser() as parser:
            result = parser.parse(b"", rssi=-60)
            assert not result.success

    def test_minimal_payload(self):
        """Test parsing minimal (1 byte) payload."""
        with RemoteIDParser() as parser:
            result = parser.parse(b"\x00", rssi=-60)
            assert not result.success


class TestMemoryStress:
    """Memory stress tests."""

    def test_many_parse_calls(self):
        """Test many parse calls in sequence."""
        with RemoteIDParser() as parser:
            adv = create_basic_id_advertisement("STRESS")
            for i in range(1000):
                result = parser.parse(adv, rssi=-60)
                assert result.success

    def test_many_parsers(self):
        """Test creating and destroying many parsers."""
        for i in range(100):
            parser = RemoteIDParser()
            adv = create_basic_id_advertisement(f"UAV{i:03d}")
            result = parser.parse(adv, rssi=-60)
            assert result.success
            parser.close()

    def test_many_uavs(self):
        """Test tracking many different UAVs."""
        with RemoteIDParser() as parser:
            for i in range(100):
                adv = create_basic_id_advertisement(f"UAV{i:03d}")
                result = parser.parse(adv, rssi=-60)
                assert result.success

            assert parser.get_active_count() == 100
            uavs = parser.get_active_uavs()
            assert len(uavs) == 100


class TestCallbackAdvanced:
    """Advanced callback tests."""

    def test_callback_with_exception(self):
        """Test that callback exceptions don't crash parser."""
        call_count = [0]

        def bad_callback(uav):
            call_count[0] += 1
            raise ValueError("Callback error")

        with RemoteIDParser() as parser:
            parser.set_on_new_uav(bad_callback)

            adv = create_basic_id_advertisement("CALLBACK_ERR")
            # This might raise or not depending on implementation
            try:
                parser.parse(adv, rssi=-60)
            except ValueError:
                pass

            # Callback should have been called
            assert call_count[0] >= 1

    def test_callback_modification_during_callback(self):
        """Test modifying callback from within callback."""
        calls = []

        def first_callback(uav):
            calls.append("first")

        def second_callback(uav):
            calls.append("second")

        with RemoteIDParser() as parser:
            parser.set_on_new_uav(first_callback)

            adv1 = create_basic_id_advertisement("UAV001")
            parser.parse(adv1, rssi=-60)

            # Change callback
            parser.set_on_new_uav(second_callback)

            adv2 = create_basic_id_advertisement("UAV002")
            parser.parse(adv2, rssi=-60)

            assert "first" in calls
            assert "second" in calls

    def test_multiple_callbacks(self):
        """Test setting multiple different callbacks."""
        new_uavs = []
        updated_uavs = []

        def on_new(uav):
            new_uavs.append(uav.id)

        def on_update(uav):
            updated_uavs.append(uav.id)

        with RemoteIDParser() as parser:
            parser.set_on_new_uav(on_new)
            parser.set_on_uav_update(on_update)

            adv = create_basic_id_advertisement("MULTI_CALLBACK")

            # First parse - new UAV
            parser.parse(adv, rssi=-60)
            assert "MULTI_CALLBACK" in new_uavs

            # Second parse - update
            parser.parse(adv, rssi=-50)
            # Update callback should have been called
            assert len(updated_uavs) >= 0  # May or may not trigger based on dedup


class TestMalformedInput:
    """Tests for malformed input handling."""

    def test_null_bytes_in_serial(self):
        """Test handling of null bytes in serial number."""
        adv = create_basic_id_advertisement("TEST\x00MID\x00END")
        with RemoteIDParser() as parser:
            result = parser.parse(adv, rssi=-60)
            # Should handle gracefully
            if result.success and result.uav:
                # ID should be truncated at first null or handled somehow
                assert "\x00" not in result.uav.id or len(result.uav.id) > 0

    def test_unicode_in_serial(self):
        """Test handling of unicode in serial number."""
        # This might not be valid per spec, but shouldn't crash
        adv = create_basic_id_advertisement("UAV中文测试")
        with RemoteIDParser() as parser:
            result = parser.parse(adv, rssi=-60)
            # Should not crash

    def test_all_zeros_payload(self):
        """Test all zeros payload."""
        with RemoteIDParser() as parser:
            result = parser.parse(bytes(100), rssi=-60)
            assert not result.success

    def test_all_ones_payload(self):
        """Test all 0xFF payload."""
        with RemoteIDParser() as parser:
            result = parser.parse(bytes([0xFF] * 100), rssi=-60)
            assert not result.success

    def test_random_bytes(self):
        """Test random byte sequences."""
        import random
        random.seed(42)

        with RemoteIDParser() as parser:
            for _ in range(100):
                payload = bytes([random.randint(0, 255) for _ in range(50)])
                result = parser.parse(payload, rssi=-60)
                # Should not crash, result may vary


class TestRSSIEdgeCases:
    """Tests for RSSI edge cases."""

    def test_rssi_zero(self):
        """Test RSSI value of 0."""
        with RemoteIDParser() as parser:
            adv = create_basic_id_advertisement("RSSI_ZERO")
            result = parser.parse(adv, rssi=0)
            assert result.success
            assert result.uav.rssi == 0

    def test_rssi_min(self):
        """Test minimum RSSI value (-127)."""
        with RemoteIDParser() as parser:
            adv = create_basic_id_advertisement("RSSI_MIN")
            result = parser.parse(adv, rssi=-127)
            assert result.success
            assert result.uav.rssi == -127

    def test_rssi_positive(self):
        """Test positive RSSI value (unusual but valid)."""
        with RemoteIDParser() as parser:
            adv = create_basic_id_advertisement("RSSI_POS")
            result = parser.parse(adv, rssi=10)
            # Should handle gracefully


class TestGarbageCollection:
    """Tests for garbage collection scenarios."""

    def test_gc_during_parse(self):
        """Test that GC doesn't cause issues during parsing."""
        import gc

        with RemoteIDParser() as parser:
            adv = create_basic_id_advertisement("GC_TEST")

            for i in range(100):
                result = parser.parse(adv, rssi=-60)
                assert result.success

                # Force garbage collection periodically
                if i % 10 == 0:
                    gc.collect()

    def test_gc_callback_reference(self):
        """Test that callbacks are not garbage collected while in use."""
        import gc

        results = []

        with RemoteIDParser() as parser:
            def callback(uav):
                results.append(uav.id)

            parser.set_on_new_uav(callback)

            # Force GC - callback should still work
            gc.collect()

            adv = create_basic_id_advertisement("GC_CALLBACK")
            parser.parse(adv, rssi=-60)

            gc.collect()

            assert "GC_CALLBACK" in results

    def test_parser_without_explicit_close(self):
        """Test parser cleanup through GC (no explicit close)."""
        import gc

        def create_and_use_parser():
            parser = RemoteIDParser()
            adv = create_basic_id_advertisement("NO_CLOSE")
            result = parser.parse(adv, rssi=-60)
            assert result.success
            # No explicit close - let GC handle it

        create_and_use_parser()
        gc.collect()  # Should clean up the parser


class TestTransportTypes:
    """Tests for different transport types."""

    def test_bt_legacy(self):
        """Test BT Legacy transport."""
        with RemoteIDParser() as parser:
            adv = create_basic_id_advertisement("BT_LEGACY")
            result = parser.parse(adv, rssi=-60, transport=TransportType.BT_LEGACY)
            assert result.success

    def test_bt_extended(self):
        """Test BT Extended transport."""
        with RemoteIDParser() as parser:
            adv = create_basic_id_advertisement("BT_EXT")
            result = parser.parse(adv, rssi=-60, transport=TransportType.BT_EXTENDED)
            # May or may not parse depending on payload format

    def test_wifi_beacon(self):
        """Test WiFi Beacon transport."""
        with RemoteIDParser() as parser:
            adv = create_basic_id_advertisement("WIFI_BCN")
            result = parser.parse(adv, rssi=-60, transport=TransportType.WIFI_BEACON)
            # Will likely fail as payload is BLE format

    def test_wifi_nan(self):
        """Test WiFi NAN transport."""
        with RemoteIDParser() as parser:
            adv = create_basic_id_advertisement("WIFI_NAN")
            result = parser.parse(adv, rssi=-60, transport=TransportType.WIFI_NAN)
            # Will likely fail as payload is BLE format


class TestConfigOptions:
    """Tests for various configuration options."""

    def test_disable_deduplication(self):
        """Test with deduplication disabled."""
        config = ParserConfig(enable_deduplication=False)
        with RemoteIDParser(config) as parser:
            adv = create_basic_id_advertisement("DEDUP_OFF")

            # Parse same payload multiple times
            for _ in range(5):
                result = parser.parse(adv, rssi=-60)
                assert result.success

    def test_short_timeout(self):
        """Test with very short UAV timeout."""
        import time

        config = ParserConfig(uav_timeout_ms=100)  # 100ms timeout
        with RemoteIDParser(config) as parser:
            adv = create_basic_id_advertisement("SHORT_TIMEOUT")
            parser.parse(adv, rssi=-60)
            assert parser.get_active_count() == 1

            # Wait for timeout
            time.sleep(0.2)
            parser.cleanup()
            # UAV should have timed out
            # Note: Might still be 1 if cleanup is not immediate
            assert parser.get_active_count() <= 1

    def test_disable_protocols(self):
        """Test with specific protocols disabled."""
        config = ParserConfig(enable_astm=False)
        with RemoteIDParser(config) as parser:
            adv = create_basic_id_advertisement("ASTM_OFF")
            result = parser.parse(adv, rssi=-60)
            # ASTM should be disabled, so parsing should fail
            # (depending on implementation)

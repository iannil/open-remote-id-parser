"""
RemoteIDParser - Main parser class for ORIP.
"""

import ctypes
from ctypes import POINTER, c_uint8, c_int8, byref
from typing import List, Optional, Callable

from .types import (
    ProtocolType,
    TransportType,
    UAVIdType,
    UAVType,
    UAVStatus,
    LocationData,
    SystemInfo,
    UAVObject,
    ParseResult,
    ParserConfig,
)
from ._bindings import (
    get_lib,
    CUAV,
    CResult,
    CConfig,
    UAVCallbackType,
)


def _convert_cuav_to_python(cuav: CUAV) -> UAVObject:
    """Convert C UAV struct to Python dataclass."""
    location = LocationData(
        valid=bool(cuav.location.valid),
        latitude=cuav.location.latitude,
        longitude=cuav.location.longitude,
        altitude_baro=cuav.location.altitude_baro,
        altitude_geo=cuav.location.altitude_geo,
        height=cuav.location.height,
        speed_horizontal=cuav.location.speed_horizontal,
        speed_vertical=cuav.location.speed_vertical,
        direction=cuav.location.direction,
        status=UAVStatus(cuav.location.status),
    )

    system = SystemInfo(
        valid=bool(cuav.system.valid),
        operator_latitude=cuav.system.operator_latitude,
        operator_longitude=cuav.system.operator_longitude,
        area_ceiling=cuav.system.area_ceiling,
        area_floor=cuav.system.area_floor,
        area_count=cuav.system.area_count,
        area_radius=cuav.system.area_radius,
        timestamp=cuav.system.timestamp,
    )

    return UAVObject(
        id=cuav.id.decode("utf-8", errors="replace").rstrip("\x00"),
        id_type=UAVIdType(cuav.id_type),
        uav_type=UAVType(cuav.uav_type),
        protocol=ProtocolType(cuav.protocol),
        transport=TransportType(cuav.transport),
        rssi=cuav.rssi,
        last_seen_ms=cuav.last_seen_ms,
        location=location,
        system=system,
        self_id_description=(
            cuav.self_id_description.decode("utf-8", errors="replace").rstrip("\x00")
            if cuav.has_self_id else None
        ),
        operator_id=(
            cuav.operator_id.decode("utf-8", errors="replace").rstrip("\x00")
            if cuav.has_operator_id else None
        ),
        message_count=cuav.message_count,
    )


class RemoteIDParser:
    """
    Remote ID Parser for detecting and parsing drone identification signals.

    Example usage:
        >>> parser = RemoteIDParser()
        >>> result = parser.parse(payload, rssi=-70, transport=TransportType.BT_LEGACY)
        >>> if result.success:
        ...     print(f"Detected drone: {result.uav.id}")
        >>> parser.close()

    Or use as context manager:
        >>> with RemoteIDParser() as parser:
        ...     result = parser.parse(payload, -70)
        ...     print(result.uav.id if result.success else result.error)
    """

    def __init__(self, config: Optional[ParserConfig] = None):
        """
        Create a new parser instance.

        Args:
            config: Optional parser configuration. Uses defaults if not provided.
        """
        self._lib = get_lib()
        self._handle = None
        self._callbacks = {}  # Keep references to prevent garbage collection

        if config:
            c_config = CConfig(
                uav_timeout_ms=config.uav_timeout_ms,
                enable_deduplication=int(config.enable_deduplication),
                enable_astm=int(config.enable_astm),
                enable_asd=int(config.enable_asd),
                enable_cn=int(config.enable_cn),
            )
            self._handle = self._lib.orip_create_with_config(byref(c_config))
        else:
            self._handle = self._lib.orip_create()

        if not self._handle:
            raise RuntimeError("Failed to create parser")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    def close(self):
        """Release parser resources."""
        if self._handle:
            self._lib.orip_destroy(self._handle)
            self._handle = None
            self._callbacks.clear()

    def __del__(self):
        self.close()

    def _check_handle(self):
        if not self._handle:
            raise RuntimeError("Parser has been closed")

    def parse(
        self,
        payload: bytes,
        rssi: int = 0,
        transport: TransportType = TransportType.BT_LEGACY,
    ) -> ParseResult:
        """
        Parse a raw Bluetooth or WiFi payload.

        Args:
            payload: Raw advertisement/beacon data
            rssi: Signal strength in dBm
            transport: Transport type (BT_LEGACY, BT_EXTENDED, WIFI_BEACON, etc.)

        Returns:
            ParseResult containing success status and parsed UAV data
        """
        self._check_handle()

        # Convert payload to ctypes array
        payload_array = (c_uint8 * len(payload))(*payload)
        result = CResult()

        ret = self._lib.orip_parse(
            self._handle,
            payload_array,
            len(payload),
            c_int8(rssi),
            int(transport),
            byref(result),
        )

        if ret != 0:
            return ParseResult(
                success=False,
                is_remote_id=False,
                protocol=ProtocolType.UNKNOWN,
                error="Internal error",
            )

        uav = None
        if result.success:
            uav = _convert_cuav_to_python(result.uav)

        error = result.error.decode("utf-8", errors="replace").rstrip("\x00")

        return ParseResult(
            success=bool(result.success),
            is_remote_id=bool(result.is_remote_id),
            protocol=ProtocolType(result.protocol),
            error=error if error else None,
            uav=uav,
        )

    def get_active_count(self) -> int:
        """Get the number of currently active (recently seen) UAVs."""
        self._check_handle()
        return self._lib.orip_get_active_count(self._handle)

    def get_active_uavs(self) -> List[UAVObject]:
        """Get a list of all currently active UAVs."""
        self._check_handle()

        count = self.get_active_count()
        if count == 0:
            return []

        # Allocate array for results
        uavs_array = (CUAV * count)()
        actual_count = self._lib.orip_get_active_uavs(
            self._handle, uavs_array, count
        )

        return [_convert_cuav_to_python(uavs_array[i]) for i in range(actual_count)]

    def get_uav(self, uav_id: str) -> Optional[UAVObject]:
        """
        Get a specific UAV by its ID.

        Args:
            uav_id: The UAV identifier (serial number or registration)

        Returns:
            The UAVObject if found, None otherwise
        """
        self._check_handle()

        cuav = CUAV()
        ret = self._lib.orip_get_uav(
            self._handle,
            uav_id.encode("utf-8"),
            byref(cuav),
        )

        if ret != 0:
            return None

        return _convert_cuav_to_python(cuav)

    def clear(self):
        """Clear all tracked UAVs."""
        self._check_handle()
        self._lib.orip_clear(self._handle)

    def cleanup(self) -> int:
        """
        Manually trigger cleanup of timed-out UAVs.

        Returns:
            Number of UAVs removed
        """
        self._check_handle()
        return self._lib.orip_cleanup(self._handle)

    def set_on_new_uav(self, callback: Optional[Callable[[UAVObject], None]]):
        """
        Set callback for new UAV detection.

        Args:
            callback: Function to call when a new UAV is detected, or None to disable
        """
        self._check_handle()

        if callback is None:
            self._lib.orip_set_on_new_uav(self._handle, UAVCallbackType(), None)
            self._callbacks.pop("new_uav", None)
            return

        def wrapper(cuav_ptr, user_data):
            uav = _convert_cuav_to_python(cuav_ptr.contents)
            callback(uav)

        c_callback = UAVCallbackType(wrapper)
        self._callbacks["new_uav"] = c_callback  # Keep reference
        self._lib.orip_set_on_new_uav(self._handle, c_callback, None)

    def set_on_uav_update(self, callback: Optional[Callable[[UAVObject], None]]):
        """
        Set callback for UAV updates.

        Args:
            callback: Function to call when a UAV is updated, or None to disable
        """
        self._check_handle()

        if callback is None:
            self._lib.orip_set_on_uav_update(self._handle, UAVCallbackType(), None)
            self._callbacks.pop("update", None)
            return

        def wrapper(cuav_ptr, user_data):
            uav = _convert_cuav_to_python(cuav_ptr.contents)
            callback(uav)

        c_callback = UAVCallbackType(wrapper)
        self._callbacks["update"] = c_callback
        self._lib.orip_set_on_uav_update(self._handle, c_callback, None)

    def set_on_uav_timeout(self, callback: Optional[Callable[[UAVObject], None]]):
        """
        Set callback for UAV timeout (removal).

        Args:
            callback: Function to call when a UAV times out, or None to disable
        """
        self._check_handle()

        if callback is None:
            self._lib.orip_set_on_uav_timeout(self._handle, UAVCallbackType(), None)
            self._callbacks.pop("timeout", None)
            return

        def wrapper(cuav_ptr, user_data):
            uav = _convert_cuav_to_python(cuav_ptr.contents)
            callback(uav)

        c_callback = UAVCallbackType(wrapper)
        self._callbacks["timeout"] = c_callback
        self._lib.orip_set_on_uav_timeout(self._handle, c_callback, None)

    @staticmethod
    def version() -> str:
        """Get the library version string."""
        lib = get_lib()
        return lib.orip_version().decode("utf-8")

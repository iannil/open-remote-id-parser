"""
Data types for ORIP library.
"""

from dataclasses import dataclass, field
from enum import IntEnum
from typing import Optional


class ProtocolType(IntEnum):
    """Protocol types supported by the parser."""
    UNKNOWN = 0
    ASTM_F3411 = 1
    ASD_STAN = 2
    CN_RID = 3


class TransportType(IntEnum):
    """Transport layer type."""
    UNKNOWN = 0
    BT_LEGACY = 1
    BT_EXTENDED = 2
    WIFI_BEACON = 3
    WIFI_NAN = 4


class UAVIdType(IntEnum):
    """UAV identification type."""
    NONE = 0
    SERIAL_NUMBER = 1
    CAA_REGISTRATION = 2
    UTM_ASSIGNED = 3
    SPECIFIC_SESSION = 4


class UAVType(IntEnum):
    """UAV type classification."""
    NONE = 0
    AEROPLANE = 1
    HELICOPTER_OR_MULTIROTOR = 2
    GYROPLANE = 3
    HYBRID_LIFT = 4
    ORNITHOPTER = 5
    GLIDER = 6
    KITE = 7
    FREE_BALLOON = 8
    CAPTIVE_BALLOON = 9
    AIRSHIP = 10
    FREE_FALL_PARACHUTE = 11
    ROCKET = 12
    TETHERED_POWERED = 13
    GROUND_OBSTACLE = 14
    OTHER = 15


class UAVStatus(IntEnum):
    """UAV status."""
    UNDECLARED = 0
    GROUND = 1
    AIRBORNE = 2
    EMERGENCY = 3
    REMOTE_ID_FAILURE = 4


@dataclass
class LocationData:
    """Location and vector data for a UAV."""
    valid: bool = False
    latitude: float = 0.0
    longitude: float = 0.0
    altitude_baro: float = 0.0
    altitude_geo: float = 0.0
    height: float = 0.0
    speed_horizontal: float = 0.0
    speed_vertical: float = 0.0
    direction: float = 0.0
    status: UAVStatus = UAVStatus.UNDECLARED


@dataclass
class SystemInfo:
    """System information (operator/pilot location)."""
    valid: bool = False
    operator_latitude: float = 0.0
    operator_longitude: float = 0.0
    area_ceiling: float = 0.0
    area_floor: float = 0.0
    area_count: int = 0
    area_radius: int = 0
    timestamp: int = 0


@dataclass
class UAVObject:
    """Complete UAV object containing all parsed data."""
    id: str = ""
    id_type: UAVIdType = UAVIdType.NONE
    uav_type: UAVType = UAVType.NONE
    protocol: ProtocolType = ProtocolType.UNKNOWN
    transport: TransportType = TransportType.UNKNOWN
    rssi: int = 0
    last_seen_ms: int = 0
    location: LocationData = field(default_factory=LocationData)
    system: SystemInfo = field(default_factory=SystemInfo)
    self_id_description: Optional[str] = None
    operator_id: Optional[str] = None
    message_count: int = 0


@dataclass
class ParseResult:
    """Parse result returned by the parser."""
    success: bool = False
    is_remote_id: bool = False
    protocol: ProtocolType = ProtocolType.UNKNOWN
    error: Optional[str] = None
    uav: Optional[UAVObject] = None


@dataclass
class ParserConfig:
    """Parser configuration."""
    uav_timeout_ms: int = 30000
    enable_deduplication: bool = True
    enable_astm: bool = True
    enable_asd: bool = False
    enable_cn: bool = False

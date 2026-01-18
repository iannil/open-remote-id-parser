"""
ctypes bindings for the ORIP C library.
"""

import ctypes
from ctypes import (
    Structure, POINTER, CFUNCTYPE,
    c_int, c_int8, c_uint8, c_uint16, c_uint32, c_uint64,
    c_float, c_double, c_char, c_char_p, c_size_t, c_void_p,
)
import os
import sys
from pathlib import Path


# Constants matching orip_c.h
ORIP_MAX_ID_LENGTH = 64
ORIP_MAX_DESCRIPTION_LENGTH = 64


class CLocation(Structure):
    """C struct: orip_location_t"""
    _fields_ = [
        ("valid", c_int),
        ("latitude", c_double),
        ("longitude", c_double),
        ("altitude_baro", c_float),
        ("altitude_geo", c_float),
        ("height", c_float),
        ("speed_horizontal", c_float),
        ("speed_vertical", c_float),
        ("direction", c_float),
        ("status", c_int),
    ]


class CSystemInfo(Structure):
    """C struct: orip_system_info_t"""
    _fields_ = [
        ("valid", c_int),
        ("operator_latitude", c_double),
        ("operator_longitude", c_double),
        ("area_ceiling", c_float),
        ("area_floor", c_float),
        ("area_count", c_uint16),
        ("area_radius", c_uint16),
        ("timestamp", c_uint32),
    ]


class CUAV(Structure):
    """C struct: orip_uav_t"""
    _fields_ = [
        ("id", c_char * ORIP_MAX_ID_LENGTH),
        ("id_type", c_int),
        ("uav_type", c_int),
        ("protocol", c_int),
        ("transport", c_int),
        ("rssi", c_int8),
        ("last_seen_ms", c_uint64),
        ("location", CLocation),
        ("system", CSystemInfo),
        ("has_self_id", c_int),
        ("self_id_description", c_char * ORIP_MAX_DESCRIPTION_LENGTH),
        ("has_operator_id", c_int),
        ("operator_id", c_char * ORIP_MAX_ID_LENGTH),
        ("message_count", c_uint32),
    ]


class CResult(Structure):
    """C struct: orip_result_t"""
    _fields_ = [
        ("success", c_int),
        ("is_remote_id", c_int),
        ("protocol", c_int),
        ("error", c_char * 128),
        ("uav", CUAV),
    ]


class CConfig(Structure):
    """C struct: orip_config_t"""
    _fields_ = [
        ("uav_timeout_ms", c_uint32),
        ("enable_deduplication", c_int),
        ("enable_astm", c_int),
        ("enable_asd", c_int),
        ("enable_cn", c_int),
    ]


# Callback type
UAVCallbackType = CFUNCTYPE(None, POINTER(CUAV), c_void_p)


def _find_library() -> str:
    """Find the orip shared library."""
    lib_name = {
        "linux": "liborip.so",
        "darwin": "liborip.dylib",
        "win32": "orip.dll",
    }.get(sys.platform, "liborip.so")

    # Search paths
    search_paths = [
        # Same directory as this module
        Path(__file__).parent,
        # Package lib directory
        Path(__file__).parent / "lib",
        # Project build directory
        Path(__file__).parent.parent.parent / "build",
        # System paths
        Path("/usr/local/lib"),
        Path("/usr/lib"),
    ]

    # Also check LD_LIBRARY_PATH / DYLD_LIBRARY_PATH
    env_paths = os.environ.get(
        "DYLD_LIBRARY_PATH" if sys.platform == "darwin" else "LD_LIBRARY_PATH",
        ""
    )
    for p in env_paths.split(":"):
        if p:
            search_paths.append(Path(p))

    for path in search_paths:
        lib_path = path / lib_name
        if lib_path.exists():
            return str(lib_path)

    # Try loading by name (system search)
    return lib_name


class ORIPLib:
    """Wrapper for the ORIP C library."""

    _instance = None
    _lib = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._load_library()
        return cls._instance

    def _load_library(self):
        """Load the shared library and set up function signatures."""
        lib_path = _find_library()
        self._lib = ctypes.CDLL(lib_path)

        # orip_version
        self._lib.orip_version.argtypes = []
        self._lib.orip_version.restype = c_char_p

        # orip_default_config
        self._lib.orip_default_config.argtypes = []
        self._lib.orip_default_config.restype = CConfig

        # orip_create
        self._lib.orip_create.argtypes = []
        self._lib.orip_create.restype = c_void_p

        # orip_create_with_config
        self._lib.orip_create_with_config.argtypes = [POINTER(CConfig)]
        self._lib.orip_create_with_config.restype = c_void_p

        # orip_destroy
        self._lib.orip_destroy.argtypes = [c_void_p]
        self._lib.orip_destroy.restype = None

        # orip_parse
        self._lib.orip_parse.argtypes = [
            c_void_p,           # parser
            POINTER(c_uint8),   # payload
            c_size_t,           # payload_len
            c_int8,             # rssi
            c_int,              # transport
            POINTER(CResult),   # result
        ]
        self._lib.orip_parse.restype = c_int

        # orip_get_active_count
        self._lib.orip_get_active_count.argtypes = [c_void_p]
        self._lib.orip_get_active_count.restype = c_size_t

        # orip_get_active_uavs
        self._lib.orip_get_active_uavs.argtypes = [
            c_void_p,       # parser
            POINTER(CUAV),  # uavs
            c_size_t,       # max_count
        ]
        self._lib.orip_get_active_uavs.restype = c_size_t

        # orip_get_uav
        self._lib.orip_get_uav.argtypes = [c_void_p, c_char_p, POINTER(CUAV)]
        self._lib.orip_get_uav.restype = c_int

        # orip_clear
        self._lib.orip_clear.argtypes = [c_void_p]
        self._lib.orip_clear.restype = None

        # orip_cleanup
        self._lib.orip_cleanup.argtypes = [c_void_p]
        self._lib.orip_cleanup.restype = c_size_t

        # orip_set_on_new_uav
        self._lib.orip_set_on_new_uav.argtypes = [c_void_p, UAVCallbackType, c_void_p]
        self._lib.orip_set_on_new_uav.restype = None

        # orip_set_on_uav_update
        self._lib.orip_set_on_uav_update.argtypes = [c_void_p, UAVCallbackType, c_void_p]
        self._lib.orip_set_on_uav_update.restype = None

        # orip_set_on_uav_timeout
        self._lib.orip_set_on_uav_timeout.argtypes = [c_void_p, UAVCallbackType, c_void_p]
        self._lib.orip_set_on_uav_timeout.restype = None

    @property
    def lib(self):
        return self._lib


# Global library instance
def get_lib():
    """Get the ORIP library instance."""
    return ORIPLib().lib

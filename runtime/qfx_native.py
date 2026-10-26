from __future__ import annotations

import ctypes
import os
from pathlib import Path


class NativeEvent(ctypes.Structure):
    _fields_ = [
        ("timestamp_ns", ctypes.c_uint64),
        ("symbol", ctypes.c_uint64),
        ("bid", ctypes.c_int64),
        ("ask", ctypes.c_int64),
        ("last", ctypes.c_int64),
        ("volume", ctypes.c_uint64),
        ("bid_size", ctypes.c_uint64),
        ("ask_size", ctypes.c_uint64),
        ("sequence", ctypes.c_uint64),
        ("flags", ctypes.c_uint64),
    ]


class NativeOrder(ctypes.Structure):
    _fields_ = [
        ("side", ctypes.c_int64),
        ("quantity", ctypes.c_uint64),
        ("price", ctypes.c_int64),
        ("stop_loss", ctypes.c_int64),
        ("take_profit", ctypes.c_int64),
        ("symbol", ctypes.c_uint64),
        ("client_order_id", ctypes.c_uint64),
    ]


class NativeReport(ctypes.Structure):
    _fields_ = [
        ("events", ctypes.c_uint64),
        ("trades", ctypes.c_uint64),
        ("fills", ctypes.c_uint64),
        ("wins", ctypes.c_uint64),
        ("losses", ctypes.c_uint64),
        ("net_pnl", ctypes.c_int64),
        ("peak_equity", ctypes.c_int64),
        ("max_drawdown", ctypes.c_int64),
        ("position", ctypes.c_int64),
        ("cash", ctypes.c_int64),
    ]


class QfxNative:
    def __init__(self, lib_path: str | os.PathLike[str] | None = None):
        if lib_path is None:
            root = Path(__file__).resolve().parents[1]
            lib_path = root / "build" / "libqfx.so"
        self.lib = ctypes.CDLL(str(lib_path))
        self._setup()
        self.lib.qfx_c_init()

    def _setup(self) -> None:
        self.lib.qfx_c_init.argtypes = []
        self.lib.qfx_c_init.restype = ctypes.c_int

        self.lib.qfx_c_tsc_now_ns.argtypes = []
        self.lib.qfx_c_tsc_now_ns.restype = ctypes.c_uint64

        self.lib.qfx_c_pin_thread.argtypes = [ctypes.c_int]
        self.lib.qfx_c_pin_thread.restype = ctypes.c_int

        self.lib.qfx_c_l2_sweep.argtypes = [
            ctypes.c_int64,
            ctypes.c_uint64,
            ctypes.POINTER(ctypes.c_int64),
            ctypes.POINTER(ctypes.c_uint64),
        ]
        self.lib.qfx_c_l2_sweep.restype = ctypes.c_int

        self.lib.qfx_c_process_buffer.argtypes = [
            ctypes.POINTER(NativeEvent),
            ctypes.c_size_t,
            ctypes.POINTER(NativeReport),
        ]
        self.lib.qfx_c_process_buffer.restype = ctypes.c_int

        self.lib.qfx_c_sbe_encode_order.argtypes = [
            ctypes.POINTER(NativeOrder),
            ctypes.c_uint64,
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t,
        ]
        self.lib.qfx_c_sbe_encode_order.restype = ctypes.c_int

        self.lib.qfx_c_sbe_decode_order.argtypes = [
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t,
            ctypes.POINTER(NativeOrder),
            ctypes.POINTER(ctypes.c_uint64),
        ]
        self.lib.qfx_c_sbe_decode_order.restype = ctypes.c_int

    def now_ns(self) -> int:
        return int(self.lib.qfx_c_tsc_now_ns())

    def sweep_l2(self, side: int, quantity: int) -> tuple[int, int]:
        out_vwap = ctypes.c_int64(0)
        out_filled = ctypes.c_uint64(0)
        rc = self.lib.qfx_c_l2_sweep(
            side, quantity, ctypes.byref(out_vwap), ctypes.byref(out_filled)
        )
        if rc != 0:
            raise RuntimeError("l2 sweep failed")
        return out_vwap.value, out_filled.value

    def process_events(self, events: list[NativeEvent]) -> NativeReport:
        arr_type = NativeEvent * len(events)
        c_arr = arr_type(*events)
        report = NativeReport()
        rc = self.lib.qfx_c_process_buffer(c_arr, len(events), ctypes.byref(report))
        if rc != 0:
            raise RuntimeError("process buffer failed")
        return report

    def sbe_roundtrip(self, order: NativeOrder, ts: int) -> tuple[bytes, NativeOrder, int]:
        buf = (ctypes.c_uint8 * 128)()
        encoded_len = self.lib.qfx_c_sbe_encode_order(
            ctypes.byref(order), ts, buf, len(buf)
        )
        if encoded_len <= 0:
            raise RuntimeError("encode failed")
        decoded_order = NativeOrder()
        decoded_ts = ctypes.c_uint64(0)
        rc = self.lib.qfx_c_sbe_decode_order(
            buf, encoded_len, ctypes.byref(decoded_order), ctypes.byref(decoded_ts)
        )
        if rc != 0:
            raise RuntimeError("decode failed")
        return bytes(buf[:encoded_len]), decoded_order, decoded_ts.value

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


class NativeVenueQuote(ctypes.Structure):
    _fields_ = [
        ("venue_id", ctypes.c_uint32),
        ("bid", ctypes.c_int64),
        ("ask", ctypes.c_int64),
        ("capacity", ctypes.c_uint64),
        ("fee", ctypes.c_int64),
        ("slippage", ctypes.c_int64),
        ("score", ctypes.c_int64),
    ]


class NativeVenueSplit(ctypes.Structure):
    _fields_ = [
        ("venue_id", ctypes.c_uint32),
        ("quantity", ctypes.c_uint64),
        ("expected_price", ctypes.c_int64),
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

        self.lib.qfx_c_risk_eval.argtypes = [
            ctypes.POINTER(NativeOrder),
            ctypes.c_int64,
            ctypes.c_int64,
            ctypes.c_uint64,
        ]
        self.lib.qfx_c_risk_eval.restype = ctypes.c_int

        self.lib.qfx_c_sor_split.argtypes = [
            ctypes.c_int64,
            ctypes.c_uint64,
            ctypes.POINTER(NativeVenueQuote),
            ctypes.c_size_t,
            ctypes.POINTER(NativeVenueSplit),
            ctypes.c_size_t,
        ]
        self.lib.qfx_c_sor_split.restype = ctypes.c_int

        self.lib.qfx_c_simd_score4.argtypes = [
            ctypes.POINTER(ctypes.c_int64),
            ctypes.POINTER(ctypes.c_int64),
            ctypes.POINTER(ctypes.c_int64),
            ctypes.POINTER(ctypes.c_int64),
            ctypes.POINTER(ctypes.c_int64),
            ctypes.POINTER(ctypes.c_int64),
        ]
        self.lib.qfx_c_simd_score4.restype = None

        self.lib.qfx_c_var_compute.argtypes = [
            ctypes.POINTER(ctypes.c_int64),
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_int64),
            ctypes.POINTER(ctypes.c_int64),
        ]
        self.lib.qfx_c_var_compute.restype = ctypes.c_int

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

    def eval_risk(self, order: NativeOrder, mid: int, pos: int, now_ns: int) -> int:
        return int(self.lib.qfx_c_risk_eval(ctypes.byref(order), mid, pos, now_ns))

    def split_venues(self, side: int, qty: int, quotes: list[NativeVenueQuote]) -> list[NativeVenueSplit]:
        in_arr = (NativeVenueQuote * len(quotes))(*quotes)
        out_arr = (NativeVenueSplit * 16)()
        n = self.lib.qfx_c_sor_split(side, qty, in_arr, len(quotes), out_arr, 16)
        if n < 0:
            raise RuntimeError("sor split failed")
        return [out_arr[i] for i in range(n)]

    def score_simd4(self, asks: list[int], bids: list[int], lats: list[int], slips: list[int], fees: list[int]) -> list[int]:
        c_asks = (ctypes.c_int64 * 4)(*asks)
        c_bids = (ctypes.c_int64 * 4)(*bids)
        c_lats = (ctypes.c_int64 * 4)(*lats)
        c_slips = (ctypes.c_int64 * 4)(*slips)
        c_fees = (ctypes.c_int64 * 4)(*fees)
        c_out = (ctypes.c_int64 * 4)()
        self.lib.qfx_c_simd_score4(c_asks, c_bids, c_lats, c_slips, c_fees, c_out)
        return [c_out[i] for i in range(4)]

    def compute_var(self, returns: list[int]) -> tuple[int, int]:
        c_ret = (ctypes.c_int64 * len(returns))(*returns)
        out_var = ctypes.c_int64(0)
        out_cvar = ctypes.c_int64(0)
        rc = self.lib.qfx_c_var_compute(c_ret, len(returns), ctypes.byref(out_var), ctypes.byref(out_cvar))
        if rc != 0:
            raise RuntimeError("var compute failed")
        return out_var.value, out_cvar.value

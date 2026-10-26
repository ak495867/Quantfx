# Simple Binary Encoding (SBE) Wire Specification

QuantFX implements compact, fixed-size binary message structures conforming to Simple Binary Encoding (SBE) v1.0 layout standards. All multi-byte integer values are serialized in little-endian byte order with 8-byte natural alignment.

## Common SBE Header (8 bytes)

```text
+-------------------+-------------------+-------------------+-------------------+
| block_length (u16)|  template_id (u16)|   schema_id (u16) |    version (u16)  |
+-------------------+-------------------+-------------------+-------------------+
0                   2                   4                   6                   8
```

| Offset | Size | Type | Field | Description |
|---:|---:|---|---|---|
| 0 | 2 | uint16 | `block_length` | Size of the message root block in bytes |
| 2 | 2 | uint16 | `template_id` | Unique SBE template identifier |
| 4 | 2 | uint16 | `schema_id` | Schema identifier (1) |
| 6 | 2 | uint16 | `version` | Schema version (1) |

---

## SBE Order Message (Template ID: 101, 56 bytes)

```text
+-------------------------------------------------------------------------------+
| SBE Header (8 bytes)                                                          |
+-------------------------------------------------------------------------------+
| timestamp_ns (u64)                                                            |
+-------------------------------------------------------------------------------+
| client_order_id (u64)                                                         |
+-------------------------------------------------------------------------------+
| symbol (u64)                                                                  |
+-------------------------------------------------------------------------------+
| price (i64)                                                                   |
+-------------------------------------------------------------------------------+
| quantity (u64)                                                                |
+-------------------+-------------------+-------------------+-------------------+
| side (i8)         | time_in_force (u8)| padding (u16)     | reserved (u32)    |
+-------------------+-------------------+-------------------+-------------------+
```

| Offset | Size | Type | Field | Description |
|---:|---:|---|---|---|
| 0 | 8 | SbeHeader | `header` | Template ID 101, block length 56 |
| 8 | 8 | uint64 | `timestamp_ns` | Nanoseconds since UNIX epoch |
| 16 | 8 | uint64 | `client_order_id` | Unique client order sequence identifier |
| 24 | 8 | uint64 | `symbol` | Encoded instrument ID |
| 32 | 8 | int64 | `price` | Fixed-point limit price (scale 100,000) |
| 40 | 8 | uint64 | `quantity` | Target execution quantity in units |
| 48 | 1 | int8 | `side` | Order side (1 = Buy, -1 = Sell) |
| 49 | 1 | uint8 | `time_in_force` | 0 = Day, 1 = GTC, 3 = IOC, 4 = FOK |
| 50 | 2 | uint16 | `padding` | Cache-line alignment padding |
| 52 | 4 | uint32 | `reserved` | Reserved for future exchange flags |

---

## SBE Execution Report (Template ID: 102, 56 bytes)

```text
+-------------------------------------------------------------------------------+
| SBE Header (8 bytes)                                                          |
+-------------------------------------------------------------------------------+
| timestamp_ns (u64)                                                            |
+-------------------------------------------------------------------------------+
| client_order_id (u64)                                                         |
+-------------------------------------------------------------------------------+
| exec_id (u64)                                                                 |
+-------------------------------------------------------------------------------+
| exec_price (i64)                                                              |
+-------------------------------------------------------------------------------+
| exec_quantity (u64)                                                           |
+-------------------+-------------------+-------------------+-------------------+
| exec_type (u8)    | side (i8)         | reserved (u16)    | padding (u32)     |
+-------------------+-------------------+-------------------+-------------------+
```

| Offset | Size | Type | Field | Description |
|---:|---:|---|---|---|
| 0 | 8 | SbeHeader | `header` | Template ID 102, block length 56 |
| 8 | 8 | uint64 | `timestamp_ns` | Nanoseconds of match execution |
| 16 | 8 | uint64 | `client_order_id` | Originating client order ID |
| 24 | 8 | uint64 | `exec_id` | Unique exchange execution ID |
| 32 | 8 | int64 | `exec_price` | Weighted execution fill price |
| 40 | 8 | uint64 | `exec_quantity` | Filled volume in units |
| 48 | 1 | uint8 | `exec_type` | 1 = Partial Fill, 2 = Fill, 8 = Reject |
| 49 | 1 | int8 | `side` | Execution side (1 = Buy, -1 = Sell) |
| 50 | 2 | uint16 | `reserved` | Exchange flags |
| 52 | 4 | uint32 | `padding` | 8-byte structure alignment padding |

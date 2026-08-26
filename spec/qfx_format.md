# QFX Binary Format

All integer fields are little-endian unsigned or signed 64-bit values as defined by the field semantics. The file begins with a 64-byte header.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | magic `QFX1` encoded as `0x31584651` |
| 8 | 8 | format version |
| 16 | 8 | record size |
| 24 | 8 | price scale |
| 32 | 8 | instrument count |
| 40 | 8 | record count |
| 48 | 8 | block size |
| 56 | 8 | flags |

Each event record is 80 bytes.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | timestamp in nanoseconds |
| 8 | 8 | symbol identifier |
| 16 | 8 | bid price in fixed-point units |
| 24 | 8 | ask price in fixed-point units |
| 32 | 8 | last price in fixed-point units |
| 40 | 8 | volume |
| 48 | 8 | bid size |
| 56 | 8 | ask size |
| 64 | 8 | vendor sequence |
| 72 | 8 | event flags |

The current writer emits a header followed by fixed-width records. Future compatible versions may append block indexes and checksums after the record area while preserving the version and record-size fields.

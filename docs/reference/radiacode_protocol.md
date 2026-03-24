# RadiaCode USB Protocol Reference

Reverse-engineered from [cdump/radiacode](https://github.com/cdump/radiacode) Python library.

## USB Device

- **VID:** `0x0483` (STMicroelectronics)
- **PID:** `0xF123`
- **Write endpoint:** `0x01` (OUT, bulk)
- **Read endpoint:** `0x81` (IN, bulk)
- **Max packet:** 256 bytes
- **Requires firmware >= 4.8**

## Transport Layer

### Request Format
```
[4 bytes LE uint32: payload_length][payload]
```

### Payload Format
```
[2 bytes LE uint16: command_id][1 byte: 0x00][1 byte: seq_no (0x80 + seq)][optional args]
```
- `seq` increments 0-31, wraps around
- Response echoes the 4-byte header back

### Response Format
```
[4 bytes LE uint32: response_length][response_data]
```
Response data may span multiple USB reads. Keep reading until `response_length` bytes received.

### Init Sequence
1. Drain any pending data (read until timeout)
2. `SET_EXCHANGE` with `\x01\xff\x12\xff`
3. `SET_TIME` with current datetime
4. `DEVICE_TIME(0)`
5. Store `base_time = now() + 128 seconds`
6. Check firmware version >= 4.8
7. Read configuration for `SpecFormatVersion`

## Commands (COMMAND enum)

| Name | Value | Description |
|------|-------|-------------|
| GET_STATUS | 0x0005 | Device status flags |
| SET_EXCHANGE | 0x0007 | Init exchange params |
| GET_VERSION | 0x000A | Firmware version |
| GET_SERIAL | 0x000B | Hardware serial number |
| FW_SIGNATURE | 0x0101 | Firmware signature |
| RD_VIRT_SFR | 0x0824 | Read virtual SFR |
| WR_VIRT_SFR | 0x0825 | Write virtual SFR |
| RD_VIRT_STRING | 0x0826 | Read virtual string (data_buf, spectrum, etc.) |
| WR_VIRT_STRING | 0x0827 | Write virtual string |
| RD_VIRT_SFR_BATCH | 0x082A | Batch read SFRs |
| SET_TIME | 0x0A04 | Set device time |

## Virtual Strings (VS enum)

| Name | Value | Description |
|------|-------|-------------|
| CONFIGURATION | 0x02 | Device config (CP1251 encoded text) |
| SERIAL_NUMBER | 0x08 | Serial number (ASCII) |
| TEXT_MESSAGE | 0x0F | Text message (ASCII) |
| DATA_BUF | 0x100 | **Buffered measurement data** (the important one) |
| SFR_FILE | 0x101 | SFR definitions |
| SPECTRUM | 0x200 | Current spectrum |
| ENERGY_CALIB | 0x202 | Energy calibration coeffs (3x float) |
| SPEC_ACCUM | 0x205 | Accumulated spectrum |

## Data Buffer Decoding (VS.DATA_BUF = 0x100)

The data buffer contains a stream of records. Each record:

```
[1 byte: seq][1 byte: eid][1 byte: gid][4 bytes LE int32: ts_offset]
```

- `ts_offset * 10` = milliseconds from `base_time`
- `dt = base_time + timedelta(milliseconds=ts_offset * 10)`
- `seq` increments 0-255

### Record Types (eid=0)

| gid | Type | Fields | Struct Format |
|-----|------|--------|---------------|
| 0 | **RealTimeData** | count_rate(f), dose_rate(f), count_rate_err(H), dose_rate_err(H), flags(H), rt_flags(B) | `<ffHHHB` |
| 1 | RawData | count_rate(f), dose_rate(f) | `<ff` |
| 2 | DoseRateDB | count(I), count_rate(f), dose_rate(f), dose_rate_err(H), flags(H) | `<IffHH` |
| 3 | RareData | duration(I), dose(f), temperature(H), charge_level(H), flags(H) | `<IfHHH` |
| 7 | Event | event(B), event_param1(B), flags(H) | `<BBH` |

### RealTimeData (gid=0) — Primary reading type
- `count_rate`: float, counts per second
- `dose_rate`: float (units depend on device config, typically Sv)
- `count_rate_err`: uint16 / 10 = error percentage
- `dose_rate_err`: uint16 / 10 = error percentage

### RareData (gid=3) — Periodic status
- `temperature`: `(raw - 2000) / 100` = °C
- `charge_level`: `raw / 100` = fraction (0.0-1.0)

## Key VSFRs for RadiaLog

| Name | Value | Type | Description |
|------|-------|------|-------------|
| CPS | 0x8020 | uint32 | Counts per second |
| DR_uR_h | 0x8021 | uint32 | Dose rate (µR/h) |
| TEMP_degC | 0x8024 | float | Device temperature |

## Unit Conversions
- dose_rate from data_buf: multiply by 10000 to get µSv/h (per BLE library)
- Or check device config for nSv vs µSv mode
- Temperature: `(raw_uint16 - 2000) / 100.0` = °C
- Charge level: `raw_uint16 / 100.0` = 0.0-1.0

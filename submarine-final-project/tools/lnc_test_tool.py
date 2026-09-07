#!/usr/bin/env python3
"""
LNC test tool - PC-side client for the Submarine Monitoring System's
LNC<->Central Computer serial protocol.

Speaks the outer frame (comm_frame.h) and TLV payload (tlv.h/comm_tags.h)
formats exactly as implemented on the device, so it can:
  - listen and decode keep-alive / event / data-report frames
  - send commands (SET_LIMITS, SET_TIME, GET_TIME, GET_DATA, GET_EVENTS)

Requires: pip install pyserial

Usage examples:
  python3 lnc_test_tool.py --port COM5 listen
  python3 lnc_test_tool.py --port COM5 get-time
  python3 lnc_test_tool.py --port COM5 set-time
  python3 lnc_test_tool.py --port COM5 set-limits --param temp --normal-min 15 --normal-max 30 --warning-min 5 --warning-max 40
  python3 lnc_test_tool.py --port COM5 set-limits --param humidity --normal-min 30 --warning-min 15
  python3 lnc_test_tool.py --port COM5 get-events --start 20260901 --end 20260905
  python3 lnc_test_tool.py --port COM5 get-data --start 20260901 --end 20260905

On Windows, --port looks like COM5. On Linux/macOS it looks like
/dev/ttyACM0 or /dev/tty.usbmodemXXXX. Baud rate must match the board's
USART2 config (default 115200 - pass --baud to override).
"""
import argparse
import datetime
import struct
import sys
import time

try:
    import serial
except ImportError:
    print("Missing dependency: pip install pyserial", file=sys.stderr)
    sys.exit(1)

# ---- comm_frame.h -----------------------------------------------------
STX = 0x02
ETX = 0x03
MAX_PAYLOAD = 128

MSG_TYPE_KEEPALIVE = 0x01
MSG_TYPE_EVENT = 0x02
MSG_TYPE_DATA_REPORT = 0x03
MSG_TYPE_ACK = 0x04
MSG_TYPE_CMD_SET_LIMITS = 0x10
MSG_TYPE_CMD_SET_TIME = 0x11
MSG_TYPE_CMD_GET_TIME = 0x12
MSG_TYPE_CMD_GET_DATA = 0x13
MSG_TYPE_CMD_GET_EVENTS = 0x14

MSG_TYPE_NAMES = {
    MSG_TYPE_KEEPALIVE: "KEEPALIVE",
    MSG_TYPE_EVENT: "EVENT",
    MSG_TYPE_DATA_REPORT: "DATA_REPORT",
    MSG_TYPE_ACK: "ACK",
}

# ---- comm_tags.h --------------------------------------------------------
TAG_TIMESTAMP_YEAR = 0x01
TAG_TIMESTAMP_MONTH = 0x02
TAG_TIMESTAMP_DAY = 0x03
TAG_TIMESTAMP_HOUR = 0x04
TAG_TIMESTAMP_MIN = 0x05
TAG_TIMESTAMP_SEC = 0x06
TAG_MODE = 0x07
TAG_LIGHT_RAW = 0x08
TAG_TEMP_ADC_RAW = 0x09
TAG_BATTERY_RAW = 0x0A
TAG_DHT_TEMP = 0x0B
TAG_DHT_HUMIDITY = 0x0C
TAG_DHT_VALID = 0x0D
TAG_EVENT_TYPE = 0x0E
TAG_EVENT_VALUE = 0x0F
TAG_PARAM = 0x10
TAG_NORMAL_MIN = 0x11
TAG_NORMAL_MAX = 0x12
TAG_WARNING_MIN = 0x13
TAG_WARNING_MAX = 0x14
TAG_STATUS = 0x15
TAG_RANGE_START_YMD = 0x16
TAG_RANGE_END_YMD = 0x17
TAG_LOG_LINE = 0x18

TAG_NAMES = {v: k for k, v in list(globals().items()) if k.startswith("TAG_")}

PARAM_NAMES = {0: "TEMP", 1: "HUMIDITY", 2: "LIGHT", 3: "BATTERY"}
PARAM_VALUES = {"temp": 0, "humidity": 1, "light": 2, "battery": 3}
STATUS_NAMES = {0: "OK", 1: "ERROR"}
MODE_NAMES = {0: "NORMAL", 1: "WARNING", 2: "ERROR"}
EVENT_TYPE_NAMES = {1: "OBJECT_DETECTED", 2: "OBJECT_CLEARED", 3: "SILENCE_PRESSED", 4: "MODE_CHANGED"}


# ---- TLV encode/decode (tlv.h/tlv.c, big-endian) ----------------------
def tlv_encode(tag, value_bytes):
    if len(value_bytes) > 0xFFFF:
        raise ValueError("TLV value too long")
    return bytes([tag]) + struct.pack(">H", len(value_bytes)) + value_bytes


def tlv_encode_u8(tag, val):
    return tlv_encode(tag, struct.pack(">B", val & 0xFF))


def tlv_encode_u16(tag, val):
    return tlv_encode(tag, struct.pack(">H", val & 0xFFFF))


def tlv_encode_u32(tag, val):
    return tlv_encode(tag, struct.pack(">I", val & 0xFFFFFFFF))


def tlv_encode_i32(tag, val):
    return tlv_encode(tag, struct.pack(">i", val))


def tlv_decode_all(buf):
    """Decode a flat buffer of concatenated TLV fields into a list of
    (tag, value_bytes). Stops (without raising) at the first field that
    would run past the end of buf, mirroring the device's bounds check."""
    fields = []
    offset = 0
    while offset < len(buf):
        if offset + 3 > len(buf):
            break
        tag = buf[offset]
        length = struct.unpack(">H", buf[offset + 1:offset + 3])[0]
        start = offset + 3
        end = start + length
        if end > len(buf):
            break
        fields.append((tag, buf[start:end]))
        offset = end
    return fields


def field_u8(v):
    return v[0] if len(v) == 1 else None


def field_u32(v):
    return struct.unpack(">I", v)[0] if len(v) == 4 else None


def field_i32(v):
    return struct.unpack(">i", v)[0] if len(v) == 4 else None


# ---- comm_frame.c encode/decode ----------------------------------------
def frame_encode(msg_type, payload):
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload too large")
    length = len(payload)
    body = bytes([msg_type, (length >> 8) & 0xFF, length & 0xFF]) + payload
    checksum = 0
    for b in body:
        checksum ^= b
    return bytes([STX]) + body + bytes([checksum, ETX])


class FrameDecoder:
    """Mirrors CommFrameDecoder_t / CommFrame_DecoderFeed exactly,
    including the resync-on-failure fix (re-examine the triggering byte
    as a possible fresh STX instead of discarding it)."""

    WAIT_STX, WAIT_TYPE, WAIT_LEN_HI, WAIT_LEN_LO, WAIT_PAYLOAD, WAIT_CHECKSUM, WAIT_ETX = range(7)

    def __init__(self):
        self._reset()

    def _reset(self):
        self.state = self.WAIT_STX
        self.received = 0
        self.checksum_accum = 0
        self.type = 0
        self.length = 0
        self.payload = bytearray()
        self.expected_checksum = 0

    def feed(self, byte):
        """Feed one byte. Returns (msg_type, payload_bytes) when a full
        valid frame completes, else None."""
        while True:
            if self.state == self.WAIT_STX:
                if byte == STX:
                    self.checksum_accum = 0
                    self.state = self.WAIT_TYPE
                return None
            if self.state == self.WAIT_TYPE:
                self.type = byte
                self.checksum_accum ^= byte
                self.state = self.WAIT_LEN_HI
                return None
            if self.state == self.WAIT_LEN_HI:
                self.length = byte << 8
                self.checksum_accum ^= byte
                self.state = self.WAIT_LEN_LO
                return None
            if self.state == self.WAIT_LEN_LO:
                self.length |= byte
                self.checksum_accum ^= byte
                self.received = 0
                self.payload = bytearray()
                if self.length > MAX_PAYLOAD:
                    self._reset()
                    continue
                self.state = self.WAIT_CHECKSUM if self.length == 0 else self.WAIT_PAYLOAD
                return None
            if self.state == self.WAIT_PAYLOAD:
                self.payload.append(byte)
                self.checksum_accum ^= byte
                self.received += 1
                if self.received >= self.length:
                    self.state = self.WAIT_CHECKSUM
                return None
            if self.state == self.WAIT_CHECKSUM:
                self.expected_checksum = byte
                self.state = self.WAIT_ETX
                return None
            if self.state == self.WAIT_ETX:
                ok = (byte == ETX) and (self.expected_checksum == self.checksum_accum)
                msg_type, payload = self.type, bytes(self.payload)
                self._reset()
                if ok:
                    return (msg_type, payload)
                continue
            # unreachable
            self._reset()
            continue


# ---- pretty-printing ----------------------------------------------------
def describe_fields(payload):
    parts = []
    for tag, val in tlv_decode_all(payload):
        name = TAG_NAMES.get(tag, f"0x{tag:02X}")
        if tag in (TAG_TIMESTAMP_YEAR,):
            v = struct.unpack(">H", val)[0] if len(val) == 2 else val.hex()
        elif tag in (TAG_MODE,):
            v = MODE_NAMES.get(field_u8(val), val.hex())
        elif tag in (TAG_EVENT_TYPE,):
            v = EVENT_TYPE_NAMES.get(field_u8(val), val.hex())
        elif tag in (TAG_STATUS,):
            v = STATUS_NAMES.get(field_u8(val), val.hex())
        elif tag in (TAG_PARAM,):
            v = PARAM_NAMES.get(field_u8(val), val.hex())
        elif tag == TAG_LOG_LINE:
            v = val.decode("ascii", errors="replace")
        elif len(val) == 1:
            v = val[0]
        elif len(val) == 4:
            v = field_u32(val)
        else:
            v = val.hex()
        parts.append(f"{name}={v}")
    return ", ".join(parts)


def print_frame(msg_type, payload):
    ts = time.strftime("%H:%M:%S")
    name = MSG_TYPE_NAMES.get(msg_type, f"0x{msg_type:02X}")
    print(f"[{ts}] {name}: {describe_fields(payload)}")


# ---- commands ------------------------------------------------------------
def open_port(args):
    return serial.Serial(args.port, args.baud, timeout=0.2)


def cmd_listen(args):
    ser = open_port(args)
    dec = FrameDecoder()
    print(f"Listening on {args.port} @ {args.baud} - Ctrl+C to stop")
    try:
        while True:
            chunk = ser.read(256)
            for b in chunk:
                result = dec.feed(b)
                if result:
                    print_frame(*result)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()


def wait_for_reply(ser, dec, expected_types, timeout_s=3.0):
    """Read bytes until a frame of one of expected_types arrives, or
    timeout. Prints and skips any other frame types (e.g. an EVENT or
    KEEPALIVE that happens to interleave)."""
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        chunk = ser.read(256)
        for b in chunk:
            result = dec.feed(b)
            if result:
                msg_type, payload = result
                if msg_type in expected_types:
                    return result
                print_frame(msg_type, payload)
    return None


def cmd_get_time(args):
    ser = open_port(args)
    dec = FrameDecoder()
    ser.write(frame_encode(MSG_TYPE_CMD_GET_TIME, b""))
    result = wait_for_reply(ser, dec, {MSG_TYPE_DATA_REPORT})
    if result:
        print_frame(*result)
    else:
        print("No reply (timeout)")
    ser.close()


def cmd_set_time(args):
    ser = open_port(args)
    dec = FrameDecoder()
    if args.datetime:
        dt = datetime.datetime.strptime(args.datetime, "%Y-%m-%d %H:%M:%S")
    else:
        dt = datetime.datetime.now()
    payload = b"".join([
        tlv_encode_u16(TAG_TIMESTAMP_YEAR, dt.year),
        tlv_encode_u8(TAG_TIMESTAMP_MONTH, dt.month),
        tlv_encode_u8(TAG_TIMESTAMP_DAY, dt.day),
        tlv_encode_u8(TAG_TIMESTAMP_HOUR, dt.hour),
        tlv_encode_u8(TAG_TIMESTAMP_MIN, dt.minute),
        tlv_encode_u8(TAG_TIMESTAMP_SEC, dt.second),
    ])
    print(f"Sending SET_TIME -> {dt}")
    ser.write(frame_encode(MSG_TYPE_CMD_SET_TIME, payload))
    result = wait_for_reply(ser, dec, {MSG_TYPE_ACK})
    if result:
        print_frame(*result)
    else:
        print("No ACK (timeout)")
    ser.close()


def cmd_set_limits(args):
    ser = open_port(args)
    dec = FrameDecoder()
    param = PARAM_VALUES[args.param]
    fields = [tlv_encode_u8(TAG_PARAM, param)]
    if args.normal_min is not None:
        fields.append(tlv_encode_i32(TAG_NORMAL_MIN, args.normal_min))
    if args.normal_max is not None:
        fields.append(tlv_encode_i32(TAG_NORMAL_MAX, args.normal_max))
    if args.warning_min is not None:
        fields.append(tlv_encode_i32(TAG_WARNING_MIN, args.warning_min))
    if args.warning_max is not None:
        fields.append(tlv_encode_i32(TAG_WARNING_MAX, args.warning_max))
    payload = b"".join(fields)
    print(f"Sending SET_LIMITS param={args.param} -> {describe_fields(payload)}")
    ser.write(frame_encode(MSG_TYPE_CMD_SET_LIMITS, payload))
    result = wait_for_reply(ser, dec, {MSG_TYPE_ACK})
    if result:
        print_frame(*result)
    else:
        print("No ACK (timeout)")
    ser.close()


def cmd_get_range(args, msg_type):
    ser = open_port(args)
    dec = FrameDecoder()
    payload = tlv_encode_u32(TAG_RANGE_START_YMD, args.start) + tlv_encode_u32(TAG_RANGE_END_YMD, args.end)
    ser.write(frame_encode(msg_type, payload))
    print(f"Query {args.start}-{args.end}, streaming results (terminated by an empty DATA_REPORT)...")
    deadline = time.time() + args.timeout
    while time.time() < deadline:
        chunk = ser.read(256)
        for b in chunk:
            result = dec.feed(b)
            if result:
                msg_type_r, payload_r = result
                if msg_type_r == MSG_TYPE_DATA_REPORT and len(payload_r) == 0:
                    print("-- end of results --")
                    ser.close()
                    return
                print_frame(msg_type_r, payload_r)
    print("(timeout waiting for terminator - results above may be incomplete)")
    ser.close()


def cmd_get_data(args):
    cmd_get_range(args, MSG_TYPE_CMD_GET_DATA)


def cmd_get_events(args):
    cmd_get_range(args, MSG_TYPE_CMD_GET_EVENTS)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", required=True, help="Serial port, e.g. COM5 or /dev/ttyACM0")
    p.add_argument("--baud", type=int, default=115200)
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("listen", help="Decode and print all incoming frames")

    sub.add_parser("get-time", help="Send GET_TIME, print the reply")

    sp = sub.add_parser("set-time", help="Send SET_TIME (defaults to this PC's current time)")
    sp.add_argument("--datetime", help="'YYYY-MM-DD HH:MM:SS', default = now")

    sp = sub.add_parser("set-limits", help="Send SET_LIMITS for one parameter")
    sp.add_argument("--param", required=True, choices=list(PARAM_VALUES.keys()))
    sp.add_argument("--normal-min", type=int)
    sp.add_argument("--normal-max", type=int, help="PARAM_TEMP only")
    sp.add_argument("--warning-min", type=int)
    sp.add_argument("--warning-max", type=int, help="PARAM_TEMP only")

    sp = sub.add_parser("get-data", help="Send GET_DATA for a YYYYMMDD range")
    sp.add_argument("--start", type=int, required=True)
    sp.add_argument("--end", type=int, required=True)
    sp.add_argument("--timeout", type=float, default=5.0)

    sp = sub.add_parser("get-events", help="Send GET_EVENTS for a YYYYMMDD range")
    sp.add_argument("--start", type=int, required=True)
    sp.add_argument("--end", type=int, required=True)
    sp.add_argument("--timeout", type=float, default=5.0)

    args = p.parse_args()
    {
        "listen": cmd_listen,
        "get-time": cmd_get_time,
        "set-time": cmd_set_time,
        "set-limits": cmd_set_limits,
        "get-data": cmd_get_data,
        "get-events": cmd_get_events,
    }[args.cmd](args)


if __name__ == "__main__":
    main()

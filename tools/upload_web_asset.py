#!/usr/bin/env python3
"""Replace one Gagguino LittleFS web asset over USB serial; no FS formatting.

Install: py -m pip install pyserial
Example: py tools/upload_web_asset.py --port COM5 --asset js --file data/js/app.js
Build the firmware with GAGGUINO_USB_WEB_UPDATE enabled on an isolated USB board.
"""
import argparse
import binascii
from pathlib import Path
import sys

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="USB serial port (e.g. COM5)")
    parser.add_argument("--asset", choices=("html", "css", "js"), required=True)
    parser.add_argument("--file", type=Path, required=True)
    parser.add_argument("--baud", type=int, default=9600)
    args = parser.parse_args()

    try:
        import serial
    except ImportError:
        parser.error("pyserial is required: py -m pip install pyserial")

    data = args.file.read_bytes()
    if not 0 < len(data) <= 65536:
        parser.error("File must contain 1..65536 bytes")
    crc = binascii.crc32(data) & 0xffffffff
    print(f"Uploading {args.asset}: {len(data)} bytes CRC32={crc:08X}")
    print("WARNING: use only an isolated USB ESP32, with all mains/actuators disconnected.")

    def send(ser, command, expected):
        ser.write((command + "\n").encode("ascii"))
        ser.flush()
        for _ in range(12):
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if line:
                print("ESP32:", line)
            if line.startswith("WEBUPDATE ERR"):
                raise RuntimeError(line)
            if line == expected:
                return
        raise RuntimeError("No expected acknowledgment: " + expected)

    # Opening the serial port may reset ESP32. Give firmware time to boot,
    # and clear unrelated logging before the first command.
    import time
    with serial.Serial(args.port, args.baud, timeout=3, write_timeout=3) as ser:
        time.sleep(3)
        ser.reset_input_buffer()
        try:
            send(ser, f"BEGIN {args.asset} {len(data)} {crc:08X}", "WEBUPDATE OK READY")
            for offset in range(0, len(data), 64):
                send(ser, "DATA " + data[offset:offset + 64].hex(), "WEBUPDATE OK DATA")
                if (offset // 64) % 32 == 0:
                    print(f"Sent {min(offset + 64, len(data))}/{len(data)} bytes")
            send(ser, "END", "WEBUPDATE OK COMMIT")
        except Exception:
            try:
                ser.write(b"ABORT\n")
                ser.flush()
            except Exception:
                pass
            raise
    print("Done. Hard-refresh the web page (Ctrl+Shift+R).")
    print("Verify /api/shots still lists the same archive entries.")

if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)

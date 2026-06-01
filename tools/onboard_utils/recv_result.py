#!/usr/bin/env python3
"""Send image to Zynq via UDP, receive input echo, compare against original, report first mismatch."""

import socket
import struct
import sys
import numpy as np
import time
from pathlib import Path

import cv2

ZYNQ_IP    = "192.168.1.10"
ZYNQ_PORT  = 5000
WIDTH      = 640
HEIGHT     = 480
PKT_DATA   = 1396
HDR_SIZE   = 4
MAX_PKTS   = 221
TIMEOUT    = 10

def compare_neighborhood(arr_orig, arr_echo, row, col):
    rmin = max(row - 3, 0)
    rmax = min(row + 4, arr_orig.shape[0])
    cmin = max(col - 3, 0)
    cmax = min(col + 4, arr_orig.shape[1])

    print(f"\n  --- Originals @ [{rmin}:{rmax}, {cmin}:{cmax}] ---")
    for r in range(rmin, rmax):
        line = " ".join(f"{arr_orig[r,c]:3d}" for c in range(cmin, cmax))
        print(f"  R{r:3d}: {line}")

    print(f"\n  --- Echo      @ [{rmin}:{rmax}, {cmin}:{cmax}] ---")
    for r in range(rmin, rmax):
        line = " ".join(f"{arr_echo[r,c]:3d}" for c in range(cmin, cmax))
        print(f"  R{r:3d}: {line}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: python {Path(__file__).name} <image_file> [zynq_ip]")
        sys.exit(1)

    image_path = sys.argv[1]
    ip = sys.argv[2] if len(sys.argv) > 2 else ZYNQ_IP

    img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        print(f"Failed to load: {image_path}")
        sys.exit(1)

    h, w = img.shape
    if w != WIDTH or h != HEIGHT:
        print(f"Resizing {w}x{h} -> {WIDTH}x{HEIGHT}")
        img = cv2.resize(img, (WIDTH, HEIGHT))

    raw        = img.tobytes()
    total_pkts = (len(raw) + PKT_DATA - 1) // PKT_DATA

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(TIMEOUT)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)

    # Send image
    for idx in range(total_pkts):
        offset = idx * PKT_DATA
        chunk  = raw[offset:offset + PKT_DATA]
        header = struct.pack("<HH", total_pkts, idx)
        sock.sendto(header + chunk, (ip, ZYNQ_PORT))
        time.sleep(0.05)

    print(f"Sent {total_pkts} pkts ({len(raw)} bytes) -> {ip}:{ZYNQ_PORT}")
    print(f"[SEND] row0[69:81]: {' '.join(f'{b:02X}' for b in raw[69:82])}")

    # Receive input echo
    img_size = WIDTH * HEIGHT
    echo_data = bytearray(img_size)
    received = [False] * MAX_PKTS
    count = 0
    echo_total = 0

    print("Waiting for input echo...")

    try:
        while count < echo_total or echo_total == 0:
            data, addr = sock.recvfrom(65535)

            total_pkts_rx = data[0] | (data[1] << 8)
            pkt_idx       = data[2] | (data[3] << 8)
            payload       = data[HDR_SIZE:]

            if echo_total == 0:
                echo_total = total_pkts_rx
                print(f"Expecting {echo_total} echo pkts ({img_size} bytes)")

            if echo_total == total_pkts_rx and pkt_idx < MAX_PKTS and not received[pkt_idx]:
                received[pkt_idx] = True
                count += 1
                offset = pkt_idx * PKT_DATA
                echo_data[offset:offset + len(payload)] = payload

    except socket.timeout:
        print(f"Timeout: received {count}/{echo_total} pkts")

    sock.close()

    if count != echo_total:
        print(f"Incomplete: {count}/{echo_total} — saving partial")
    else:
        print(f"Received complete: {count} pkts")

    # Save echo as PNG
    base = Path(image_path).stem
    arr_echo = np.frombuffer(bytes(echo_data), dtype=np.uint8).reshape(HEIGHT, WIDTH)
    out_png = f"{base}_echo.png"
    cv2.imwrite(out_png, arr_echo)
    print(f"Saved {out_png}")

    arr_orig = img

    # Find first mismatching pixel
    mask = arr_orig != arr_echo
    if not mask.any():
        print("\nAll pixels match — loopback perfect.")
    else:
        mismatch = np.argwhere(mask)
        r, c = mismatch[0]
        print(f"\nFirst mismatch at row={r} col={c}")
        print(f"  Original: {arr_orig[r,c]} (0x{arr_orig[r,c]:02X})")
        print(f"  Echo:     {arr_echo[r,c]} (0x{arr_echo[r,c]:02X})")
        compare_neighborhood(arr_orig, arr_echo, r, c)

        total_mismatch = mask.sum()
        print(f"\nTotal mismatched pixels: {total_mismatch} / {WIDTH * HEIGHT} ({100*total_mismatch/(WIDTH*HEIGHT):.1f}%)")

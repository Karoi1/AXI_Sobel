#!/usr/bin/env python3
"""Send grayscale 640x480 to Zynq Z7-20 via UDP, receive 640x480 Sobel result, save to disk.

Protocol: [total_pkts:2B LE][pkt_idx:2B LE][payload]
"""
import time
import socket
import struct
import sys
import numpy as np
from pathlib import Path

import cv2

ZYNQ_IP    = "192.168.1.10"
ZYNQ_PORT  = 5000
WIDTH      = 640
HEIGHT_IN  = 480
HEIGHT_OUT = 480
PKT_DATA   = 1396
HDR_SIZE   = 4
MAX_PKTS   = 221
TIMEOUT    = 30


def recv_sobel(sock, img_size_out):
    sobel_data = bytearray(img_size_out)
    received = [False] * MAX_PKTS
    count = 0
    sobel_total = 0

    try:
        while count < sobel_total or sobel_total == 0:
            data, addr = sock.recvfrom(65535)
            total_pkts = data[0] | (data[1] << 8)
            pkt_idx    = data[2] | (data[3] << 8)

            if sobel_total == 0:
                sobel_total = total_pkts
                expected = (img_size_out + PKT_DATA - 1) // PKT_DATA
                print(f"Sobel: expecting {sobel_total} pkts ({img_size_out} bytes)")

                if sobel_total != expected:
                    print(f"WARNING: expected {expected} pkts, got {sobel_total}")

            if sobel_total == total_pkts and pkt_idx < MAX_PKTS and not received[pkt_idx]:
                received[pkt_idx] = True
                count += 1
                offset = pkt_idx * PKT_DATA
                payload = data[HDR_SIZE:]
                sobel_data[offset:offset + len(payload)] = payload

    except socket.timeout:
        print(f"Timeout: received {count}/{sobel_total} pkts")

    return bytes(sobel_data), count


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: python {Path(__file__).name} <image_file> [zynq_ip] [output_file]")
        sys.exit(1)

    image_path = sys.argv[1]
    ip = sys.argv[2] if len(sys.argv) > 2 else ZYNQ_IP
    output = sys.argv[3] if len(sys.argv) > 3 else None

    img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        print(f"Failed to load: {image_path}")
        sys.exit(1)

    h, w = img.shape
    if w != WIDTH or h != HEIGHT_IN:
        print(f"Resizing {w}x{h} -> {WIDTH}x{HEIGHT_IN}")
        img = cv2.resize(img, (WIDTH, HEIGHT_IN))

    raw = img.tobytes()
    total_pkts = (len(raw) + PKT_DATA - 1) // PKT_DATA

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(TIMEOUT)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)

    for idx in range(total_pkts):
        offset = idx * PKT_DATA
        chunk  = raw[offset:offset + PKT_DATA]
        header = struct.pack("<HH", total_pkts, idx)
        sock.sendto(header + chunk, (ip, ZYNQ_PORT))
        time.sleep(0.01)  # small delay to avoid overwhelming the network

    print(f"Sent {total_pkts} pkts ({len(raw)} bytes) -> {ip}:{ZYNQ_PORT}")

    img_size_out = WIDTH * HEIGHT_OUT
    sobel_data, count = recv_sobel(sock, img_size_out)

    sock.close()

    base = output if output else Path(image_path).stem
    out_dir = Path(__file__).resolve().parent.parent.parent / "test_imgs" / "onboard_result"
    out_dir.mkdir(parents=True, exist_ok=True)

    if count > 0:
        arr = np.frombuffer(sobel_data, dtype=np.uint8).reshape(HEIGHT_OUT, WIDTH)
        out_png = str(out_dir / f"{base}_sobel.png")
        cv2.imwrite(out_png, arr)
        print(f"Saved {out_png} ({count} pkts)")
    else:
        print("No data received.")
        sys.exit(1)

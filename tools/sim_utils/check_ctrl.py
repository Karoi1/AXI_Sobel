#!/usr/bin/env python3
"""Validate ctrl_out.txt: check tlast/tvalid timing in AXI-Stream capture.

Rules:
  1. Every tlast=1 MUST have tvalid=1 at the same time slice.
  2. Between tlast events (including the tvalid that comes with each tlast),
     exactly 160 tvalid=1 beats must be counted. Counter resets at each tlast.

Usage: python check_ctrl.py [ctrl_out.txt path]
"""

import sys
from pathlib import Path

EXPECTED_TVALID = 160


def check_ctrl_out(filepath: str) -> bool:
    tvalid_count = 0
    line_num = 0
    tlast_index = 0
    errors = []
    first_tvalid_seen = False

    with open(filepath) as f:
        for line in f:
            line_num += 1
            line = line.strip()
            if not line:
                continue

            parts = line.split()
            if len(parts) != 4:
                errors.append(f"LINE {line_num}: malformed line: {line}")
                continue

            timestamp = int(parts[0])
            tvalid = int(parts[1])
            tlast = int(parts[2])
            tuser = int(parts[3])

            if tvalid == 1:
                first_tvalid_seen = True
                tvalid_count += 1

            if tlast == 1:
                tlast_index += 1

                if tvalid != 1:
                    errors.append(
                        f"tlast without tvalid at time {timestamp} "
                        f"(line {line_num}, tlast_idx={tlast_index})"
                    )
                    tvalid_count = 0
                    continue

                if tvalid_count != EXPECTED_TVALID:
                    errors.append(
                        f"tvalid count {tvalid_count} (expected {EXPECTED_TVALID}) "
                        f"at time {timestamp} (line {line_num}, tlast_idx={tlast_index})"
                    )

                tvalid_count = 0

    if not first_tvalid_seen:
        print("No tvalid=1 found in file.")
        return False

    if errors:
        print(f"ERRORS ({len(errors)}):")
        for e in errors:
            print(f"  {e}")
        return False
    else:
        print(f"OK: {tlast_index} tlast events, all {EXPECTED_TVALID}-beat lines correct.")
        return True


if __name__ == "__main__":
    default = Path(__file__).resolve().parent.parent / "ctrl_out.txt"
    fpath = sys.argv[1] if len(sys.argv) > 1 else str(default)

    if not Path(fpath).exists():
        print(f"File not found: {fpath}")
        sys.exit(1)

    ok = check_ctrl_out(fpath)
    sys.exit(0 if ok else 1)

"""Capture HV RAM through SWD without halting the CPU or printing from IRQ.

Requires OpenOCD and arm-none-eabi-nm on PATH, firmware built with
HV_DIAGNOSTICS=ON, and the matching ELF. Does not change PI or HV settings.
Timestamps use the nominal 1 kHz sample rate. Raw min/max include every
sample between records, even when --decimation is greater than one.
"""

import argparse
import csv
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
HEADER = ("capture decimation count samples rejected ovp_holds overruns filtered_mv "
          "raw_min raw_max divider interval_flags irq_max_us kp_q16 ki_q16 reserved "
          "filter_shift target_v").split()
COUNT = 128
SIZE = len(HEADER) * 4 + COUNT * 16


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True, help="ELF actually flashed on the board")
    parser.add_argument("--output", type=Path, required=True, help="CSV path; JSON summary is saved alongside")
    parser.add_argument("--decimation", type=int, default=8, help="1..80 ADC samples per record")
    args = parser.parse_args()
    if not 1 <= args.decimation <= 80:
        parser.error("decimation must be in 1..80")
    symbols = subprocess.check_output(["arm-none-eabi-nm", "-S", str(args.elf.resolve())], text=True)
    symbol = re.search(r"^([0-9a-fA-F]+) ([0-9a-fA-F]+) \w bsp_hv_diag$", symbols, re.M)
    if not symbol or int(symbol[2], 16) != SIZE:
        parser.error("ELF has no compatible bsp_hv_diag; build with HV_DIAGNOSTICS=ON")
    address = int(symbol[1], 16)
    with tempfile.TemporaryDirectory(prefix="hv-capture-") as temporary:
        binary = Path(temporary) / "capture.bin"
        script = (f"init; mww 0x{address+4:x} {args.decimation}; mww 0x{address:x} 1; "
                  f"sleep {COUNT*args.decimation+100}; "
                  f"dump_image {{{binary.as_posix()}}} 0x{address:x} {SIZE}; shutdown")
        run = subprocess.run(["openocd", "-f", "debug/openocd/board.cfg", "-c", script],
                             cwd=ROOT, capture_output=True, text=True, timeout=30)
        if run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        blob = binary.read_bytes()
    if len(blob) != SIZE:
        raise RuntimeError(f"Incomplete RAM dump: {len(blob)} bytes, expected {SIZE}")
    header = dict(zip(HEADER, struct.unpack_from("<18I", blob)))
    if header["capture"] != 3 or header["count"] != COUNT:
        raise RuntimeError(f"Incomplete capture: {header}")
    records = []
    for i in range(COUNT):
        mv, lo, hi, compare, phase, flags, irq = struct.unpack_from("<I6H", blob, 72 + i*16)
        records.append(dict(time_s=(i+1)*args.decimation/1000, filtered_v=mv/1000,
                            raw_min_v=lo*333.3/4095, raw_max_v=hi*333.3/4095,
                            compare=compare, pwm_phase_ticks=phase, flags=flags, irq_us=irq,
                            separators=bool(flags & 1024), enabled=bool(flags & 32768),
                            target_bin_v=100+10*((flags >> 11) & 15)))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, records[0].keys())
        writer.writeheader()
        writer.writerows(records)
    header.update(filtered_min_v=min(r["filtered_v"] for r in records),
                  filtered_max_v=max(r["filtered_v"] for r in records),
                  raw_min_v=min(r["raw_min_v"] for r in records),
                  raw_max_v=max(r["raw_max_v"] for r in records))
    args.output.with_suffix(".json").write_text(json.dumps(header, indent=2), encoding="utf-8")
    print(json.dumps(header, indent=2))


if __name__ == "__main__":
    main()

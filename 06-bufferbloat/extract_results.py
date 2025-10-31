#!/usr/bin/env python3
"""
extract_results.py

从 Mininet 运行输出中提取时间序列并写入 CSV：
 - cwnd_kb.csv (time_s,cwnd_kb)
 - qlen.csv (time_s,qlen_pkts)
 - rtt_ms.csv (time_s,rtt_ms)

用法示例：
  python3 extract_results.py          # 处理脚本目录下的 qlen-* 目录
  python3 extract_results.py qlen-10  # 只处理指定目录
"""

import re
import csv
import sys
from pathlib import Path
import argparse
import glob
import os


def parse_cwnd(cwnd_path, out_path):
    # read lines like: <timestamp>,  <ss -i output ... cwnd:10 ... mss:1448 ...>
    pat_cwnd = re.compile(r'cwnd:([0-9]+)')
    pat_mss = re.compile(r'mss:([0-9]+)')
    base = None
    rows = []
    with cwnd_path.open('r', errors='ignore') as f:
        for line in f:
            parts = line.split(',', 1)
            if not parts:
                continue
            try:
                ts = float(parts[0].strip())
            except Exception:
                continue
            if base is None:
                base = ts
            m = pat_cwnd.search(line)
            if not m:
                continue
            cwnd = float(m.group(1))
            mm = pat_mss.search(line)
            mss = float(mm.group(1)) if mm else 1448.0
            cwnd_kb = (cwnd * mss) / 1024.0
            rows.append((ts - base, cwnd_kb))

    if rows:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with out_path.open('w', newline='') as csvf:
            w = csv.writer(csvf)
            w.writerow(['time_s', 'cwnd_kb'])
            for t, v in rows:
                w.writerow([f"{t:.6f}", f"{v:.3f}"])


def parse_qlen(qlen_path, out_path):
    base = None
    rows = []
    with qlen_path.open('r', errors='ignore') as f:
        for line in f:
            parts = line.split(',')
            if len(parts) < 2:
                continue
            try:
                ts = float(parts[0].strip())
            except Exception:
                continue
            if base is None:
                base = ts
            val = parts[1].strip()
            rows.append((ts - base, val))

    if rows:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with out_path.open('w', newline='') as csvf:
            w = csv.writer(csvf)
            w.writerow(['time_s', 'qlen_pkts'])
            for t, v in rows:
                w.writerow([f"{t:.6f}", v])


def parse_rtt(rtt_path, out_path):
    # lines like: <timestamp>, 64 bytes from ... time=40.7 ms
    pat_time = re.compile(r'time=([0-9]+\.?[0-9]*)\s*ms')
    base = None
    rows = []
    with rtt_path.open('r', errors='ignore') as f:
        for line in f:
            parts = line.split(',', 1)
            if not parts:
                continue
            try:
                ts = float(parts[0].strip())
            except Exception:
                continue
            if base is None:
                base = ts
            m = pat_time.search(line)
            if not m:
                continue
            rtt = float(m.group(1))
            rows.append((ts - base, rtt))

    if rows:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with out_path.open('w', newline='') as csvf:
            w = csv.writer(csvf)
            w.writerow(['time_s', 'rtt_ms'])
            for t, v in rows:
                w.writerow([f"{t:.6f}", f"{v:.3f}"])


def process_dir(d: Path, outdir: Path):
    print(f"Processing: {d} -> {outdir}")
    cwnd = d / 'cwnd.txt'
    qlen = d / 'qlen.txt'
    rtt = d / 'rtt.txt'

    if cwnd.exists():
        parse_cwnd(cwnd, outdir / 'cwnd_kb.csv')
        print(f"  -> {outdir/'cwnd_kb.csv'}")
    else:
        print(f"  (no {cwnd})")

    if qlen.exists():
        parse_qlen(qlen, outdir / 'qlen.csv')
        print(f"  -> {outdir/'qlen.csv'}")
    else:
        print(f"  (no {qlen})")

    if rtt.exists():
        parse_rtt(rtt, outdir / 'rtt_ms.csv')
        print(f"  -> {outdir/'rtt_ms.csv'}")
    else:
        print(f"  (no {rtt})")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('pattern', nargs='?', default='qlen-*', help='directory pattern under script dir (default: qlen-*)')
    args = parser.parse_args()

    base_dir = Path(__file__).resolve().parent
    pattern = args.pattern
    matches = sorted(glob.glob(str(base_dir / pattern)))
    if not matches:
        print(f"No directories match: {base_dir}/{pattern}")
        sys.exit(1)

    for m in matches:
        d = Path(m)
        if not d.is_dir():
            continue
        # if writable by current user, write into same dir; else write into outputs/<dirname>
        if os.access(str(d), os.W_OK):
            outdir = d
        else:
            outdir = base_dir / 'outputs' / d.name
        process_dir(d, outdir)


if __name__ == '__main__':
    main()

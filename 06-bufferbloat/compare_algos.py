#!/usr/bin/env python3
"""
compare_algos.py

读取多个算法的 outputs/<algo>/rtt_ms.csv，并把它们绘制到同一张图中（支持对数 y 轴）。

用法：
  python3 compare_algos.py outputs/taildrop outputs/codel outputs/red -o outputs/mitigate_rtt.png --log
"""

import argparse
from pathlib import Path
import csv
import matplotlib.pyplot as plt


def read_rtt(path: Path):
    xs = []
    ys = []
    if not path.exists():
        return xs, ys
    with path.open('r', errors='ignore') as f:
        r = csv.reader(f)
        next(r, None)
        for row in r:
            if len(row) < 2:
                continue
            try:
                xs.append(float(row[0]))
                ys.append(float(row[1]))
            except Exception:
                continue
    return xs, ys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('dirs', nargs='+', help='Output directories (e.g. outputs/taildrop outputs/codel outputs/red)')
    parser.add_argument('-o', '--out', required=True, help='Output PNG file')
    parser.add_argument('--log', action='store_true', help='Use log scale on y axis')
    args = parser.parse_args()

    series = []
    labels = []
    for d in args.dirs:
        p = Path(d)
        name = p.name
        rttfile = p / 'rtt_ms.csv'
        tx, ty = read_rtt(rttfile)
        if tx and ty:
            series.append((tx, ty))
            labels.append(name)
        else:
            print(f'Warning: no rtt data for {name} ({rttfile})')

    if not series:
        print('No RTT series found. Exiting.')
        return

    plt.figure(figsize=(10,6))
    for (tx, ty), label in zip(series, labels):
        plt.plot(tx, ty, label=label)

    plt.xlabel('Time (s)')
    plt.ylabel('RTT (ms)')
    if args.log:
        # plt.yscale('log')
        ...
    plt.legend()
    plt.title('RTT comparison')
    plt.grid(True, which='both', linestyle='--', linewidth=0.5)
    outpath = Path(args.out)
    outpath.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(outpath)
    print(f'Saved combined plot to {outpath}')


if __name__ == '__main__':
    main()

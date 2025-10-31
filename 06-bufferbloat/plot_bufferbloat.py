#!/usr/bin/env python3
"""
plot_bufferbloat.py

读取由 extract_results.py 生成的 CSV（cwnd_kb.csv, qlen.csv, rtt_ms.csv），绘制三张随时间变化的图并保存。

用法示例：
  python3 plot_bufferbloat.py /path/to/output_dir
  python3 plot_bufferbloat.py --dir outputs/qlen-10 --out-file outputs/qlen-10/plot.png
"""

import argparse
from pathlib import Path
from typing import Optional
import csv
import matplotlib.pyplot as plt


def read_csv_two_cols(path):
    xs = []
    ys = []
    with path.open('r', errors='ignore') as f:
        r = csv.reader(f)
        header = next(r, None)
        for row in r:
            if len(row) < 2:
                continue
            try:
                xs.append(float(row[0]))
                ys.append(float(row[1]))
            except Exception:
                continue
    return xs, ys


def plot_dir(outdir: Path, out_file: Optional[Path] = None, show: bool = False):
    cwnd_file = outdir / 'cwnd_kb.csv'
    qlen_file = outdir / 'qlen.csv'
    rtt_file = outdir / 'rtt_ms.csv'

    # Dynamically create subplots only for available data files.
    panels = []  # list of tuples (key, path)
    if cwnd_file.exists():
        panels.append(('cwnd', cwnd_file))
    if qlen_file.exists():
        panels.append(('qlen', qlen_file))
    if rtt_file.exists():
        panels.append(('rtt', rtt_file))

    if not panels:
        # no data at all -> one empty figure with message
        fig, ax = plt.subplots(1, 1, figsize=(8, 3))
        ax.text(0.5, 0.5, 'No data (cwnd_kb.csv, qlen.csv, rtt_ms.csv missing)', ha='center', va='center')
        ax.set_axis_off()
    else:
        n = len(panels)
        fig, axs = plt.subplots(n, 1, figsize=(10, 3*n), sharex=True)
        # ensure axs is iterable list
        if n == 1:
            axs = [axs]

        for idx, (key, path) in enumerate(panels):
            ax = axs[idx]
            tx, ty = read_csv_two_cols(path)
            if key == 'cwnd':
                ax.plot(tx, ty, label='cwnd (KB)', color='C0')
                ax.set_ylabel('CWND (KB)')
            elif key == 'qlen':
                ax.plot(tx, ty, label='qlen (pkts)', color='C1')
                ax.set_ylabel('Queue (pkts)')
            elif key == 'rtt':
                ax.plot(tx, ty, label='RTT (ms)', color='C2')
                ax.set_ylabel('RTT (ms)')
            ax.legend()
        # xlabel on the bottom-most axis
        axs[-1].set_xlabel('Time (s)')

    fig.suptitle(f'Bufferbloat: {outdir.name}')
    fig.tight_layout(rect=(0, 0, 1, 0.97))

    if out_file:
        out_file.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(out_file)
        print(f'Saved plot to {out_file}')

    if show:
        plt.show()
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dir', '-d', required=True, help='Directory containing cwnd_kb.csv, qlen.csv, rtt_ms.csv')
    parser.add_argument('--out-file', '-o', help='Output PNG file (optional)')
    parser.add_argument('--show', action='store_true', help='Show plot interactively')
    args = parser.parse_args()

    outdir = Path(args.dir)
    if not outdir.exists():
        print(f'Directory not found: {outdir}')
        raise SystemExit(1)

    out_file = Path(args.out_file) if args.out_file else None
    plot_dir(outdir, out_file, args.show)


if __name__ == '__main__':
    main()

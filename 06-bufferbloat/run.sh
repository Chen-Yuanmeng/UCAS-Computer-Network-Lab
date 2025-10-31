#!/usr/bin/env bash
# Extract CWND (KB), Qlen (packet), RTT (ms) time series from Mininet run outputs
# Produces: <dir>/cwnd_kb.csv, <dir>/qlen.csv, <dir>/rtt.csv for each matching directory (default: qlen-*)

set -eu

PATTERN=${1:-qlen-*}

# operate relative to the script's directory so it works no matter CWD
BASEDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

process_dir() {
    dir="$1"
    echo "Processing: $dir"
    # choose output directory: prefer the source dir if writable, otherwise an outputs/ subdir
    if [ -w "$dir" ]; then
        outdir="$dir"
    else
        outdir="$BASEDIR/outputs/$(basename "$dir")"
        mkdir -p "$outdir"
    fi
    # cwnd -> extract timestamp and cwnd (packets) and mss -> convert to KB
    if [ -f "$dir/cwnd.txt" ]; then
    awk 'BEGIN{FS=","; base=""}
        {
            ts=$1;
            # extract cwnd and mss from the whole line
            if(match($0,/cwnd:([0-9]+)/,a)){
                cwnd=a[1];
                if(match($0,/mss:([0-9]+)/,b)) mss=b[1]; else mss=1448;
                if(base=="") base=ts+0;
                t=(ts+0)-base;
                cwnd_kb = (cwnd * mss) / 1024.0;
                # time in seconds (relative), cwnd in KB
                printf("%.6f,%.3f\n", t, cwnd_kb);
            }
    }' "$dir/cwnd.txt" > "$outdir/cwnd_kb.csv.tmp" || true
        # add header if file non-empty
        if [ -s "$outdir/cwnd_kb.csv.tmp" ]; then
            (echo "time_s,cwnd_kb"; cat "$outdir/cwnd_kb.csv.tmp") > "$outdir/cwnd_kb.csv"
            rm -f "$outdir/cwnd_kb.csv.tmp"
            echo "  -> $outdir/cwnd_kb.csv"
        else
            rm -f "$dir/cwnd_kb.csv.tmp"
            echo "  (no cwnd data)"
        fi
    else
        echo "  (no cwnd.txt)"
    fi

    # qlen -> simple timestamp,value pairs
    if [ -f "$dir/qlen.txt" ]; then
        awk -F"," 'NR==1{base=$1+0} {if(base=="" ) base=0; t=($1+0)-base; gsub(/^ +| +$/,"",$2); printf("%.6f,%s\n", t, $2)}' "$dir/qlen.txt" > "$outdir/qlen.csv.tmp" || true
        if [ -s "$outdir/qlen.csv.tmp" ]; then
            (echo "time_s,qlen_pkts"; cat "$outdir/qlen.csv.tmp") > "$outdir/qlen.csv"
            rm -f "$outdir/qlen.csv.tmp"
            echo "  -> $outdir/qlen.csv"
        else
            rm -f "$dir/qlen.csv.tmp"
            echo "  (no qlen data)"
        fi
    else
        echo "  (no qlen.txt)"
    fi

    # rtt -> timestamp, extract time=NN ms
    if [ -f "$dir/rtt.txt" ]; then
    awk 'BEGIN{base=""}
        {
            ts=$1;
            if(match($0,/time=([0-9]+\.?[0-9]*)\s*ms/,a)){
                rtt=a[1];
                if(base=="") base=ts+0;
                t=(ts+0)-base;
                printf("%.6f,%.3f\n", t, rtt);
            }
        }' "$dir/rtt.txt" > "$outdir/rtt_ms.csv.tmp" || true
        if [ -s "$outdir/rtt_ms.csv.tmp" ]; then
            (echo "time_s,rtt_ms"; cat "$outdir/rtt_ms.csv.tmp") > "$outdir/rtt_ms.csv"
            rm -f "$outdir/rtt_ms.csv.tmp"
            echo "  -> $outdir/rtt_ms.csv"
        else
            rm -f "$dir/rtt_ms.csv.tmp"
            echo "  (no rtt data)"
        fi
    else
        echo "  (no rtt.txt)"
    fi
}


shopt -s nullglob
# expand pattern under BASEDIR
mapfile -t dirs < <(printf "%s\n" "$BASEDIR"/$PATTERN)
if [ ${#dirs[@]} -eq 0 ]; then
    echo "No directories match pattern: $BASEDIR/$PATTERN"
    exit 1
fi

for d in "${dirs[@]}"; do
    if [ -d "$d" ]; then
        process_dir "$d"
    fi
done

echo "Done. CSV files created in each directory (if data existed)."
#!/bin/bash


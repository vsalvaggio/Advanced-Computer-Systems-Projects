#!/usr/bin/env bash
# ssd_profile.sh
# Run SSD performance profiling experiments with fio
# Usage: ./ssd_profile.sh /mnt/project_partition/testfile.img

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <target-file>"
    exit 1
fi

TARGET=$1
RESULTS_DIR=results/
mkdir -p "$RESULTS_DIR"

# Common fio options
COMMON="--filename=$TARGET --direct=1 --ioengine=libaio --time_based --runtime=30s --group_reporting --output-format=json"

########################################
# 1. Zero-queue baselines (QD=1)
########################################
echo "Running zero-queue baselines..."
for rw in randread randwrite read write; do
    bs="4k"
    [[ $rw == "read" || $rw == "write" ]] && bs="128k"
    fio $COMMON --name=zeroq_${rw}_${bs} --rw=$rw --bs=$bs --iodepth=1 --numjobs=1 \
        --output="$RESULTS_DIR/zeroq_${rw}_${bs}.json"
done

########################################
# 2. Block size sweep
########################################
echo "Running block size sweeps..."
for pattern in randread read; do
    for bs in 4k 16k 32k 64k 128k 256k; do
        fio $COMMON --name=bs_${pattern}_${bs} --rw=$pattern --bs=$bs --iodepth=32 --numjobs=1 \
            --output="$RESULTS_DIR/bs_${pattern}_${bs}.json"
    done
done

########################################
# 3. Read/Write mix sweep (4k random)
########################################
echo "Running read/write mix sweeps..."
for mix in 100 0 70 50; do
    fio $COMMON --name=mix_${mix}R --rw=randrw --rwmixread=$mix --bs=4k --iodepth=32 --numjobs=1 \
        --output="$RESULTS_DIR/mix_${mix}R.json"
done

########################################
# 4. Queue depth sweep (4k random read)
########################################
echo "Running queue depth sweeps..."
for qd in 1 2 4 8 16 32 64 128; do
    fio $COMMON --name=qd_randread_${qd} --rw=randread --bs=4k --iodepth=$qd --numjobs=1 \
        --output="$RESULTS_DIR/qd_randread_${qd}.json"
done

########################################
# 5. Tail latency characterization
########################################
echo "Running tail latency characterization..."
# Example: mid-QD=16, near-knee QD=64 (adjust if your SSD knee differs)
for qd in 16 64; do
    fio $COMMON --name=tail_lat_qd${qd} --rw=randread --bs=4k --iodepth=$qd --numjobs=1 \
        --output="$RESULTS_DIR/tail_lat_qd${qd}.json" --lat_percentiles=1
done

echo "All experiments done. Results stored in $RESULTS_DIR/"
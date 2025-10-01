#!/bin/bash
# Project 2: Cache & Memory Profiling Harness
# Usage: ./run_all.sh
# Results will be saved in results/YYYYMMDD_HHMM

set -e

# directories
PERF_BIN="$HOME/WSL2-Linux-Kernel/tools/perf/perf"
SAXPY_BIN="./saxpy"
OUTDIR="results/"
mkdir -p "$OUTDIR"

echo "Results will be saved to: $OUTDIR"
echo

############################################
# 1. Baseline Latency Measurements
############################################
############################################
# 1. Baseline Latency Measurements (per cache level & stride)
############################################
echo "[1/7] Baseline Latency Measurements (per level & stride)..."
mkdir -p "$OUTDIR/01_baseline"

# Define working-set sizes in bytes (approximate for L1, L2, L3, DRAM)
declare -A WSIZES
WSIZES=( ["16KB"]=16 ["256KB"]=256 ["2MB"]=2048 ["16MB"]=16384 ["64MB"]=65536 ["128MB"]=131072)

# Define strides in bytes

# Loop over each level
for level in "${!WSIZES[@]}"; do
    size=${WSIZES[$level]}
    echo "Measuring $level cache with working-set size $size kilobytes..."
        ./mlc --loaded_latency -b${size} -W5 -c0 \
            > "$OUTDIR/01_baseline/loaded_latency_${level}_1to1RW.csv"
done
############################################
# 2. Pattern & Granularity Sweep
############################################
echo "[2/7] Pattern & Granularity Sweep..."
mkdir -p "$OUTDIR/02_pattern_sweep"
for stride in 64 256 512 1024 2048; do
    # Sequential
    ./mlc --idle_latency -c0 -l${stride} \
        > "$OUTDIR/02_pattern_sweep/latency_stride_${stride}_seq.csv"
    ./mlc --peak_injection_bandwidth -c0 -l${stride} \
        > "$OUTDIR/02_pattern_sweep/bandwidth_stride_${stride}_seq.csv"
    
done

############################################
# 3. Read/Write Mix Sweep
############################################
echo "[3/7] Read/Write Mix Sweep..."
mkdir -p "$OUTDIR/03_rw_mix"
for W in 2 3 5 6; do
    ./mlc --loaded_latency -W${W} -c0 \
        > "$OUTDIR/03_rw_mix/bw_W${W}.csv"
done

############################################
# 4. Intensity Sweep (Throughput vs Latency)
############################################
echo "[4/7] Intensity Sweep..."
mkdir -p "$OUTDIR/04_intensity"
for threads in 1 4 16; do
    ./mlc --loaded_latency -t${threads} -c0 \
        > "$OUTDIR/04_intensity/loaded_latency_${threads}t.csv"
done

############################################
# 5. Working-Set Size Sweep
############################################
echo "[5/7] Working-Set Size Sweep..."
mkdir -p "$OUTDIR/05_ws_sweep"
for size in 32 64 512 8192 65536 131072; do
    ./mlc --idle_latency -b${size} \
        > "$OUTDIR/05_ws_sweep/ws_${size}KB.csv"
done

############################################
# 6. Cache-Miss Impact on SAXPY
############################################
echo "[6/7] Cache-Miss Impact on Kernel..."
mkdir -p "$OUTDIR/06_saxpy"
for p in 0.0 0.1 0.25 0.5 0.75 0.9 1.0; do
  $PERF_BIN stat -e cycles,instructions,cache-references,cache-misses \
    ./mix_miss 16384 8388608 64 $p > "$OUTDIR/06_saxpy/mix_p_${p}.csv" 2>&1
done
############################################
# 7. TLB-Miss Impact
############################################
echo "[7/7] TLB-Miss Impact..."
mkdir -p "$OUTDIR/07_tlb"
for P in 4 16 64 128 256 512 1024; do
  $PERF_BIN stat -e cycles,dTLB-loads,dTLB-load-misses \
    ./tlb_test $P 2097152 100000000 > "$OUTDIR/07_tlb/tlb_P${P}.csv" 2>&1
done

echo
echo "✅ All experiments complete!"
echo "Results stored in: $OUTDIR"
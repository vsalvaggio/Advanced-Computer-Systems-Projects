#!/bin/bash
# ==========================================================
# ECSE 4320 Project A1 - Automated Benchmark Runner
# Runs all microbenchmarks sequentially and records perf stats.
# ==========================================================

PERF_BIN="$HOME/WSL2-Linux-Kernel/tools/perf/perf"  # or just "perf" if installed system-wide

set -e
mkdir -p results
make >/dev/null 2>&1 || true

declare -A FILES=(
 ["zero_copy_io"]="zero_copy_io"
 ["async_io"]="async_io"
 ["scheduler_affinity"]="scheduler_affinity"
 ["smt_interference"]="smt_interference"
)

# ==========================================================
# Generic benchmark runner
# ==========================================================
run_bench () {
    exe=$1
    csv="results/${exe}.csv"
    echo "Run,Mode,Time_s" > "$csv"

    for i in {1..3}; do
        echo "[$exe] run $i"
        $PERF_BIN stat -e cycles,instructions,cache-misses -x, -o perf_tmp.txt -- ./$exe > out_tmp.txt 2>/dev/null
        while IFS=, read -r mode t; do
            [ -z "$mode" ] && continue
            echo "$i,$mode,$t" >> "$csv"
        done < out_tmp.txt
    done

    rm -f out_tmp.txt perf_tmp.txt
    echo "✅ Results saved to $csv"
}

# ==========================================================
# Main Loop
# ==========================================================
for k in "${!FILES[@]}"; do
    run_bench "${FILES[$k]}"
done

echo "=========================================================="
echo "✅ All benchmarks complete! CSV results stored in ./results/"
echo "=========================================================="

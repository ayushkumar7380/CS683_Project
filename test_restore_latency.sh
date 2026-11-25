#!/usr/bin/env bash

set -euo pipefail
cd "$(dirname "$0")"
case "${1:---quick}" in
  --quick) shapes=("12288 1024"); penalties=(0 64 256 512 1024) ;;
  --full) shapes=("8192 1024" "12288 1024" "16384 1024"); penalties=(0 16 64 128 256 512 1024) ;;
  *) echo 'Usage: bash test_restore_latency.sh [--quick|--full]' >&2; exit 2 ;;
esac
scons
mkdir -p restore_latency_results
for shape in "${shapes[@]}"; do
  read -r out in <<< "$shape"
  python3 generate_scaling_data.py "$out" "$in"
  for cache in 0 1; do
    if [[ "$cache" == 0 ]]; then run_penalties=(0); else run_penalties=("${penalties[@]}"); fi
    for penalty in "${run_penalties[@]}"; do
      stem="restore_latency_results/${out}x${in}_cache${cache}_penalty${penalty}"
      echo "[RESTORE_LATENCY_RUNNING] output=$out input=$in cache=$cache penalty=$penalty correctness"
      PIM_STEP11_OUT="$out" PIM_STEP11_IN="$in" PIM_GEMV_TILE_CACHE="$cache" PIM_STEP13_RESTORE_EXTRA_CYCLES="$penalty" \
         ./sim --gtest_filter=PIMKernelFixture.gemv_step11_scaling > "${stem}_correct.log" 2>&1 || {
           echo "[RESTORE_LATENCY_ERROR] correctness test failed: ${stem}_correct.log" >&2; tail -20 "${stem}_correct.log" >&2; exit 1;
         }
      if ! grep -Eq ">[[:space:]]*passed[[:space:]]*:[[:space:]]*$out([[:space:]]|$)" "${stem}_correct.log" ||
         ! grep -Eq ">[[:space:]]*failed[[:space:]]*:[[:space:]]*0([[:space:]]|$)" "${stem}_correct.log"; then
        echo "[RESTORE_LATENCY_ERROR] incorrect output counts: ${stem}_correct.log" >&2; exit 1
      fi
      echo "[RESTORE_LATENCY_RUNNING] output=$out input=$in cache=$cache penalty=$penalty performance"
      PIM_STEP11_OUT="$out" PIM_STEP11_IN="$in" PIM_GEMV_TILE_CACHE="$cache" PIM_STEP13_RESTORE_EXTRA_CYCLES="$penalty" \
        ./sim --gtest_filter=PIMBenchFixture.gemv_step11_scaling > "${stem}_perf.log" 2>&1 || {
          echo "[RESTORE_LATENCY_ERROR] performance test failed: ${stem}_perf.log" >&2; tail -20 "${stem}_perf.log" >&2; exit 1;
        }
      grep -E '^> Cycle :|^\[STEP13_RUNTIME\]' "${stem}_perf.log"
    done
  done
done
python3 analyze_restore_latency.py restore_latency_results

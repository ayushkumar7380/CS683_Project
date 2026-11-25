#!/usr/bin/env bash

set -euo pipefail
cd "$(dirname "$0")"
mode="${1:---quick}"
case "$mode" in
 --quick) shapes=("8192 1024" "16384 1024") ;;
 --full) shapes=("4096 1024" "8192 1024" "12288 1024" "16384 1024" "8192 512" "16384 512") ;;
 *) echo "Usage: bash test_scalability.sh [--quick|--full]" >&2; exit 2 ;;
esac
scons
mkdir -p scalability_results
for shape in "${shapes[@]}"; do
 read -r out in <<< "$shape"
 python3 generate_scaling_data.py "$out" "$in"
 for cache in 0 1; do
  stem="scalability_results/${out}x${in}_cache${cache}"
  echo "[SCALABILITY_RUNNING] output=$out input=$in cache=$cache correctness"
  PIM_STEP11_OUT="$out" PIM_STEP11_IN="$in" PIM_GEMV_TILE_CACHE="$cache" \
    ./sim --gtest_filter=PIMKernelFixture.gemv_step11_scaling > "${stem}_correct.log" 2>&1 || {
      echo "[SCALABILITY_ERROR] correctness test failed, inspect ${stem}_correct.log" >&2
      tail -15 "${stem}_correct.log" >&2; exit 1;
    }
  if ! grep -Eq ">[[:space:]]*passed[[:space:]]*:[[:space:]]*$out([[:space:]]|$)" "${stem}_correct.log" ||
     ! grep -Eq ">[[:space:]]*failed[[:space:]]*:[[:space:]]*0([[:space:]]|$)" "${stem}_correct.log"; then
      echo "[SCALABILITY_ERROR] incorrect output counts: ${stem}_correct.log" >&2; exit 1
  fi
  echo "[SCALABILITY_RUNNING] output=$out input=$in cache=$cache performance"
  PIM_STEP11_OUT="$out" PIM_STEP11_IN="$in" PIM_GEMV_TILE_CACHE="$cache" \
    ./sim --gtest_filter=PIMBenchFixture.gemv_step11_scaling > "${stem}_perf.log" 2>&1 || {
       echo "[SCALABILITY_ERROR] performance test failed, inspect ${stem}_perf.log" >&2; exit 1;
    }
  grep -E 'BASELINE_PIM_CONFIG|CACHE_ENABLED_CONFIG|BASELINE_PIM_PROFILE|CACHE_ENABLED_PROFILE|Cycle :|Speed-up' "${stem}_perf.log" | tail -5
 done
done
python3 analyze_scalability.py scalability_results

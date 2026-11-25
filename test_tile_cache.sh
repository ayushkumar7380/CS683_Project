#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
scons
python3 generate_gemv_data.py
for mode in 0 1; do
  echo "===== TILE_CACHE mode=$mode ====="
  PIM_GEMV_TILE_CACHE="$mode" ./sim --gtest_filter=PIMKernelFixture.gemv_step10_rectangular > "tile_cache_correct_${mode}.log" 2>&1
  grep -E 'passed :|failed :|BASELINE_PIM_CONFIG|CACHE_ENABLED_CONFIG|BASELINE_PIM_PROFILE|CACHE_ENABLED_PROFILE|\[  PASSED  \]' "tile_cache_correct_${mode}.log" || true
  if ! grep -Eq '>[[:space:]]*failed[[:space:]]*:[[:space:]]*0([[:space:]]|$)' "tile_cache_correct_${mode}.log"; then
    echo "[TILE_CACHE_ERROR] mode=$mode failed numerical correctness. No speedup claim is valid." >&2
    exit 1
  fi
  if ! grep -Eq '>[[:space:]]*passed[[:space:]]*:[[:space:]]*16384([[:space:]]|$)' "tile_cache_correct_${mode}.log"; then
    echo "[TILE_CACHE_ERROR] mode=$mode did not pass all 16384 output comparisons." >&2
    exit 1
  fi
  PIM_GEMV_TILE_CACHE="$mode" ./sim --gtest_filter=PIMBenchFixture.gemv_rectangular_16384x1024 > "tile_cache_perf_${mode}.log" 2>&1
  grep -E 'Cycle :|Speed-up|BASELINE_PIM_CONFIG|CACHE_ENABLED_CONFIG|BASELINE_PIM_PROFILE|CACHE_ENABLED_PROFILE' "tile_cache_perf_${mode}.log"
done
python3 - <<'PY2'
import re
from pathlib import Path
v={}
for mode in (0,1):
    txt=Path(f'tile_cache_perf_{mode}.log').read_text()
    cycles=list(map(int,re.findall(r'> Cycle :\s*(\d+)',txt)))
    if len(cycles)<2: raise SystemExit(f'Expected non-PIM and PIM cycle counts, got {cycles} for {mode}')
    v[mode]=cycles[-1]
print(f'[TILE_CACHE_COMPARISON] baseline_pim_cycles={v[0]} optimized_pim_cycles={v[1]} pim_over_pim_speedup={v[0]/v[1]:.6f}')
PY2

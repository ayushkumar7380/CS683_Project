# PIMSimulator — GRF-A Tile Cache Extension

This is the project source for an **experimental GRF-A input-tile cache extension** to Samsung's PIMSimulator / FIMDRAM model. It is **not** a stock Samsung hardware feature. Cache-control operations (`SELECT`, `RESTORE`) are simulator-specific controls, **not** native Samsung FIMDRAM CRF instructions.

## Project contribution

- An additional **2 KiB per PIM rank** input-tile cache: eight cached tiles, each containing eight 256-bit GRF-A register words (`8 × 8 × 32 bytes = 2048 bytes`, excluding control metadata).
- On the first GEMV output tile, standard GRF-A loading also populates this cache.
- For later output tiles, **SELECT** identifies the cached tile and **RESTORE** reloads its words into GRF-A, avoiding repeated GRF-A payload writes. The original MAC instructions, GRF-B accumulation, and output-tile-major execution order remain unchanged.
- A separate optional simulator experiment models additional **command-issue cooldown after RESTORE**. This is a timing sensitivity test, **not measured physical SRAM access latency**.

## Quick start (Linux / Ubuntu / WSL)

Prerequisites for rebuilding: C++17 compiler, Python 3, NumPy, SCons, and GoogleTest development headers/libraries. Typical Ubuntu setup:

```bash
sudo apt update
sudo apt install -y build-essential python3 python3-numpy scons libgtest-dev
```

The ZIP includes a **prebuilt Linux x86-64 `sim` executable** from the supplied project, so if compatible with your system you can run the smoke test without rebuilding:

```bash
cd PIMSimulator
bash run_project.sh --smoke
```

For a clean build from the included source (recommended for reproducibility):

```bash
scons
./sim --gtest_list_tests
bash run_project.sh --smoke
```

If a test fails, inspect the printed error and check the required dependencies, particularly GoogleTest and SCons. **A prebuilt executable passing a smoke test does not by itself validate a fresh rebuild from cleaned source.**

## Full project experiments

All commands below run from `PIMSimulator/`. Full tests can consume significant memory and time due to the 16K × 1024 FP16 GEMV data.

| Goal | Command | Outputs |
| --- | --- | --- |
| Smoke test | `bash run_project.sh --smoke` | Console output and `project_run_logs/smoke.log` |
| Two-shape baseline/cache comparison | `bash run_project.sh --quick` | `scalability_results/*`, summary CSV |
| All six GEMV shapes | `bash run_project.sh --full` | `scalability_results/scalability_summary.csv` |
| Cache restore penalty (one shape) | `bash run_project.sh --restore` | `restore_latency_results/*`, analysis CSV/MD |
| Detailed command overhead analysis | `bash run_project.sh --overhead` | `scalability_results/command_overhead_report.md` (requires scalability results) |

Equivalent original scripts:

```bash
bash test_scalability.sh --quick
bash test_scalability.sh --full
bash test_restore_latency.sh --quick
bash test_command_overhead.sh --analyze-only
```

The `test_scalability.sh` and `test_restore_latency.sh` scripts call `scons`, generate deterministic input data, run correctness checks **before** timing, and write log files. They will stop on failed numerical verification.

### Manually compare cache OFF vs ON

```bash
python3 generate_scaling_data.py 8192 1024
PIM_STEP11_OUT=8192 PIM_STEP11_IN=1024 PIM_GEMV_TILE_CACHE=0 \
  ./sim --gtest_filter=PIMKernelFixture.gemv_step11_scaling
PIM_STEP11_OUT=8192 PIM_STEP11_IN=1024 PIM_GEMV_TILE_CACHE=1 \
  ./sim --gtest_filter=PIMKernelFixture.gemv_step11_scaling
PIM_STEP11_OUT=8192 PIM_STEP11_IN=1024 PIM_GEMV_TILE_CACHE=0 \
  ./sim --gtest_filter=PIMBenchFixture.gemv_step11_scaling
PIM_STEP11_OUT=8192 PIM_STEP11_IN=1024 PIM_GEMV_TILE_CACHE=1 \
  ./sim --gtest_filter=PIMBenchFixture.gemv_step11_scaling
```

The main environment flags are:

| Variable | Meaning |
| --- | --- |
| `PIM_GEMV_TILE_CACHE=0` or `1` | Disable/enable experimental cache; default OFF |
| `PIM_STEP11_OUT` | GEMV matrix rows / output dimension |
| `PIM_STEP11_IN` | GEMV matrix columns / input dimension |
| `PIM_STEP13_RESTORE_EXTRA_CYCLES` | Additional modeled per-channel command-issue cooldown after RESTORE; default 0 |
| `PIM_TRACE_STEP10=1` | Optional detailed cache-control tracing |

Optimization eligibility is restricted to supported **non-tree, single-batch GEMV**, with at least two output tiles and at most eight input tiles. Other configurations follow the baseline path.

## Expected verified results from the project's earlier experiments

These values are reference results from the previously validated six-workload project experiments; **they are not claims that this cleaned archive was completely rebenchmark-tested here**:

| GEMV (rows × columns) | Baseline PIM cycles | Cache PIM cycles | Reduction |
| --- | ---: | ---: | ---: |
| 4096 × 1024 | 3662 | 3662 | 0.00% |
| 8192 × 1024 | 7037 | 6806 | 3.28% |
| 12288 × 1024 | 10395 | 9576 | 7.88% |
| 16384 × 1024 | 13389 | 12712 | 5.06% |
| 8192 × 512 | 3767 | 3668 | 2.63% |
| 16384 × 512 | 7176 | 6880 | 4.13% |

For 16384 × 1024, the generated GRF-A payload writes changed from 16,384 to 4,096 (**75% reduction**) while all 131,072 MAC instructions were preserved. The largest PIM-cycle reduction was **7.88%**, for 12288 × 1024.

For the 12288 × 1024 restore penalty experiment, PIM cycles for penalties 0, 64, 256, 512, 1024 were respectively **9576, 10698, 14135, 18582, 27464** versus **10395** baseline cycles. The restoration cooldown must be sufficiently small to preserve the benefit.

## Inspect logs and metrics

```bash
cat scalability_results/scalability_summary.csv
cat restore_latency_results/restore_latency_results.csv
find restore_latency_results -maxdepth 1 -type f | sort

grep -hE '\[STEP10_PROFILE\]|\[GEMV_TILE_PROFILE\]|\[STEP13_RUNTIME\]' scalability_results/*_perf.log

grep -hE '> passed :|> failed :' scalability_results/*_correct.log

grep -h '^> Cycle :' scalability_results/*_perf.log
```

**Metric caution:** command counts, diagnostic wait counts aggregated across channels, and end-to-end PIM cycles are different quantities. Do not sum overlapping timing counters as if they were cycle savings. This project does not measure physical SRAM area, energy, or silicon latency.

## Project-specific source files

The following files contain identifiable project-specific logic or instrumentation in this uploaded snapshot. This is **not a verified Git diff against pristine Samsung upstream**, because the archive does not contain an original baseline checkout or Git history.

| File | Project role |
| --- | --- |
| `src/tests/PIMKernel.cpp` | GEMV tile traversal, SELECT/RESTORE command generation, cache-enabled control and profiling |
| `src/tests/PIMKernel.h` | Cache-related state / payload declarations |
| `src/PIMRank.cpp` | Per-rank tile-cache storage, fill, restore and validity logic |
| `src/PIMRank.h` | Cache storage arrays, metadata, counters |
| `src/Rank.cpp` | Cache command routing / dispatch tracing |
| `src/MemoryController.cpp` | RESTORE command recognition and timing cooldown simulation |
| `src/MemoryController.h` | Per-channel RESTORE penalty and count state |
| `src/MultiChannelMemorySystem.cpp` | Aggregate memory-system, scheduling, and RESTORE profiling |
| `src/tests/PIMBenchTestCases.cpp` | Rectangular / scaling GEMV benchmark cases |
| `src/tests/KernelTestCases.cpp` | GEMV correctness validation cases |

Project scripts: `generate_gemv_data.py`, `generate_scaling_data.py`, `analyze_scalability.py`, `analyze_restore_latency.py`, `analyze_command_overhead.py`, `test_tile_cache.sh`, `test_scalability.sh`, `test_restore_latency.sh`, `test_command_overhead.sh`, and `run_project.sh` (convenience wrapper added for this delivery).

## Source cleanup and licensing

The source code was inspected for comments. In this snapshot, project source already had no removable inline C++ comments outside copyright/legal headers; Python and shell files mainly contain executable shebangs. Copyright, license notices, third-party `lib/` headers, `LICENSE-*` files, and documentation were intentionally preserved. **Deleting legal notices could violate licensing conditions.** This distribution removes build intermediates and stale `.sconsign.dblite`, but keeps the provided Linux `sim` binary as an optional convenience.

For further technical background see `docs/STEP9_GEMV_TILE_PROFILE.md`, `docs/STEP10_CACHE_EXPERIMENT.md`, and `docs/STEP11_SCALABILITY.md`. Note that some historical step documents refer to older command encodings or script names: the supplied **current C++ source and runnable scripts take precedence**.

## Readable experiment log tags

New builds emit `[BASELINE_PIM_CONFIG]` and `[BASELINE_PIM_PROFILE]` when cache reuse is inactive; they emit `[CACHE_ENABLED_CONFIG]` and `[CACHE_ENABLED_PROFILE]` when cache reuse is active. The `enabled` field is authoritative: a single-output-tile run may have `requested=1` but `enabled=0` and correctly prints a baseline tag. Legacy `STEP10_*` tags in older logs remain supported by the analysis scripts. Build with `scons` to update the executable before testing these label changes.

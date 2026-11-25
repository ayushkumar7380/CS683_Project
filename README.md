# Cache-Enabled PIMSimulator for GEMV

## About the project

This is my CS683 project on improving GEMV (General Matrix-Vector Multiplication) performance in Samsung's PIMSimulator. I started by profiling GEMV execution to understand where time was being spent. One issue I found was that the same input-vector data was loaded into GRF-A multiple times while processing different output tiles.

To reduce these repeated loads, I added an experimental GRF-A tile cache to the simulator. The idea is to store input operands when they are loaded for the first time and reuse them for later output tiles instead of sending the same GRF-A payload writes again.

This is a **simulator-level extension**, not a feature of the original Samsung FIMDRAM hardware.

## What I changed

- Added a **2 KiB input-tile cache per PIM rank** to hold reusable GRF-A operands. It stores eight tiles, each containing eight 256-bit words.
- Added two cache-control operations: **SELECT** chooses a stored input tile, and **RESTORE** copies it back into GRF-A.
- Updated the GEMV command generator to use normal loading for the first output tile and cache-based reuse for later output tiles.
- Kept the existing MAC operations, GRF-B accumulation, and output-tile-major execution order unchanged.
- Added profiling and an optional RESTORE-overhead experiment to understand when the cache helps and when its extra cost becomes too high.

SELECT and RESTORE are experimental simulator controls, not native Samsung FIMDRAM instructions.

## Setup

The project is intended to run on Linux or Ubuntu/WSL. Install the build tools and Python dependencies:

```bash
sudo apt update
sudo apt install -y build-essential scons libgtest-dev python3 python3-numpy
```

Clone the repository and build the simulator:

```bash
ggit clone https://github.com/ayushkumar7380/CS683_Project.git
cd CS683_Project
scons
```

The compiled simulator executable is named `sim`.

## Extracting GEMV data from the ZIP

I keep the larger GEMV input files in a ZIP archive rather than directly in the repository. If you have downloaded the data archive, place it in the **same folder as `extract_gemv_data.py`** (the `PIMSimulator/` directory). Name it `data.zip`, or specify its filename when running the script.

```bash
# Run from the PIMSimulator/ folder
python3 extract_gemv_data.py
```

If your archive uses a different name, for example `GEMV_inputs.zip`:

```bash
python3 extract_gemv_data.py GEMV_inputs.zip
```

The script extracts the files to `PIMSimulator/data/`. It works whether the ZIP contains a top-level `data/` directory or files directly inside the archive. It keeps existing data files by default; add `--overwrite` only if you want to replace them.

```bash
# Check the extracted data
ls data
du -sh data
```

**Note:** A large data ZIP may be too big for a normal GitHub Git push (GitHub blocks individual Git files above 100 MiB). In that case, share the archive separately, or let the existing experiment scripts generate the GEMV input files locally. The `run_project.sh --full` workflow generates its benchmark inputs and does not require this ZIP for those tests.

## Running the experiments

The easiest way to run the project is through `run_project.sh`:

```bash
# Run a basic smoke test
bash run_project.sh --smoke

# Run a smaller baseline/cache comparison
bash run_project.sh --quick

# Run all six GEMV configurations
bash run_project.sh --full

# Test the effect of extra RESTORE overhead
bash run_project.sh --restore
```

The scripts can generate the required GEMV input data locally, run correctness and performance tests, and save the results. If you already have the GEMV data ZIP, extract it using the command above.

To view the six-workload results:

```bash
cat scalability_results/scalability_summary.csv
```

## Results

I compared baseline PIM execution with the cache-enabled version across six GEMV workloads.

| GEMV matrix | Baseline PIM cycles | Cache-enabled PIM cycles | Cycle reduction |
|---|---:|---:|---:|
| 4096 × 1024 | 3,662 | 3,662 | 0.00% |
| 8192 × 1024 | 7,037 | 6,806 | 3.28% |
| 12288 × 1024 | 10,395 | 9,576 | **7.88%** |
| 16384 × 1024 | 13,389 | 12,712 | 5.06% |
| 8192 × 512 | 3,767 | 3,668 | 2.63% |
| 16384 × 512 | 7,176 | 6,880 | 4.13% |

For the **16384 × 1024** workload, generated GRF-A payload writes went from **16,384 to 4,096**, a **75% reduction**. The required MAC instruction counts stayed the same, and correctness checks passed.

The single-output-tile case did not improve because there was no opportunity to reuse the input vector across output tiles. Another important finding was that fewer GRF-A writes do not translate directly into the same percentage of cycle savings: command scheduling, DRAM timing, and RESTORE operations also contribute to execution time.

I also varied the modeled RESTORE penalty. For the 12288 × 1024 workload, the cache helped at zero extra penalty, but became slower than baseline at the tested penalty of 64 command-issue cycles per RESTORE. This is a **simulated command-issue cooldown**, not a measured SRAM latency.

## Main implementation files

| File | Main purpose |
|---|---|
| `src/tests/PIMKernel.cpp` | GEMV tile traversal and cache-control command generation |
| `src/tests/PIMKernel.h` | Cache-related declarations |
| `src/PIMRank.cpp` / `src/PIMRank.h` | Cache storage, fill, selection, and restore state |
| `src/Rank.cpp` | Command handling and tracing |
| `src/MemoryController.cpp` / `src/MemoryController.h` | RESTORE timing model and related counters |
| `src/MultiChannelMemorySystem.cpp` | Memory-system profiling |
| `src/tests/PIMBenchTestCases.cpp` | Performance test cases |
| `src/tests/KernelTestCases.cpp` | Correctness test cases |

The repository also contains the Python data-generation scripts and Bash scripts used to run and analyze the experiments.

## Conclusion

The experiments showed that keeping reusable input tiles close to GRF-A can remove a large number of repeated operand-loading commands and improve simulated GEMV performance. The best cycle reduction I observed was **7.88%**, with up to **75% fewer generated GRF-A payload writes**. A useful next step would be to compare different cache sizes and evaluate physical area, energy, and realistic restore latency.

## Acknowledgment

This project builds on Samsung's PIMSimulator/FIMDRAM research simulator. The original source code and its license notices belong to their respective authors.

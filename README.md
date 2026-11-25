# Cache-Enabled PIMSimulator for GEMV

## About the project

This is my CS683 project on improving GEMV (General Matrix-Vector Multiplication) performance in Samsung's PIMSimulator. While profiling GEMV execution, I found that the same input-vector data was loaded into GRF-A repeatedly when the simulator moved between output tiles.

To reduce those repeated transfers, I added an experimental GRF-A tile cache. Input operands are cached when they are first loaded and restored for later output tiles instead of issuing the same payload writes again.

**This is an experimental simulator-level extension, not a feature of the original Samsung FIMDRAM hardware.**

## What I changed

- Added a **2 KiB input-tile cache per PIM rank**, storing eight tiles of eight 256-bit words each (excluding metadata).
- Introduced two simulator-specific cache-control operations: **SELECT** chooses a cached tile and **RESTORE** copies it back to GRF-A.
- Modified the GEMV command generator to use normal operand loads for the first output tile and cache-based reuse for subsequent output tiles.
- Kept the original MAC instructions, GRF-B accumulation, and output-tile-major execution order unchanged.
- Added profiling and an optional RESTORE command-issue overhead experiment.

SELECT and RESTORE are **not** native instructions in Samsung's original FIMDRAM ISA.

## Requirements

The project is intended for Linux or Ubuntu/WSL. Install the required tools:

```bash
sudo apt update
sudo apt install -y git build-essential scons libgtest-dev python3 python3-numpy
```

## Setup and dataset download

I have split the GEMV input dataset across two GitHub repositories so that it does not need to be committed as a single large file in the simulator repository:

- [GEMV dataset — part 1](https://github.com/ayushkumar7380/data1_cs_683project)
- [GEMV dataset — part 2](https://github.com/ayushkumar7380/data2_cs683_project)

The included `setup_data.sh` script clones both data repositories, combines the supported GEMV input files, and places them in `data/gemv/`. It checks for file-name conflicts rather than silently overwriting existing data. The dataset repositories must be accessible to the person running the script.

**Clone and prepare the project:**

```bash
git clone https://github.com/ayushkumar7380/CS683_Project.git
cd CS683_Project

# Download and combine the two GEMV dataset parts
bash setup_data.sh

# Confirm the data is available
ls data/gemv
find data/gemv -name '*.npy' | wc -l

# Build the simulator
scons
```

The compiled simulator executable is named `sim`.

**Note:** `setup_data.sh` needs to be present in the root of this repository. If the two dataset repositories are private, users need read access or suitable GitHub authentication. I have not independently verified a fresh clone of both dataset repositories; test the setup on a clean checkout before relying on it for reproduction.

The project also includes Python data-generation scripts. In particular, the full scalability workflow generates its own required GEMV benchmark inputs, so those experiments may be run without downloading the pre-generated dataset.

## Running the experiments

Run the commands from the project root after building:

```bash
# Basic smoke test
bash run_project.sh --smoke

# Smaller baseline-vs-cache comparison
bash run_project.sh --quick

# All six GEMV configurations
bash run_project.sh --full

# Optional RESTORE-overhead experiment
bash run_project.sh --restore
```

To view the six-workload summary:

```bash
cat scalability_results/scalability_summary.csv
```

## Results

I compared baseline PIM execution with my cache-enabled version across six GEMV workloads.

| GEMV matrix | Baseline PIM cycles | Cache-enabled PIM cycles | Cycle reduction |
|---|---:|---:|---:|
| 4096 × 1024 | 3,662 | 3,662 | 0.00% |
| 8192 × 1024 | 7,037 | 6,806 | 3.28% |
| 12288 × 1024 | 10,395 | 9,576 | **7.88%** |
| 16384 × 1024 | 13,389 | 12,712 | 5.06% |
| 8192 × 512 | 3,767 | 3,668 | 2.63% |
| 16384 × 512 | 7,176 | 6,880 | 4.13% |

For the **16384 × 1024** workload, generated GRF-A payload writes fell from **16,384 to 4,096**, a **75% reduction**. MAC instruction counts remained unchanged and the correctness checks passed.

The 4096 × 1024 workload showed no benefit because it only uses one output tile, leaving no opportunity to reuse operands across output tiles. Fewer operand writes also do not guarantee the same percentage of cycle savings: DRAM timing constraints, command scheduling, and other operations still affect execution time.

I also tested the sensitivity to modeled RESTORE overhead. On the 12288 × 1024 workload, the cache helped with zero extra penalty but became slower than baseline when a penalty of 64 command-issue cycles per RESTORE was applied. **This models command-issue cooldown, not measured SRAM latency.**

## Main implementation files

| File | Purpose |
|---|---|
| `src/tests/PIMKernel.cpp` | GEMV tile traversal and cache-control command generation |
| `src/tests/PIMKernel.h` | Cache-related declarations |
| `src/PIMRank.cpp` / `src/PIMRank.h` | Cache storage, filling, selection, and restore logic |
| `src/Rank.cpp` | Command handling and tracing |
| `src/MemoryController.cpp` / `src/MemoryController.h` | RESTORE timing model and counters |
| `src/MultiChannelMemorySystem.cpp` | Memory-system profiling |
| `src/tests/PIMBenchTestCases.cpp` | Performance test cases |
| `src/tests/KernelTestCases.cpp` | Correctness test cases |
| `setup_data.sh` | Download and merge the two dataset repositories into `data/gemv/` |

The repository also includes data-generation scripts, experiment runners, and analysis scripts.

## Conclusion

The experiments showed that retaining reusable input-vector tiles near GRF-A can eliminate many redundant operand-loading commands and improve simulated GEMV performance. I observed up to **75% fewer generated GRF-A payload writes** and a maximum **7.88% reduction in simulated PIM cycles**, while retaining numerical correctness. Future work could explore other cache sizes and evaluate physical area, energy, and realistic hardware restore latency.

## Acknowledgment

This project builds on Samsung's PIMSimulator/FIMDRAM research simulator. The original source code and its license notices belong to their respective authors.

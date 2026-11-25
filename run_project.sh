#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p project_run_logs
case "${1:---smoke}" in
  --smoke)
    if [[ ! -x ./sim ]]; then echo 'Missing executable ./sim. Install dependencies and run: scons' >&2; exit 1; fi
    ./sim --gtest_filter=PIMBenchFixture.gemv_small > project_run_logs/smoke.log 2>&1
    grep -E 'Cycle :|\[  PASSED  \]|\[  FAILED  \]' project_run_logs/smoke.log | tail -12
    echo 'Full smoke-test log: project_run_logs/smoke.log'
    ;;
  --quick) bash test_scalability.sh --quick ;;
  --full) bash test_scalability.sh --full ;;
  --restore) bash test_restore_latency.sh --quick ;;
  --overhead) bash test_command_overhead.sh --analyze-only ;;
  *) echo 'Usage: bash run_project.sh [--smoke|--quick|--full|--restore|--overhead]' >&2; exit 2 ;;
esac

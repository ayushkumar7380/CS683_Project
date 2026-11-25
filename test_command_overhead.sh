#!/usr/bin/env bash

set -euo pipefail
cd "$(dirname "$0")"
case "${1:---analyze-only}" in
  --analyze-only)
    if ! compgen -G 'scalability_results/*_cache0_perf.log' > /dev/null; then
      echo '[COMMAND_OVERHEAD_ERROR] No scalability logs. Run: bash test_command_overhead.sh --quick or --full' >&2
      exit 1
    fi
    ;;
  --quick) bash test_scalability.sh --quick ;;
  --full) bash test_scalability.sh --full ;;
  *) echo 'Usage: bash test_command_overhead.sh [--analyze-only|--quick|--full]' >&2;exit 2;;
esac
python3 analyze_command_overhead.py scalability_results
printf '\n[COMMAND_OVERHEAD_PREVIEW]\n'
head -n 8 scalability_results/command_overhead_report.md

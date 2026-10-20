#!/usr/bin/env bash
# Train the Stage 2 RMIs: ranks chosen by sosd_select.R (7 = latency, 10 = throughput) for every
# dataset on each confirmation seed. Run inside WSL with nothing else running.
set -euo pipefail
cd "$(dirname "$0")/.."
for seed in 20260910 20260911 20260912; do
  for ds in books wiki fb osm; do
    bash analysis/rmi_build.sh "$ds" "$seed" "7,10" > "data/rmi/build_${ds}_${seed}.log" 2>&1
    tail -1 "data/rmi/build_${ds}_${seed}.log"
  done
done

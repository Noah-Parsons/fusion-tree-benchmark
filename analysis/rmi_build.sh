#!/usr/bin/env bash
# rmi_build.sh — train RMIs and generate their C++ code (run inside Linux/WSL).
#
#   analysis/rmi_build.sh <dataset> <seed> [ranks]
#
# Uses the RMI optimizer's Pareto list for the dataset
# (data/rmi/optimize/opt_<dataset>.json, from the tuning seed), orders it by
# model size (rank 1 = smallest), and trains the listed ranks (default: all)
# on data/samples/<dataset>_<seed>. Generated code goes to data/rmi/gen/,
# parameters to data/rmi/params/, and one line per RMI is added to
# data/rmi/manifest.csv. Then run: python analysis/rmi_registry.py
#
# Needs the reference RMI tool: https://github.com/learnedsystems/RMI
# (built with cargo; set RMI_BIN if it is not at ~/RMI/target/release/rmi).
set -euo pipefail
DATASET=$1
SEED=$2
RANKS=${3:-all}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
RMI_BIN=${RMI_BIN:-$HOME/RMI/target/release/rmi}
WORK=${RMI_WORK:-$HOME/rmi_work}
mkdir -p "$WORK" "$ROOT/data/rmi/gen" "$ROOT/data/rmi/params"

# The tool reads the key type from the file name suffix.
cp "$ROOT/data/samples/${DATASET}_${SEED}" "$WORK/${DATASET}_${SEED}_uint64"

cd "$WORK"
python3 "$ROOT/analysis/rmi_grid.py" "$ROOT/data/rmi/optimize/opt_${DATASET}.json" \
    "$DATASET" "$SEED" "$RANKS" "grid_${DATASET}_${SEED}.json" "$ROOT/data/rmi/manifest.csv"
"$RMI_BIN" "${DATASET}_${SEED}_uint64" --param-grid "grid_${DATASET}_${SEED}.json" \
    -d "$ROOT/data/rmi/params" --threads 8 --zero-build-time
mv rmi_"${DATASET}"_"${SEED}"_r*.cpp rmi_"${DATASET}"_"${SEED}"_r*.h "$ROOT/data/rmi/gen/"
mv rmi_"${DATASET}"_"${SEED}"_r*_data.h "$ROOT/data/rmi/gen/" 2>/dev/null || true
rm -f "${DATASET}_${SEED}_uint64"
echo "built RMIs for $DATASET $SEED (ranks: $RANKS)"

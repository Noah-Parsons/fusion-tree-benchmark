#!/usr/bin/env bash
# run_remaining_experiments.sh — fills in what's missing from the fusion-tree study:
#
#   1. node_lat   (Experiment 3a) — never run: dependency-chain cost of one
#                  node search, in cycles. Fits in L1, so it's memory-free.
#   2. spread     (Experiment 1)  — never run: does K=8/K=16 actually fit a
#                  64-/256-bit word in practice, across K=2..16?
#   3. full 10-structure timing sweep (throughput + latency) — timing_raw/lat
#                  only covered 4 of the 10 structures bench_pred.cpp defines.
#                  This gets real wall-clock numbers for fusion8_c, fusion8_bf,
#                  fusion16_w, btree8_a64, btree8_bl_a64, btree16_bl_a64.
#   4. 2x2 factorial: blocked/shuffled x pinned/unpinned — tests whether
#                  "shuffled beat blocked" and "unpinned beat pinned" in your
#                  earlier data are actually the same underlying cause
#                  (sustained load throttling one pinned core).
#
# Run from the project root, in MSYS2/Git-Bash. Set CPU to your performance
# core if it isn't 8 — check results/machine.txt.

set -euo pipefail

CPU=8
REPS=15
MAXLOG=25        # matches the range already in your timing_raw/timing_lat

mkdir -p results

echo "== building bench_pred, node_lat, spread =="
make build/bench_pred build/node_lat build/spread

echo "== 1/4: node_lat (Experiment 3a, seconds) =="
./build/node_lat "$REPS" "$CPU" > results/node_lat.csv

echo "== 2/4: spread (Experiment 1, seconds) =="
./build/spread 2000 > results/spread.csv

echo "== 3/4: full 10-structure timing sweep (throughput + latency) =="
./build/bench_pred "$REPS" "$MAXLOG" "$CPU" tput shuffled all > results/timing_all_tput.csv
./build/bench_pred "$REPS" "$MAXLOG" "$CPU" lat  shuffled all > results/timing_all_lat.csv

echo "== 4/4: 2x2 factorial — order x pinning (sorted_array, btree8, fusion8) =="
./build/bench_pred "$REPS" "$MAXLOG" "$CPU" tput blocked  sorted_array,btree8,fusion8 > results/factorial_blocked_pinned.csv
./build/bench_pred "$REPS" "$MAXLOG" -1     tput blocked  sorted_array,btree8,fusion8 > results/factorial_blocked_unpinned.csv
./build/bench_pred "$REPS" "$MAXLOG" "$CPU" tput shuffled sorted_array,btree8,fusion8 > results/factorial_shuffled_pinned.csv
./build/bench_pred "$REPS" "$MAXLOG" -1     tput shuffled sorted_array,btree8,fusion8 > results/factorial_shuffled_unpinned.csv

echo "done — new files in results/:"
ls -la results/node_lat.csv results/spread.csv \
       results/timing_all_tput.csv results/timing_all_lat.csv \
       results/factorial_blocked_pinned.csv results/factorial_blocked_unpinned.csv \
       results/factorial_shuffled_pinned.csv results/factorial_shuffled_unpinned.csv

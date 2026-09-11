# Fusion Tree — Galactic Algorithms, Measured

Reference implementation and measurement harness.
See the work logs (`Work_Log_*.docx`) for what was changed, measured and found.

## Quick start

    make test        # 20.5M differential checks (8.48M node + 12M tree), must report 0 failures
    make test-loop   # the same tests with the O(r) loop sketch
    make spread      # Experiment 1 (K = 2..16, 64- and 256-bit words) -> results/spread.csv
    make bench       # Experiment 2, throughput -> results/timing_raw.csv
    make bench-lat   # Experiment 2, latency    -> results/timing_lat.csv
    make node-lat    # Experiment 3a: one node search in cycles -> results/node_lat.csv
    make traffic     # Experiment 3b: cache lines per query + modelled misses -> results/traffic.csv
    make figures     # requires R + ggplot2

    powershell -ExecutionPolicy Bypass -File analysis/final_campaign.ps1   # final campaign, ~1 hour
    Rscript analysis/final.R                                              # final tables and figures

Every target builds with clang too: `make test CXX=clang++ BUILD=build_clang`.

`CPU=8` (default) pins the benchmark to logical processor 8, a performance
core on the Core Ultra 7 255HX. This CPU is hybrid: see `results/machine.txt`
for which logical processors are performance cores. `MAXLOG=25` extends a
campaign to 2^25 keys. Any single campaign can be analysed with a tag:

    Rscript analysis/analyse.R results/timing_lat.csv lat

## The structures

| name | what it is | node bytes |
|---|---|---|
| sorted_array | branchless binary search | — |
| btree8 | B-tree, 8 keys, early-exit scan | 112 |
| btree8_bl | B-tree, 8 keys, branch-free scan | 112 |
| btree8_a64 / btree8_bl_a64 | the same, nodes aligned to 64 bytes | 128 |
| btree16_bl_a64 | B-tree, 16 keys, branch-free, aligned | 256 |
| fusion8 | the original fusion tree | 184 |
| fusion8_bf | same layout, branch-free rank | 184 |
| fusion8_c | compact node (keys, sketches, mask), branch-free, aligned | 128 |
| fusion16_w | K = 16 fusion node on a 256-bit AVX2 word | 320 |

## Building on Windows (MSYS2 ucrt64)

Run `make` from PowerShell with `C:\msys64\ucrt64\bin;C:\msys64\usr\bin` on
PATH and `TMP`/`TEMP` pointing at a writable directory. From Git Bash, g++
fails with "Cannot create temporary file in C:\Windows\". With
`C:\msys64\usr\bin` first on PATH, `cmd` resolves to an MSYS script; call
`$env:ComSpec` for the real `cmd.exe`.

## Layout

    include/bits.hpp                msb, popcount, naive bit extraction
    include/sketch_fast.hpp         PEXT sketch + Fredman-Willard multiplier search
    include/fusion_node.hpp         the fusion node, and the shared branch-free rank
    include/fusion_node_compact.hpp the 88-byte fusion node (128-byte tree node)
    include/fusion_node_wide.hpp    the 16-key fusion node on a 256-bit word
    include/structures.hpp          FusionTreeT, BTree variants, SortedArray
    tests/                          differential tests against brute force and std::lower_bound
    bench/                          bench_pred (timing), spread, node_lat, traffic
    analysis/                       analyse.R (one campaign), final.R (all), final_campaign.ps1

Without BMI2, drop `-mbmi2 -DFT_USE_PEXT` from the Makefile.

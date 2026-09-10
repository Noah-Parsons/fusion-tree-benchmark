# Fusion Tree — Galactic Algorithms, Measured

Reference implementation and measurement harness for the STEM Lab 2026–27 project.
See `Fusion_Tree_Project_Manual.md` for the complete manual, and
`Work_Log_2026-09-10.docx` for the 10 September changes and measurements.

## Quick start

    make test        # 6.5M differential checks (1.72M node + 4.8M tree), must report 0 failures
    make test-loop   # the same tests with the O(r) loop sketch
    make spread      # Experiment 1 -> results/spread.csv
    make bench       # Experiment 2, throughput -> results/timing_raw.csv
    make bench-lat   # Experiment 2, latency    -> results/timing_lat.csv
    make bench-loop  # throughput, loop sketch  -> results/timing_loop.csv
    make figures     # requires R + ggplot2

`CPU=8` (default) pins the benchmark to logical processor 8, a performance
core on the Core Ultra 7 255HX. This CPU is hybrid: see `results/machine.txt`
for which logical processors are performance cores. `MAXLOG=25` extends the
campaign to 2^25 keys. Any campaign can be analysed with a tag:

    Rscript analysis/analyse.R results/timing_lat.csv lat

## Building on Windows (MSYS2 ucrt64)

Run `make` from PowerShell with `C:\msys64\ucrt64\bin;C:\msys64\usr\bin` on
PATH and `TMP`/`TEMP` pointing at a writable directory. From Git Bash, g++
fails with "Cannot create temporary file in C:\Windows\". With
`C:\msys64\usr\bin` first on PATH, `cmd` resolves to an MSYS script; call
`$env:ComSpec` for the real `cmd.exe`.

## Layout

    include/bits.hpp          msb, popcount, naive bit extraction
    include/sketch_fast.hpp   PEXT sketch + Fredman-Willard multiplier search
    include/fusion_node.hpp   the fusion node (important bits, parallel comparison, rank)
    include/structures.hpp    FusionTree, BTree baselines (early-exit and branch-free), SortedArray
    tests/                    differential tests against brute force and std::lower_bound
    bench/                    the two experiments
    analysis/analyse.R        figures, bootstrap intervals and the ratio model

Without BMI2, drop `-mbmi2 -DFT_USE_PEXT` from the Makefile.

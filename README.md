# Fusion Tree — Galactic Algorithms, Measured

Reference implementation and measurement harness for the STEM Lab 2026–27 project.
See `Fusion_Tree_Project_Manual.md` for the complete manual.

## Quick start

    make test      # 5.3M differential checks, must report 0 failures
    make spread    # Experiment 1 -> results/spread.csv
    make bench     # Experiment 2 -> results/timing_raw.csv
    make figures   # requires R + ggplot2

## Layout

    include/bits.hpp          msb, popcount, naive bit extraction
    include/sketch_fast.hpp   PEXT sketch + Fredman-Willard multiplier search
    include/fusion_node.hpp   the fusion node (important bits, parallel comparison, rank)
    include/structures.hpp    FusionTree, BTree baseline, SortedArray baseline
    tests/                    differential tests against brute force and std::lower_bound
    bench/                    the two experiments
    analysis/analyse.R        figures and the ratio model

Without BMI2, drop `-mbmi2 -DFT_USE_PEXT` from the Makefile.

# Fusion Tree — Galactic Algorithms, Measured

The fusion tree (Fredman & Willard, 1993) answers predecessor queries in
O(log_w n) time, beating the Ω(log n) comparison bound. It is the textbook
example of a *galactic algorithm*: provably faster, universally assumed to be
useless in practice, and almost never measured. This repository measures it.

**Research question.** Does a correctly implemented fusion tree ever beat a
cache-conscious B-tree or binary search on real hardware, and if not, which
mechanism is responsible?

## The result

**No, at no size — and the gap grows.** Nanoseconds per predecessor query at
2^25 keys (33.5 million), g++, median of three sessions on an idle machine:

| structure | throughput | latency |
|---|---|---|
| B-tree, 8 keys, branch-free (AVX2) | **186** | **475** |
| B-tree, 8 keys, early-exit | 442 | 468 |
| fusion tree, compact 128-byte node | 527 | 537 |
| fusion tree, original | 579 | 577 |
| fusion tree, 16 keys on a 256-bit word | 576 | 611 |
| binary search | 723 | 1407 |

Four independent lines of evidence agree on why:

1. **It executes more instructions, not fewer.** Measured with hardware
   counters (Intel VTune): 441 instructions per query against the B-tree's
   200, and 730 cycles against 291 — 2.2× the work, 2.5× the cycles.
2. **Its node search is a long dependency chain.** ~60 cycles against ~28
   for a vectorised B-tree scan, with everything resident in L1.
3. **Memory traffic is a minor factor.** Given a node of identical size
   (128 bytes) and identical measured cache-line traffic, the fusion tree is
   still 2.8× slower in throughput and 1.2× in latency at 2^25.
4. **No crossover exists.** The latency ratio falls (1.8× → 1.2×) only
   because both structures wait longer on memory as n grows; the *absolute*
   deficit rises from 18 ns to 102 ns per query. A method that falls further
   behind cannot cross over.

**The 1993 construction does not fit a 64-bit word.** Its multiplication
sketch packs K fields only for K ≤ 4 (K = 8: 0% of random key sets). The
implementations here are viable only because of `PEXT` (BMI2, 2013). A
256-bit word does not rescue it either: K ≤ 6.

Full write-ups: `Experiments_Report_2026-09-10.docx` (all four experiments,
mechanism, ablations) and `Work_Log_2026-09-10.docx` (what was built and why).
Figures: `figures/final_*_v3.png`.

## Scale of the measurement

- **Correctness:** 8,483,288 node checks and 12,000,000 tree checks against
  brute force and `std::lower_bound`, zero failures, g++ 16.1 and clang 22.1.
- **Timing:** 10 structures × 2^8–2^25 keys × 15 repetitions × 3 sessions ×
  2 compilers × 2 modes (throughput/latency) × 2 run orders = 48,600 timings.
- **Mechanism:** hardware counters, per-node cycle timing, exact cache-line
  counts with a modelled hierarchy, and ablations isolating branches,
  node size and word width.

Every structure returns an identical checksum at every size in every run, so
all of them answered the same question.

## Reproduce

    make test                                   # correctness, ~2 minutes
    make spread                                 # Experiment 1
    make node-lat                               # Experiment 3a
    make traffic MAXLOG=25                      # Experiment 3b
    powershell -ExecutionPolicy Bypass -File analysis/final_campaign.ps1 -OutDir results/final_v4
    Rscript analysis/final.R results/final_v4 v4

A single campaign instead of the full set:

    make bench       # throughput -> results/timing_raw.csv
    make bench-lat   # latency
    Rscript analysis/analyse.R results/timing_lat.csv lat

Every target builds with clang: `make test CXX=clang++ BUILD=build_clang`.
`CPU=8` pins to a performance core of the Core Ultra 7 255HX (this CPU is
hybrid — see `results/machine.txt`); `MAXLOG` sets the largest size.

## The structures

| name | what it is | node bytes |
|---|---|---|
| sorted_array | branchless binary search | — |
| btree8 | B-tree, 8 keys, early-exit scan | 112 |
| btree8_bl | B-tree, 8 keys, branch-free AVX2 scan | 112 |
| btree8_a64 / btree8_bl_a64 | the same, nodes aligned to 64 bytes | 128 |
| btree16_bl_a64 | B-tree, 16 keys, branch-free, aligned | 256 |
| fusion8 | the original fusion tree | 184 |
| fusion8_bf | same layout, branch-free rank | 184 |
| fusion8_c | compact node (keys, sketches, mask), aligned | 128 |
| fusion16_w | K = 16 fusion node on a 256-bit AVX2 word | 320 |

Each pair differs in exactly one thing, so a difference in time has one cause.
The trees share arity, separators, build procedure and descent loop; only the
search inside a node differs.

## Results files

    results/final_v3/            the clean campaign — 18 runs, use this one
    results/final_summary_v3.csv medians and session ranges
    results/final_ratios_v3.csv  every comparison, with session ranges
    results/final_trends_v3.csv  fitted trends and crossover tests
    results/vtune_counters.csv   measured counters (two clean runs, two discarded and marked)
    results/node_lat*.csv        one node search, in cycles (g++ and clang)
    results/traffic.csv          cache lines per query + modelled misses
    results/spread.csv           Experiment 1, K = 2..16
    results/machine.txt          the machine, recorded once
    results/timing_*.csv         earlier single-session campaigns
    results/factorial_*.csv      run order × CPU pinning
    results/final/, final_v1_*, final_v2_*   superseded (see report §7)

`results/final/` and `final_v1_*` are kept deliberately: that campaign used a
B-tree baseline the compiler had silently de-vectorised, which halved its
speed and changed a headline ratio from 3.0× to 1.2×. The report explains it;
the data stays as a record. Do not draw conclusions from those files.

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
    tests/                          differential tests
    bench/                          bench_pred (timing), spread, node_lat, traffic
    analysis/                       analyse.R, final.R, final_campaign.ps1

Without BMI2, drop `-mbmi2 -DFT_USE_PEXT` from the Makefile; a portable
fallback is used.

STEM Lab 2026–27. MIT licensed.

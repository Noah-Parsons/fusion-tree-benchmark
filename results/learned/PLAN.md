# ClusterJump against the learned indexes: plan, fixed before any timing

Written 2026-09-12, after RadixSpline and PGM-index were downloaded and wrapped
(`include/learned_indexes.hpp`) and before either was timed.

## Question

On the four real SOSD datasets, is ClusterJump faster than the best published
learned indexes, RadixSpline and PGM-index? And at what memory cost?

## Fairness

- Both learned indexes finish with the same last step as ClusterJump
  (branch-free halving to at most 8 keys, one AVX2 compare), not with the
  `std::lower_bound` of their examples.
- Their `assert` checks are compiled out, as in their authors' benchmarks.
- One portability patch to RadixSpline, recorded in `third_party/PATCHES.md`.
- Each gets a range of its own recommended settings, chosen by the rule below.

## Stage 1: tuning (exploratory)

- RadixSpline: radix bits R ∈ {18, 22} × error E ∈ {8, 16, 32} (six settings).
- PGM-index: Epsilon ∈ {16, 32, 64, 128} (four settings).
- Also in each run: `splus8`, `splus16`, `clusterjump`.
- books, wiki, fb, osm × throughput/latency, seed 20260909, 7 repetitions,
  2^8 to 2^25 keys, core 8, shuffled order, g++.

**Selection rule.** Separately for each library and each mode, at n = 2^25,
take each setting's time ÷ the faster S+ tree on each dataset, and choose the
setting with the lowest geometric mean over the four datasets. Ties within 2%:
the setting using less memory. Each library may therefore get a different
setting for throughput and for latency, which is how they are tuned in
practice.

## Stage 2: confirmation (fresh keys)

- The chosen settings, `clusterjump`, `splus8`, `splus16`.
- Seeds 20260910–12, 15 repetitions, otherwise as Stage 1.

**Claim rule.** In a cell (dataset × mode) at 2^25, ClusterJump **beats** a
learned index if its median is lower in all three sessions, **loses** if
higher in all three, and otherwise **ties**. Every cell is reported.

## Memory

Total bytes per key for every structure at 2^25 on each dataset, keys
included (`bench/index_bytes.cpp`), reported next to the times.

## Expectation, written before running

The learned indexes were designed for exactly this data. I expect them to use
far less memory than ClusterJump, and to beat it in latency on at least books
and wiki, where the key shape is smooth. I expect ClusterJump to stay ahead in
throughput on fb and osm, where its short search matters most. This is a
guess, not a prediction from a model.

## Stage 1 outcome (recorded before Stage 2 started)

18:12–18:19, all 8 runs, checksums agree. `Rscript analysis/learned_select.R`.
Geometric mean over the four datasets of time ÷ faster S+ tree at 2^25:

| setting | throughput | latency | bytes per key (mean) |
|---|---|---|---|
| clusterjump | 0.692 | 1.152 | 8.67 |
| rs18_e16 (chosen, both modes) | 0.847 | 0.853 | 8.20 |
| pgm16 (chosen, both modes) | 1.213 | 0.806 | 8.08 |

Every RadixSpline setting and every PGM setting is in the script's output.
Memory is close for every structure: 8.0 to 9.0 bytes per key, keys included
(a bare array is 8.00). The learned indexes use slightly less.

Early picture, exploratory only: ClusterJump ahead in throughput, both
learned indexes ahead in latency. Stage 2 decides.

## Stage 2 outcome

18:21–18:40, 24 runs, checksums agree. `Rscript analysis/learned_confirm.R`
writes `results/learned_summary.csv`. ClusterJump time ÷ rival time at 2^25,
median of three sessions; verdict by the claim rule:

| dataset | vs RadixSpline, tput | vs PGM, tput | vs RadixSpline, lat | vs PGM, lat |
|---|---|---|---|---|
| books | 0.73 beats | 0.42 beats | 1.21 loses | 1.26 loses |
| wiki | 0.76 beats | 0.42 beats | 1.28 loses | 1.31 loses |
| fb | 0.38 beats | 0.46 beats | 0.90 beats | 1.06 loses |
| osm | 0.68 beats | 0.44 beats | 1.13 loses | 1.12 loses |

The expectation held for throughput and latency. It was wrong about memory:
the learned indexes do not use "far less". Every structure uses 8.0 to 9.0
bytes per key, keys included; ClusterJump 8.3–8.8, RadixSpline (R = 18,
E = 16) 8.1–8.4, PGM (Epsilon = 16) 8.0–8.2.

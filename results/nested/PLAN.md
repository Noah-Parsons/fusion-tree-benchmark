# Clumps inside clumps: plan, fixed before any nested-key timing

Written 2026-09-12, before the `nested` key shape was timed.

## Question

ClusterJump rescales its second table for clusters at one scale. What happens
when clusters are made of smaller clusters?

## Keys

`bench_pred ... nested`: 64 clusters, each 2^40 wide; inside each, 64
sub-clusters, each 2^20 wide. Queries are drawn the same way.

Why this should hurt ClusterJump. Each big cluster lands in one top bucket.
Its second table spreads about n/256 slots across 2^40, so each slot is about
2^23 wide at n = 2^25: wider than a sub-cluster. So a whole sub-cluster, about
8,000 keys, lands in a single slot, and the search falls back to about 13
steps of binary search.

## Runs

- `sorted_array`, `btree8_bl`, `splus8`, `splus16`, `clusterjump`,
  `rs18_e16`, `pgm16`, `spline32`. All settings as already fixed; nothing
  tuned.
- Throughput and latency, 3 sessions (seeds 20260910–12), 15 repetitions,
  2^8–2^25, core 8, shuffled order, g++.

## Claim rule

`Rscript analysis/learned_confirm.R results/nested results/nested_summary.csv`:
at 2^25, ClusterJump **beats** a rival if faster in all three sessions,
**loses** if slower in all three, otherwise **ties**. Rivals: the faster S+
tree, `rs18_e16`, `pgm16`.

## Expectation, written before running

ClusterJump loses its throughput lead over the S+ tree and falls behind it in
latency. The learned indexes and the S+ tree barely change, because their
error bounds and tree shape do not care about the scale of clustering.

## Outcome

18:47–18:52, 6 runs, checksums agree.
`Rscript analysis/learned_confirm.R results/nested results/nested_summary.csv`.
Medians of three sessions at 2^25, ns per search:

| structure | throughput | latency |
|---|---|---|
| rs18_e16 | 104.5 | 255.0 |
| splus8 | 110.4 | 384.4 |
| splus16 | 115.4 | 318.7 |
| pgm16 | 118.9 | 239.5 |
| spline32 | 132.8 | 271.6 |
| btree8_bl | 184.8 | 464.3 |
| **clusterjump** | **196.7** | **519.5** |
| sorted_array | 707.4 | 1395.4 |

**The expectation held.** ClusterJump loses to every rival by the claim rule:
1.78× the faster S+ tree's time in throughput and 1.65× in latency; 1.88× and
2.04× RadixSpline's; 1.64× and 2.18× PGM's. It is even slower than the plain
branch-free B-tree. The S+ tree's times are the same as on uniform keys, as
expected. So ClusterJump's advantage depends on the data having clusters at
one scale, and nested clusters are its real weakness.

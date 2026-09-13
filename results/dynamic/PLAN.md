# Data that changes: plan, fixed before any timing

Written 2026-09-12, after ClusterJumpD, the TLX B+ tree and ALEX passed the
insert test (`tests/test_dynamic.cpp`) and before any of them was timed.

## What the correctness test found first (before any timing)

`tests/test_dynamic.cpp` (std::set oracle, 240 trials, 465,888 checks per
structure, about 1.39 million in all):

- **ClusterJumpD and TLX: no wrong answers, no crashes.**
- **ALEX, three problems, all in ALEX's own code:**
  1. Queries far outside the stored keys gave wrong answers. The cause is in
     `alex_nodes.h`: `int bucketID = model_.predict(key)` overflows an `int`
     for such keys and is clamped to the first child. The wrapper now answers
     those two cases (below the smallest key, above the largest) from the
     smallest and largest key it tracks; all other queries still go to ALEX.
  2. Keys above 2^53 spaced closer than a double can resolve, inserted into
     an index that started empty, gave 31 wrong answers in 2 of 240 trials.
     Not worked around.
  3. One trial of tight clumps far apart, inserted from empty, crashed ALEX
     (`std::bad_array_new_length`). Not worked around.
- The race below always bulk-loads 8 million keys first, and every run's
  checksums must agree, so a wrong ALEX answer in the race would show up.

## Question

Can a version of ClusterJump that accepts inserts (`include/cluster_jump_dynamic.hpp`)
compete with structures built for changing data?

## Racers

- `clusterjump_d`: ClusterJumpD, parameters fixed in its source (16 keys per
  box, about 6 per box after a rebuild).
- `tlx_btree`: the TLX library's B+ tree, default settings.
- `alex`: ALEX (Microsoft Research / MIT), the best-known learned index for
  changing data, default settings.
- Not included: the PGM-index's dynamic version, whose iterator cannot step
  backwards, so it has no efficient "largest key below q".
- Nothing is tuned.

## Workload (`bench/bench_dyn.cpp`)

N = 2^24 distinct keys. Bulk-load a random half, then run N/2 operations in a
fixed random order: inserts of keys from the other half, and predecessor
searches drawn from the keys' own shape. Two mixes: **10% inserts**
(read-heavy) and **50% inserts** (write-heavy). Throughput only: nanoseconds
per operation over the whole sequence. Memory reported at the end of the run.

Key shapes: uniform, clustered, and the SOSD real datasets books, wiki, fb
and osm.

3 sessions (seeds 20260910–12) × 6 shapes × 2 mixes, 5 repetitions each,
core 8, shuffled structure order, g++ -std=c++17.

## Claim rule

In each cell (shape × mix), ClusterJumpD **beats** a rival if its median is
lower in all three sessions, **loses** if higher in all three, and otherwise
**ties**. Every cell is reported.

## Expectation, written before running

- Against the TLX B+ tree: ClusterJumpD wins every read-heavy cell, because a
  search waits on memory about once instead of once per tree level.
- Against ALEX: ClusterJumpD wins on uniform and clustered keys, and ALEX wins
  or ties on the real datasets, which its models were designed for.
- Write-heavy: closer races. A bucket rebuild is expensive when a cluster sits
  in one bucket, so ClusterJumpD may lose on clustered keys and fb.
- Memory: ClusterJumpD uses the most, around twice the keys' own size,
  because its boxes are kept partly empty.

This is a guess, not the output of a model.

## Outcome

19:02–19:32, 36 runs, every run's checksums agree (so ALEX gave no wrong
answers in the race). `Rscript analysis/dynamic_confirm.R` writes
`results/dynamic_summary.csv`. ClusterJumpD time ÷ rival time, median of
three sessions, with the claim-rule verdict:

| keys | vs TLX, 10% inserts | vs ALEX, 10% | vs TLX, 50% inserts | vs ALEX, 50% |
|---|---|---|---|---|
| uniform | 0.14 beats | 0.66 beats | 0.26 beats | 0.95 beats |
| clustered | 0.11 beats | 0.34 beats | 0.21 beats | 0.53 beats |
| books | 0.15 beats | 0.72 beats | 0.27 beats | 0.95 beats |
| wiki | 0.15 beats | 0.72 beats | 0.25 beats | 0.82 beats |
| osm | 0.27 beats | 0.53 beats | 0.51 beats | 0.73 beats |
| **fb** | 0.30 **ties** | 0.67 **ties** | 0.63 **ties** | 0.83 **ties** |

**fb is a tie only because one session went badly wrong.** In session 2
(seed 20260911) ClusterJumpD took about 846 ns per operation with 10%
inserts and about 4,100 ns with 50%, in all five repetitions alike, while
TLX and ALEX ran normally. So it is caused by that key sample, not by noise:
there ClusterJumpD was 3.6× (10%) and 12× (50%) slower than ALEX. In the
other two sessions it won comfortably.

Memory at the end of the run, bytes per key (median): ClusterJumpD 15–19 on
uniform, clustered, books and wiki, and about 25 on fb and osm; ALEX 11.6–16.8;
TLX 13.5–17.5 (computed from its node counts).

Against the expectations:
- TLX: expected a ClusterJumpD win in every read-heavy cell. It won five of
  six; fb tied.
- ALEX: **the guess was wrong.** ALEX was expected to win or tie on the real
  datasets. ClusterJumpD beat it on books, wiki and osm, and tied on fb.
- Write-heavy on clustered keys: expected a possible loss; it won (0.53).
  fb: the bad session appeared, as feared.
- Memory: expected ClusterJumpD to use the most. It did, most of all on fb
  and osm.

## Why fb session 2 went wrong (diagnosed after the race)

A diagnostic program rebuilt each session's exact fb key set and printed
ClusterJumpD's shape (`ClusterJumpD::stats()`, used by no search or insert):

| seed | nonempty top buckets | keys in overflow lists | longest overflow list |
|---|---|---|---|
| 20260910 (session 1) | 4 of 50,974 | 71% | 143 keys |
| **20260911 (session 2)** | **2 of 36,411** | **100%** | **31,747 keys (47,545 after inserts)** |
| 20260912 (session 3) | 36,864 of 36,864 | 51% | 101 keys |

fb holds a few enormous IDs (the largest sampled key is about 10^19, while
99.9% of keys are below 8 × 10^10). They stretch the top table, so almost
every key lands in one or two top buckets. In session 2 the second level was
stretched the same way, so the ordinary keys fell into a few hundred boxes
that overflowed into lists of 30,000–47,000 keys. A search then halves its
way through a long list, and an insert shifts tens of thousands of keys. That
is the clumps-inside-clumps weakness (`results/nested/PLAN.md`), here
triggered by outliers in real data. Even in the good sessions most fb keys
sit in overflow lists, which is why ClusterJumpD uses about 25 bytes per key
there.

**Plain summary:** ClusterJumpD is fast on data without extreme outliers, and
it has a bad worst case on data with them. A fix would need the top levels to
ignore outliers (for example, by fitting the table to the bulk of the keys
rather than to the full range). That is a design change and has not been
tested.

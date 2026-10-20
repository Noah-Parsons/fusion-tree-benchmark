# Breakthrough attempt: the fusion tree's guarantee at learned-index speed

Written 2026-10-15, before any of the code below was timed.

## The target

One static predecessor structure that

1. **provably** answers any query on any set of 64-bit keys in a number of
   cache lines bounded by about log_32(n) + a small constant, with no
   dependence on how the keys are spread (the fusion tree's kind of
   guarantee), and
2. runs as fast as CHT and PGM-index on books, wiki, fb and osm.

No learned index has (1) in that form: PGM's bound is B-tree-like with larger
constants, CHT's grows with key length (about 64/r levels), and ClusterJump's
is about log2(n) reads (`include/cluster_jump.hpp`, step 3).

## Test 0: prior art (done 2026-10-15)

HOT (Binna et al., SIGMOD 2018; TODS 47(1), 2022) already has the node this
attempt planned: a compound node over a binary Patricia trie, up to 32
entries, discriminative bits pulled out with PEXT, and an AVX2 search over
"sparse partial keys" (TODS Listing 4). So **the node is not new.**

What HOT does not state (checked in the TODS text):

- no worst-case height in terms of n and key length. Its theorem is that the
  height is the minimum over all partitions with fanout at most k, which is
  relative, not a number;
- no predecessor (lower-bound) bound: its lookup is an exact-match descent
  followed by one key comparison;
- no comparison with any learned index (no mention of RMI, PGM, RadixSpline,
  CHT or SOSD).

So the attempt continues, with the claim narrowed to: **a proved,
distribution-free worst case for predecessor search in a HOT-style trie, and
a race against the learned indexes.** The node is borrowed, with credit.

## Test 1: node speed (the kill gate)

`bench/node_lat_patricia.cpp`, in the same harness as `bench/node_lat.cpp`
(64 nodes, all in L1, 2^14 queries, median of 15 reps, cycles from the ADD
calibration loop, CPU 8). Two operations, at K = 16 and 32 keys:

- **step**: one blind Patricia descent through a node (PEXT, AVX2 comply
  test, highest match). In the full trie this is what each level costs; the
  correction against a real key happens once per query, not per level.
- **rank**: a complete predecessor rank inside one node (step, then one key
  comparison, then a second search on the dense sketches). This is the
  like-for-like with the fusion nodes in `results/node_lat.csv`.

The S+-tree-style 16-key branch-free scan is re-measured in the same run.

**Gate, fixed now.** In latency mode, let

    cost per bit = cycles / log2(fanout)

with fanout K for the trie step and 17 for the 16-key B-tree node.

- **PASS** if step at K = 32 costs no more per bit than the B-tree node,
  **and** rank at K = 32 is under 2× the B-tree node's latency (the
  correction must be affordable once per query).
- **FAIL** otherwise. The attempt stops, as the fusion tree did, and the
  fallback (proved worst cases of the existing indexes) takes over.

Correctness is checked before timing: step against an explicit Patricia
descent, rank against `std::lower_bound`, on every query. Any mismatch aborts.

Load on the laptop: a few seconds of one core, under 1 MB of memory.

## Outcome (2026-10-15): FAIL. The attempt stops here.

`results/breakthrough/node_lat_patricia.csv`, CPU 8 pinned, 4.95 GHz, 15
reps. Correctness first: 2,048,000 checks (uniform, comb, consecutive and
nested-cluster key sets), 0 wrong.

| node | keys | lat cycles | tput cycles | lat cycles per bit of fanout |
|---|---|---|---|---|
| B-tree, branch-free (re-measured) | 16 | 31.2 | 5.3 | 7.6 |
| Patricia step | 16 | 30.1 | 4.9 | 7.5 |
| Patricia step | 32 | 32.8 | 7.2 | **6.6** |
| Patricia rank | 16 | 73.6 | 35.4 | |
| Patricia rank | 32 | **82.4** | 39.1 | |

- Step at K = 32: 6.6 cycles per bit against the B-tree's 7.6. **Passes.**
- Rank at K = 32: 82.4 cycles against the limit of 2 × 31.2 = 62.4.
  **Fails.**

**Why it fails.** The compiled rank has a single branch (q equal to the
landing key, almost never taken), so this is not misprediction. It is the
same dependency chain that sank the fusion tree: step, then load the real
key, then find where it differs, then a second PEXT, then a second SIMD
search. Each link waits for the one before.

**What it means for the full structure.** A query would cost about depth ×
step + one correction. At 2^25 keys, with a fanout of 32 and an exact
correction of about 50 cycles beyond the step, that is roughly 5 × 33 + 50 ≈
215 cycles, against roughly 7 × 31 ≈ 217 for a 16-key B-tree. PGM already
beats that latency on all four datasets (it beats ClusterJump, which ties the
S+ tree: `results/learned/`, `results/realdata/`); CHT's race in
`results/sosd_rivals/` has not finished. So the fusion-style guarantee does not come at
learned-index speed through this node: it costs about what a B-tree costs,
and the per-level saving (13% per bit) is eaten by the correction.

**Exploratory, not part of the gate.** Rank's throughput (39 cycles, against
7 for step) is higher than its instruction count suggests. That was not
investigated further; a faster correction would not change the latency
result above, which is the one the gate is about.

**Next:** the fallback in the plan. Construct the worst key set for each
index (CHT, PGM, RadixSpline, ClusterJump, S+ tree), prove each one's worst
case, and measure it on that key set.

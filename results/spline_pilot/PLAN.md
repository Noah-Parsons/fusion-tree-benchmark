# Spline pilot: plan, fixed before any spline timing was seen

Written 2026-09-12, after `spline_index.hpp` passed its correctness tests and
before it was timed.

## Question

Does SplineIndex beat the best S+ tree at large n, on evenly spread AND on
clustered keys, in both throughput and latency?

## Stage 1: tuning (one session, exploratory)

- Candidates: `spline8`, `spline16`, `spline32` (error bound E = 8, 16, 32).
- Opponents in the same run: `sorted_array`, `btree8_bl`, `splus8`, `splus16`,
  `radixjump`.
- 7 repetitions, 2^8 to 2^25 keys, core 8, shuffled order, g++,
  seed 20260909; `tput` and `lat`; `uniform` and `clustered` keys.

**Selection rule.** At n = 2^25, for each candidate take its ratio to the
faster of `splus8` and `splus16` in each of the four cells
(tput/lat × uniform/clustered). Choose the candidate whose WORST ratio is
lowest. Ties within 2%: choose the smaller E (less memory per window).

## Stage 2: confirmation (fresh keys)

- The chosen candidate, `splus8`, `splus16`, `btree8_bl`, `sorted_array`.
- Three sessions, seeds 20260910, 20260911 and 20260912, none of which was used
  for tuning. 15 repetitions, otherwise as Stage 1.

**Claim rule.** "Beats the S+ tree" is claimed for a cell (mode × keys) at
size n only if the chosen candidate's median is below the faster S+ tree's
median in all three sessions. Every cell is reported, including the ones where
it loses.

Results from Stage 1 are reported as exploratory and are not used as evidence
for the claim.

## Stage 1 outcome (recorded before anything else was timed)

Worst ratio to the faster S+ tree at 2^25 across the four cells: spline8
1.169, spline16 1.243, spline32 1.231. **The rule selects spline8, and spline8
beats the S+ tree in none of the four cells.** No spline candidate beats it in
throughput (1.15–1.24); in latency spline32 is at 0.857 (uniform) and 0.837
(clustered). Clustering no longer hurts: every spline's clustered times are
within a few per cent of its uniform times.

Diagnosis. At E = 32 there are 28,312 pieces (0.7 MB), so the piece tree is in
cache and memory is not the problem. The search is long (four S+ levels, a
float prediction, a 17-compare window), so fewer searches fit in the CPU's
out-of-order window while each waits on memory. RadixJump, a far shorter
search, is 3× faster than the S+ tree in throughput.

## Decision: a new design, and a revised Stage 2

- New structure `clusterjump` (cluster_jump.hpp): RadixJump's short path with
  a second, per-bucket rescaled table. Its parameters (at most 2^16 top
  buckets, about 4 keys per second-level slot) are fixed in the source before
  it is timed; nothing about it is tuned on data.
- Stage 1b: one exploratory session of `clusterjump` against `splus8` and
  `splus16`, seed 20260909, only to catch a design failure before the long run.
- **Stage 2 (revised)** tests two candidates, `clusterjump` and `spline32`,
  under the unchanged claim rule, on the unchanged fresh seeds 20260910–12.
  `spline32` is a post-hoc choice (the best latency in Stage 1, not the rule's
  pick) and is labelled as such wherever it is reported.

## Stage 1b outcome (recorded before Stage 2 started)

`clusterjump` ÷ faster S+ tree at 2^25, seed 20260909, 7 repetitions:
throughput 0.47 (uniform) and 0.46 (clustered); latency 1.02 (uniform) and
0.90 (clustered). No design change followed. Stage 2 runs as revised above,
into `results/spline_confirm/`, with `sorted_array`, `btree8_bl`, `splus8`,
`splus16`, `spline32` and `clusterjump`.

Expectation, written down in advance so it can be checked: `clusterjump` wins
throughput in both key sets; `spline32` wins latency in both; neither wins all
four cells. Latency is where they differ. `spline32` waits on one uncached
read per search (its scan window), while `clusterjump` waits on two, one after
the other (its second table, then the keys).

## Stage 2 outcome

Seeds 20260910–12, 15 repetitions, 17:12–17:18 on 12 September;
`Rscript analysis/confirm.R results/spline_confirm` writes
`results/spline_confirm_summary.csv`. At 2^25, candidate ÷ faster S+ tree,
median of three sessions [lowest, highest]:

| candidate | mode | uniform keys | clustered keys |
|---|---|---|---|
| clusterjump | throughput | **0.45** [0.44, 0.45] | **0.40** [0.40, 0.42] |
| clusterjump | latency | 1.01 [1.00, 1.01], no claim | **0.92** [0.90, 0.93] |
| spline32 (post hoc) | throughput | 1.21 [1.21, 1.23], loses | 1.21 [1.19, 1.25], loses |
| spline32 (post hoc) | latency | **0.84** [0.83, 0.85] | **0.84** [0.83, 0.86] |

Bold = below 1 in all three sessions (the claim rule). The expectation written
before Stage 2 held in every cell except one it did not predict: `clusterjump`
also wins latency on clustered keys.

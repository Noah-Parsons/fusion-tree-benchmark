# ClusterJump2: design and pilot rule (written before any code or timing)

2026-10-19. Exploratory; tuning seed 20260909 only. Confirmation seeds are not touched until a
design is frozen under `results/best/PLAN.md` section 3.

## What ClusterJump spends its latency on

A search reads, one after another: the top table (up to 2^16 entries x 24 bytes, about 1.5 MB),
two adjacent entries of its bucket's second table, and then the keys (256 MB at 2^25 keys). The
second tables together hold about one entry per 4 keys, i.e. 2^23 x 4 bytes = 32 MB, which is
larger than this machine's 30 MB L3 cache. In latency mode (each query depends on the previous
answer) that read therefore usually misses to DRAM, as does the key read. PGM-index and
RadixSpline keep their upper layers small enough to stay cached and pay mainly for the key read.

## Changes (each within section 2 of PLAN.md)

1. **Wider slots, wider last step.** The second level targets 2^K keys per slot instead of 4
   (K = 2 is the current design), shrinking the second tables to 32/2^(K-2) MB. The last step
   compares W keys at once with AVX2 (W = 8 now; W = 16 means two cache lines loaded
   independently). Longer slots are narrowed by halving until at most W keys remain, as now.
2. **Outlier-robust fitting.** For a bucket with more than 32 keys: if its largest key is more
   than 4x as far from the bucket's smallest key as its 9th-largest key is, the top 8 keys are
   left out of the fit and are reached through the end slot (the existing clamp j = slots); the
   same test is then applied to the 8 smallest keys, which are reached through slot 0. So the
   second level spans the bulk of the bucket, and at most 8 keys sit in each end slot.

Unchanged: the equal-width top level, the fixed depth (top table, second table, keys), and the
search semantics. No recursion; no equal-width sub-bins (rules 1-3 of PLAN.md section 2).

## Variants piloted

| name | K (keys per slot 2^K) | W | robust fitting |
|---|---|---|---|
| `clusterjump` | 2 | 8 | no (current) |
| `cj2_k2w8` | 2 | 8 | yes (isolates the outlier fix) |
| `cj2_k3w16` | 3 | 16 | yes |
| `cj2_k4w16` | 4 | 16 | yes |

## Pilot (fixed now)

Harness `bench_one`, seed 20260909 samples of books, wiki, fb, osm, both modes, 7 repetitions,
racers: the four variants above plus `splus8`, `rs18_e16`, `pgm16`, `cht64_e16`, `cht256_e16`,
`rmi_r7`, `rmi_r10`. Safe settings as in Stage 2.

**Choice of design to freeze.** Among the cj2 variants, the one with the lowest geometric mean
over the four datasets of (latency / fastest rival's latency), subject to: (a) its throughput on
every dataset is within 10% of `clusterjump`'s or better, except fb where it must be at most the
S+ tree's; (b) correctness tests pass. If no variant satisfies (a), none is frozen and the paper
reports the current ClusterJump.

The pilot cannot by itself establish any claim; only the confirmation race of PLAN.md can.

## Pilot outcome (2026-10-19)

**Bug found during the pilot.** The first pilot run showed checksum mismatches on osm for all three
cj2 variants. Cause: when the 8 largest keys of a bucket were left out of the fit, all 8 were sent
to the end slot, although some of them can lie inside the last regular slot's range; queries there
missed them. Fixed by assigning every key by its slot number (the end slot holds only keys beyond
the fitted range, at most 8). `tests/test_cj2.cpp` now includes this pattern and runs on six real
samples: 80,080,344 checks, 0 failures. The invalid first-pilot files are in
`results/best/invalid_pilot_endslot_bug/`.

**Corrected pilot** (`analysis/pilot_cj2.ps1`, seed 20260909, 7 reps, all checksums agree):

| variant | throughput rule | latency geomean / fastest rival |
|---|---|---|
| clusterjump | fails (fb 144 ns vs S+ 54) | 1.51 |
| cj2_k2w8 | **passes** | **1.21** |
| cj2_k3w16 | fails (slower than clusterjump on every dataset) | 1.38 |
| cj2_k4w16 | fails | 1.40 |

**Frozen design: `cj2_k2w8`** (robust fitting only; slots and last step as in ClusterJump).
Robust fitting fixed fb (throughput 144 -> 47 ns, latency 379 -> 177 ns) and left the other
datasets essentially unchanged. Wider slots made both modes slower, so the hypothesis that the
second-level cache misses dominate latency is not supported; the remaining latency gap (1.08-1.39x
the fastest rival on every dataset) lies in the key accesses. A full "Best" claim is therefore not
expected for this design; the confirmation race would test "Best in throughput" for the robust
version.

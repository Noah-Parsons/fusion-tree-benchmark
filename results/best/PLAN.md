# What "best" means, and how ClusterJump must differ from the Hist-Tree

Written 2026-10-19, before Stage 1 of `results/sosd_rivals/PLAN.md` was finished (osm still to
run) and before any new ClusterJump design existed. Nothing below may be changed after the
confirmation data are seen; any later change is added as a dated amendment that says so.

## 1. The claim "best available" and its rule

**Racers.** ClusterJump (the version that exists when the confirmation runs start) against:
`splus8`, `splus16` (S+ tree), `rs18_e16` (RadixSpline), `pgm16` (PGM-index), the CHT setting
and the RMI configuration chosen by `analysis/sosd_select.R` in Stage 1 (one per mode), exactly
as `results/sosd_rivals/PLAN.md` specifies.

**Data.** At n = 2^25 keys: the four SOSD datasets (books, wiki, fb, osm), plus the synthetic
`nested` key set (64 clusters x 64 sub-clusters, `results/nested/`). Confirmation seeds
20260910, 20260911, 20260912; 15 repetitions; both modes (throughput, latency); the harness of
Stage 2 (`bench_one`, one structure in memory at a time, 8 GB cap).

**Cell rule (unchanged from earlier races).** In each cell (dataset x mode), ClusterJump
*beats* a rival if its median time is lower in all three sessions, *loses* if higher in all
three, otherwise *ties*.

**Graded claims.** Exactly one of these is reported, the highest that holds:

| Claim | Condition |
|---|---|
| **Best** | beats every rival in both modes on all four SOSD datasets, and on `nested` is no slower than 1.25x the fastest rival in either mode |
| **Best in throughput** | beats every rival in throughput on all four SOSD datasets (latency reported as is) |
| **Best in latency** | the same for latency |
| **Not best** | none of the above; every cell is reported anyway |

**Memory.** Bytes per key are reported for every racer. If ClusterJump uses more than twice the
median rival's memory on any dataset, the claim is stated as "best at higher memory".

**What would make the claim false.** Any SOSD cell where a rival is faster in all three sessions,
or a `nested` time above 1.25x the fastest rival.

## 2. "Different from the Hist-Tree"

The Hist-Tree (Crotty, CIDR 2021) splits each node's key range into equal-width bins and
recurses into any bin holding more than a threshold of keys, to whatever depth is needed; CHT is
its flattened read-only form. Any ClusterJump design raced under section 1 must satisfy all of:

1. **Fixed depth.** Every search reads at most two tables before the final search, whatever
   the key distribution. No recursion into child nodes.
2. **Fitted second level.** Each top bucket's table spans that bucket's own smallest-to-largest
   key, not the fixed range of its parent bin.
3. **Crowded slots are not split into equal-width sub-bins.** A slot holding too many keys must
   be handled by some other mechanism (for example equal-count (quantile) boundaries, or a
   bounded sorted run searched with SIMD). Whatever mechanism is chosen is described before it
   is raced.

If a design meets section 1 only by breaking a rule in section 2, the paper says so plainly
instead of claiming a new design.

## 3. Order of work

1. Finish Stage 1 (osm), run `sosd_select.R`, then Stage 2 with the *current* ClusterJump. This
   is the honest baseline and answers the reviewer.
2. Only then design changes (within section 2), each tested in an exploratory pilot on the
   tuning seed 20260909, never on the confirmation seeds.
3. Race the final design once under section 1.

## 4. Safety (non-negotiable)

WSL shut down before any timed run; one structure in memory at a time; 8 GB cap per process;
watchdog stops everything below 6 GB free RAM; one campaign at a time; the user is asked before
each campaign that runs longer than about 30 minutes.

## Outcome for the current ClusterJump (2026-10-19)

Stage 2 ran in the safe harness (`analysis/sosd_stage2_safe.ps1`): 24 runs, peak 2.5 GB, lowest
free RAM 13.9 GB, no watchdog stop. Racers as in section 1 (CHT `cht256_e16`, `cht64_e16`; RMI
`rmi_r10`, `rmi_r7`, chosen by `sosd_select.R`).

**Amendment (bug found and fixed before the claim was applied).** On wiki, the samples for seeds
20260910 and 20260912 share key count, first and last key, which was the whole RMI fingerprint,
so session 3 used seed 10's RMIs on seed 12's keys (RMI times about 30x too slow). The fingerprint
now also includes the keys at n/4 and n/2, `rmi_registry.py` refuses to write a registry with a
collision, and the two wiki session-3 runs were rerun; the invalid files are kept in
`results/sosd_rivals/invalid_wiki_s3_fingerprint/`. No other dataset had a collision.

**Claim reached: "Best in throughput".** ClusterJump beats every rival in throughput in all three
sessions on all four datasets (32 of 32 cells; ratios 0.28-0.86). Latency (W = beats, L = loses,
T = tie, rivals in the order S+8, S+16, RS, PGM, CHT256, CHT64, RMI10, RMI7):

| dataset | latency |
|---|---|
| books | T T L L T L W T |
| wiki | T W L L W W W T |
| fb | T W W T W W W W |
| osm | L T T T W W W W |

So not "Best": latency loses to RadixSpline and PGM-index on books and wiki, to the S+ tree on
osm, and to CHT64 on books.

**Memory** (`bench/index_bytes_race.cpp`, seed 20260910, keys included): ClusterJump 8.3-8.8
bytes per key, among the smallest of the nine (rivals 8.0-25.7). No memory qualifier.

**Caveat that stays with the claim.** On the tuning seed 20260909, one fb outlier key falls into
the bulk's top bucket and ClusterJump's throughput on fb is 2.9x the S+ tree's
(`results/sosd_rivals/PLAN.md`). The confirmation seeds do not contain such a key. The claim is
reported together with this sensitivity.

## Outcome for the final design `cj2_k2w8` (2026-10-19)

Design frozen by the pilot rule of `results/best/DESIGN_cj2.md` (ClusterJump with outlier-robust
second levels; slots and last step unchanged). Confirmation race `analysis/confirm_cj2.ps1`:
seeds 20260910-12, 15 reps, safe harness, 24 runs, peak 2.5 GB, lowest free RAM 14 GB, every
checksum identical. Old ClusterJump raced alongside for reference (not a rival).

**Claim reached: "Best in throughput".** `cj2_k2w8` beats every rival in throughput in all three
sessions on all four datasets (32 of 32 cells; ratios 0.29-0.83). On these seeds its speed equals
old ClusterJump's (0.92-1.04x), as expected: none of the confirmation samples has an outlier in
the bulk's top bucket. On the tuning seed, where one does, the pilot showed fb throughput
144 -> 47 ns, faster than every rival.

Latency (W = beats, L = loses, T = tie; rivals S+8, S+16, RS, PGM, CHT256, CHT64, RMI10, RMI7):

| dataset | latency |
|---|---|
| books | T W L T W T W W |
| wiki | W W L L W T W T |
| fb | L W W T W W W W |
| osm | L W L T W W W W |

Not "Best": latency loses to RadixSpline on books, wiki and osm, to PGM-index on wiki, and to the
S+ tree on fb and osm.

**Memory** (`results/best/index_bytes_cj2.csv`): 8.32-8.81 bytes per key, identical to old
ClusterJump, among the smallest of the racers. No memory qualifier.

**Statement for the paper.** Among the structures raced (S+ tree, RadixSpline, PGM-index, CHT,
RMI), the outlier-robust ClusterJump has the highest lookup throughput on all four SOSD datasets
at 2^25 keys, at the lowest memory, and no longer degrades when a single outlier key shares the
bulk's top bucket; it is not the fastest in single-lookup latency.

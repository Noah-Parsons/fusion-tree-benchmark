# Every experiment: the question, the answer, and where it lives

The project is a chain of questions, each one prompted by the last. They are
listed in the order they were asked.

**How to read the numbers.** A ratio is one method's time divided by
another's. **Below 1 means the first method is faster.** 0.5 means twice as
fast; 2.0 means twice as slow.

**Rules every experiment follows:**
1. All answers are checked for correctness first.
2. Every race checks that all methods gave identical answers (checksums).
3. From Part 2 on, the rules for deciding the winner were **written down
   before the race was timed** (the `PLAN.md` files).
4. A win counts only if it happens in **all three** separate sessions.

All results come from **one laptop** (Intel Core Ultra 7 255HX; details in
`results/machine.txt`). Another computer could give different numbers.

---

## Part 1: does the fusion tree beat a B-tree in real life?

The original project (STEM Lab 2026–27). Full write-up:
`Experiments_Report_2026-09-10.docx`.

### Experiment 1: can the 1993 design even be built?
**Question:** does the fusion tree's original "multiplication sketch" fit in a
64-bit number?
**Answer:** only for 4 numbers per box, not 8. The versions raced here work
only because of the PEXT instruction (2013).
**Files:** `results/spread.csv` · **Program:** `make spread`

### Experiment 2: the timing race
**Question:** at any list size from 256 to 33 million, is the fusion tree
faster?
**Answer:** it beats binary search on big lists, but it **never beats the
branch-free B-tree**, and the gap grows with size.

| at 33 million numbers | throughput (ns) | latency (ns) |
|---|---|---|
| branch-free B-tree | **186** | 475 |
| fusion tree | 579 | 577 |
| binary search | 723 | 1407 |

**Files:** `results/final_v3/`, `results/final_summary_v3.csv`,
`results/final_ratios_v3.csv`, `figures/final_*_v3.png`
**Program:** `analysis/final_campaign.ps1`, then `analysis/final.R`

### Experiment 3: why does it lose?
**Answer:** each box search takes about twice as many steps (60 processor
cycles against 28). Hardware counters showed 2.2× more instructions per
search. Memory is only a small part of the gap: at equal box size, it is
still 2.8× slower.
**Files:** `results/node_lat.csv`, `results/traffic.csv`,
`results/vtune_counters.csv`
**Programs:** `make node-lat`, `make traffic`

### Experiment 4: does a wider (256-bit) number help?
**Answer:** no. The 16-key fusion box is no faster.

---

## Part 2: can anything beat the S+ tree?

### 2a. First try: RadixJump and SplineIndex
**Answer:**
- **RadixJump:** 3× faster on evenly spread numbers, but collapses on clumps.
- **SplineIndex:** handles clumps, wins latency by 16%, but loses throughput.

**Files:** `results/splus_pilot/`, `results/spline_pilot/PLAN.md`

### 2b. ClusterJump, confirmed on fresh data
**Answer:** 2.2–2.5× faster than the S+ tree in throughput (even and clumped
numbers). In latency it wins on clumps (8%) and ties on even numbers.
**Files:** `results/spline_confirm/`, `results/spline_confirm_summary.csv`,
plan in `results/spline_pilot/PLAN.md`

---

## Part 3: does it hold on real data?

**Question:** does ClusterJump still beat the S+ tree on the four real SOSD
datasets?
**Answer:** yes, 1.6–2.3× faster in throughput, and equal or slightly faster
in latency.
**Files:** `results/realdata/PLAN.md`, `results/realdata_summary.csv`
**Needs:** the SOSD datasets in `data/` (see [PROGRAMS.md](PROGRAMS.md))

---

## Part 4: against published learned indexes

**Question:** RadixSpline and PGM-index were designed for this data. Does
ClusterJump beat them?
**Answer:** it splits.
- **Throughput:** ClusterJump wins on all four datasets (1.3–2.6× faster).
- **Latency:** the learned indexes win (6–31% faster), except RadixSpline on
  Facebook IDs.
- **Memory:** about equal (8–9 bytes per number).

**Files:** `results/learned/PLAN.md`, `results/learned_summary.csv`

---

## Part 5: clumps inside clumps

**Question:** what happens when the clumps are made of smaller clumps?
**Answer:** ClusterJump **loses to everything**, 1.6–2.2× slower. This is its
real weakness, and it was predicted before running.
**Files:** `results/nested/PLAN.md`, `results/nested_summary.csv`

---

## Part 6: data that changes

**Question:** can a version of ClusterJump that accepts new numbers
(ClusterJumpD) compete with ALEX and the TLX B+ tree?
**Answer:** faster than both on five of six datasets, with 10% or 50% of
operations adding numbers. On Facebook IDs it has a bad worst case (up to 12×
slower in one session) because of a few extreme values. It uses the most
memory. The tests also found three bugs in ALEX.
**Files:** `results/dynamic/PLAN.md`, `results/dynamic_summary.csv`,
`third_party/PATCHES.md`

---

## Part 7: is ClusterJump new?

**Answer: no.** It is essentially a two-level version of the **Hist-Tree**
(Crotty, CIDR 2021). The contribution of this project is **careful
measurement**, not a new invention.
**File:** `results/RELATED_WORK.md`

---

## Part 8: against Hist-Tree (CHT) and RMI — IN PROGRESS

**Question:** the two most important missing rivals. How does ClusterJump
compare with them?
**Status:** not finished. The RMIs are generated and checked (80 million
correct answers), and CHT passes its tests. Two timing attempts were stopped
because the old benchmark program used too much memory; the new
`bench_one` program (one method in memory at a time, with a hard 8 GB memory
cap) is being used instead.
**Files:** `results/sosd_rivals/PLAN.md` (includes the full story of the
stopped attempts)

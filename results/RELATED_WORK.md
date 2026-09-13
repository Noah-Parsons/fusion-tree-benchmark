# Related work: is ClusterJump new?

Checked 2026-09-12, after all ClusterJump and ClusterJumpD results were in.
Short answer: **no.** Its core idea was published in 2021. What this
repository adds is careful measurement, not a new structure.

## The closest published design: Hist-Tree (2021)

Andrew Crotty, "Hist-Tree: Those Who Ignore It Are Doomed to Learn", CIDR 2021.
https://www.cidrdb.org/cidr2021/papers/cidr2021_paper20.pdf

- Every node is a histogram that splits its key range into equal-width
  (power-of-two) bins, so a bin is found with a shift, as in ClusterJump.
- A bin holding more keys than a threshold gets its own child node, and
  this **repeats as deeply as needed**. So dense regions inside dense regions
  get their own finer levels.
- The Compact Hist-Tree (CHT) flattens the tree into one lookup table for
  read-only data. Evaluated on the same SOSD datasets used here (books, fb,
  osm, wiki). The paper reports CHT lookups 1.1–1.8× faster than RMI,
  1.7–2.5× faster than PGM-index, and 1.2–2.7× faster than RadixSpline.
- Code: https://github.com/stoianmihail/CHT (used in PLEX, below).

**How ClusterJump compares.** ClusterJump is essentially a Hist-Tree cut off
at two levels, with the second level fitted to each bucket's actual smallest
and largest key and an AVX2 final step. Those are small variations. The
weakness measured here (`results/nested/PLAN.md`, and fb in
`results/dynamic/PLAN.md`) is exactly what Hist-Tree's unlimited depth
avoids. **ClusterJump has not been raced against CHT.** Given the numbers
above, CHT would probably match or beat it.

## Other close relatives

- **RadixSpline** (Kipf et al., aiDM 2020): a radix table on the top bits,
  pointing into a spline. Raced here.
- **PLEX** (Stoian, Kipf, Marcus, Kraska, AIDB 2021): a spline whose radix
  layer is replaced by a CHT. https://arxiv.org/abs/2108.05117

## Structures for changing data

- **ALEX** (Ding et al., SIGMOD 2020): gapped arrays under linear models.
  Raced here.
- **LIPP** (Wu et al., PVLDB 14(8), 2021): every key at a precisely predicted
  position, with conflicts moved into child nodes. The authors report up to
  4× over earlier learned indexes. **Not raced here.**
  http://www.vldb.org/pvldb/vol14/p1276-wu.pdf
- **BLI** (2025): a bucket-based learned index with fixed-size, locally
  unsorted buckets; reported up to 2.21× faster than ALEX and LIPP.
  **Not raced here.** ClusterJumpD's fixed-size boxes per slot are a similar
  bucket idea. https://arxiv.org/abs/2502.10597
- **DILI**, **DyTIS**, **SALI**, **FINEdex**: further updatable learned
  indexes, none raced here.

## Robustness of updatable learned indexes

- Luo, Xie, Tong, Jiang and Chai, "Understanding Robustness Issues of Updatable
  Learned Indexes", Proc. ACM Manag. Data 3(4) (SIGMOD), 2025. They find that
  none of the state-of-the-art updatable learned indexes robustly outperform
  a B+ tree or ART. Performance swings widely between workloads, and ALEX and
  LIPP can use up to 10× the indexed data's size in the worst case.
  https://doi.org/10.1145/3749188
- "Algorithmic Complexity Attacks on Dynamic Learned Indexes",
  https://arxiv.org/abs/2403.12433, and "Poisoning Learned Index Structures:
  Static and Dynamic Adversarial Attacks on ALEX",
  https://arxiv.org/abs/2604.24975: adversarial inputs can blow up ALEX's
  time and memory.

## The ALEX problems found here: known or not?

Checked against ALEX's GitHub issue tracker (21 issues) and a web search:

| problem found by tests/test_dynamic.cpp | already known? |
|---|---|
| crash (`bad_array_new_length`) on tight, far-apart clumps from empty | probably related: open issue #30 "Insertion uses all memory", and the adversarial-input papers above |
| wrong answers for closely spaced keys above 2^53 from empty | related limit known: the alex-go port notes that ALEX's regression can overflow or lose precision on large uint64 keys |
| wrong answers for queries outside the key range (`int bucketID = model_.predict(key)` overflows) | **no report found**. This is not proof that nobody has seen it. |

## What would be needed to claim anything new

1. Race ClusterJump against CHT, and ClusterJumpD against LIPP and BLI.
2. Then either show a clear, repeatable advantage, with a reason for it,
   or report honestly that the published designs are as fast or faster.
3. Run on a second machine.

Until then, the accurate description is: an independent reimplementation
and careful measurement of a Hist-Tree-like design, with its failure cases
identified.

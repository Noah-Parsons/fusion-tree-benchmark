# The search methods, explained simply

Every method answers the same question: **given q, what is the biggest stored
number smaller than q?** They differ only in *how* they find it.

The name in `code font` is what you type to race it, and what appears in the
results files.

---

## Part 1: the fusion tree study

### `sorted_array`: binary search
**Idea:** keep the numbers in a sorted list. Look at the middle, throw away
half, repeat.
**Good:** tiny and simple. **Bad:** every look jumps somewhere new in memory,
so big lists mean many slow trips to RAM.
File: `include/structures.hpp`

### `btree8`: B-tree, 8 keys per box, "stop early"
**Idea:** group numbers into boxes of 8. Check a box's numbers left to right
and stop at the first one that is too big; that tells you which box to open
next.
**Good:** about 8 memory trips instead of 25. **Bad:** the processor keeps
guessing wrong about where it will stop.
File: `include/structures.hpp`

### `btree8_bl`: B-tree, "branch-free"
**Idea:** the same boxes, but compare **all 8 numbers at once** with AVX2 and
count how many are smaller. Nothing to guess.
**This is the strongest rival in the fusion tree study.**
Variants: `btree8_a64` and `btree8_bl_a64` (boxes lined up with memory
chunks), and `btree16_bl_a64` (16 keys per box).
File: `include/structures.hpp`

### `fusion8`: the fusion tree
**Idea:** the same box shape as the B-tree, but instead of comparing numbers,
it squeezes each number down to a few important bits (a "sketch") and compares
all the sketches in one subtraction. Then a repair step fixes cases where the
sketch alone gives the wrong box.
**Result:** clever, but the repair step makes each box search take about
twice as many steps as the B-tree's.
Variants:
- `fusion8_bf`: the same, without branches.
- `fusion8_c`: a compact box, the same size as a B-tree box.
- `fusion16_w`: 16 keys per box, on a 256-bit AVX2 word.

Files: `include/fusion_node*.hpp`, `include/structures.hpp`

---

## Part 2: beating the S+ tree

### `splus8`, `splus16`: the S+ tree
**Idea:** a B-tree with no pointers at all (a box's children are found by
arithmetic), all numbers in the bottom row, and AVX2 comparisons. The fastest
known tree for data that never changes.
File: `include/splus_tree.hpp`

### `radixjump`: RadixJump
**Idea:** use the first bits of q as a position in a table, like opening a
dictionary straight to the right letter. Then check the few numbers there.
**Good:** very fast on evenly spread numbers. **Bad:** falls apart on clumped
numbers (most of the table is empty and a few spots hold everything).
File: `include/radix_jump.hpp`

### `spline8`, `spline16`, `spline32`: SplineIndex
**Idea:** draw straight lines through the numbers (like a connect-the-dots
graph of number versus position), guaranteed never to be more than E places
off. Find the right line, guess the position, check the nearby numbers.
**Good:** handles any shape. **Bad:** the search has many steps, so it loses
when many searches run at once.
File: `include/spline_index.hpp`

### `clusterjump`: ClusterJump (built in this project)
**Idea:** RadixJump, plus a second, zoomed-in table for each region. A crowded
region gets its own fine-grained table, and an empty region costs nothing.
**Good:** fastest measured here when many searches run at once, on made-up
and real data. **Bad:** breaks on clumps inside clumps, and on data with a few
extreme values (like Facebook user IDs).
**Not new:** it is essentially a two-level version of the published Hist-Tree;
see `results/RELATED_WORK.md`.
File: `include/cluster_jump.hpp`

---

## Part 3: published rivals (third-party code)

These use other researchers' code from `third_party/`, wrapped so they race
under the same rules. Every wrapper finishes with the **same final search
step** as ClusterJump, so no method wins or loses just because of its last
step.

### `rs18_e16` (and other `rs*`): RadixSpline
A table on the first bits points into a line-fitted model. Settings: `18` is
the table size in bits, and `e16` means at most 16 places off.
Wrapper: `include/learned_indexes.hpp`

### `pgm16` (and other `pgm*`): PGM-index
Straight-line pieces, indexed by smaller sets of straight-line pieces. `16` is
the maximum error.
Wrapper: `include/learned_indexes.hpp`

### `cht64_e16` … `cht1024_e128`: Compact Hist-Tree (CHT)
Splits the number range into equal slices (64, 256 or 1024 of them), and splits
any slice holding more than E numbers again, as deep as needed. **Warning:**
the 1024-slice settings with a small E can need many GB to build.
Wrapper: `include/sosd_rivals.hpp`

### `rmi_r1` … `rmi_r10`: RMI
A small model picks a larger model, which guesses the position. Its code is
generated for each specific key set by the RMI tool. `r1` is that dataset's
smallest configuration and `r10` its largest.
Wrapper: `include/rmi_index.hpp`

---

## Part 4: structures that accept new numbers

### `clusterjump_d`: ClusterJumpD (built in this project)
ClusterJump where every slot of the second table is a small box of 16
numbers. New numbers go into their box; a full box spills into an overflow
list; a region is rebuilt when too much overflows. Cannot delete numbers.
File: `include/cluster_jump_dynamic.hpp`

### `tlx_btree`: TLX B+ tree
A widely used, general-purpose B+ tree.
Wrapper: `include/dynamic_rivals.hpp`

### `alex`: ALEX
A learned index for changing data. The tests found three bugs in ALEX itself
(see `third_party/PATCHES.md`).
Wrapper: `include/dynamic_rivals.hpp`

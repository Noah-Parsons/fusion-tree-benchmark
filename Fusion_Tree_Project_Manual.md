# The Fusion Tree Project — Complete Manual

**Galactic Algorithms, Measured: does the fusion tree ever beat binary search?**

Noah Parsons · STEM Lab 2026–27 · target completion 4 December 2026

---

## How to use this manual

This document is meant to be worked through in order, once, with a terminal open beside it. It contains every piece of mathematics, every line of code, every command, and every decision you need. Where it says "run this", run it. Where it says "you should see", check that you see it.

Three conventions:

- **Boxes marked CHECKPOINT** are places to stop and confirm something works before continuing. If a checkpoint fails, do not proceed — the failure will be much harder to find three steps later.
- **Boxes marked WHY** explain the reason behind a decision. Skipping them will not stop the code working, but you will not be able to defend the project without them.
- Code is given complete. Nothing is elided.

The companion code tree accompanies this manual. Everything in it has been compiled and run; the correctness tests pass 5.3 million checks with zero failures, and the timing figures quoted in Part X are real measurements, not illustrations.

> **Updated 10 September 2026.** The code tree has grown since this manual was written: three more fusion node types (branch-free, compact, 256-bit), four more B-tree variants, and tools for Experiments 3 and 4. The tests now run 8,483,288 node checks and 12,000,000 structure checks, all passing under both g++ and clang. Corrections to this manual are marked **[Correction]** where they occur. The work logs in the repository record what changed and why.

---

## Part 0 — What you are doing, in one page

Computer science has a stock of algorithms that are provably faster than the ones everybody actually uses, and that nobody uses. They are called **galactic algorithms**: the input would have to be galactically large before the advantage appeared. They get taught, cited, and never run.

The **fusion tree** (Fredman and Willard, 1993) is the canonical example. It stores *n* integers and answers *predecessor* queries — "what is the largest stored key below *q*?" — in O(log_w *n*) time on a machine with *w*-bit words. Binary search takes O(log *n*). Since log_w *n* = log *n* / log *w*, the fusion tree is asymptotically faster by a factor of log *w*, which for a 64-bit machine is a factor of six. It formally breaks the comparison-based lower bound, which is a genuinely startling thing for a data structure to do.

Nobody uses it. The universal assumption is that the constant factors are ruinous. That assumption is almost never accompanied by a measurement.

**Your research question:** *Does a correctly implemented fusion tree ever beat a cache-optimised B-tree or plain binary search on real hardware, and if not, exactly which mechanism is responsible?*

**What you will produce:**

1. A correct, tested, public implementation of a fusion node and a fusion tree.
2. A measurement campaign comparing it against two baselines across input sizes spanning five orders of magnitude, instrumented with hardware performance counters.
3. A quantitative answer: either a crossover point, or a bound on where one could be, with the mechanism identified.
4. A short paper and a released repository.

**Both outcomes are results.** If the fusion tree loses everywhere, you have measured and explained something the field has only ever assumed. That is publishable and it is the more likely finding. What would not be a result is a vague "it was slower" — which is why so much of this manual is about measuring properly.

---

## Part I — The mathematics

You need five ideas. None of them requires calculus. All of them are about bits.

### I.1 Asymptotic notation, said precisely

*f*(*n*) = O(*g*(*n*)) means: there exist constants *c* > 0 and *n*₀ such that for all *n* ≥ *n*₀, *f*(*n*) ≤ *c*·*g*(*n*).

Two things follow that matter enormously for this project.

First, **the constant *c* is not specified and may be astronomical.** An algorithm that is O(*n*) with *c* = 10¹² is slower than one that is O(*n*²) with *c* = 1 for every *n* below 10¹². The notation is silent about this by design.

Second, **the threshold *n*₀ is not specified either.** The guarantee only begins to hold somewhere, and "somewhere" might be past the size of any input that can physically exist.

A galactic algorithm is one where *c*, *n*₀, or both are large enough that the asymptotic guarantee never engages in practice. Your project is an attempt to pin down *c* and *n*₀ for one specific structure, by measurement rather than by assertion.

> **WHY this framing matters for your write-up.** The interesting claim is never "the fusion tree is slow". It is "the fusion tree's constant factor is *X*, its crossover point is *n*₀ ≈ *Y*, and *Y* exceeds addressable memory by a factor of *Z*". That is a quantitative statement. The first is an opinion.

### I.2 The word RAM model

Every complexity bound is stated relative to a model of computation. The fusion tree lives in the **word RAM**:

- Memory is an array of **words**, each *w* bits wide.
- Each stored key fits in one word, so *w* ≥ log₂(universe size).
- These operations on whole words cost **one unit of time each**: addition, subtraction, multiplication, bitwise AND, OR, XOR, NOT, left and right shift, and comparison.

That last point is the whole trick. In the comparison model, you may only ask "is *a* < *b*?", and information theory then forces you to spend Ω(log *n*) comparisons to locate a key among *n*. In the word RAM you may do arithmetic, and arithmetic on a *w*-bit word can manipulate many small values *at once*. This is called **word-level parallelism**, and it is how the fusion tree escapes a lower bound that is otherwise inescapable.

The model is not a cheat. Real CPUs really do multiply 64-bit numbers in one instruction. The question this project asks is whether "one instruction" and "one unit of time" are close enough to the same thing.

> **WHY the model is also where the trouble starts.** The word RAM charges nothing for memory access. Real machines charge enormously and unevenly for it: roughly 1 nanosecond for L1 cache, 80 for main memory. An algorithm optimised for a model that ignores this can be beaten by one designed around it. Hold that thought; it returns in Part II.

### I.3 Binary tries, and where the structure comes from

Write each key in binary and read it as a path: 0 means go left, 1 means go right. All the keys together form a **binary trie** of depth *w*.

Consider three 4-bit keys: 4 = `0100`, 5 = `0101`, 12 = `1100`.

```
                    (root, bit 3)
                   0/          \1
             (bit 2)            12 = 1100
            0/                  
       (bit 1)                  
          1\                    
        (bit 0)   <- BRANCHING NODE
        0/    \1
      4=0100  5=0101
```

Notice that most nodes on the way down have only one child. Only two positions in this trie actually *decide* anything: bit 3 (where 12 splits from the others) and bit 0 (where 4 splits from 5).

**Definition — important bits.** The *important bits* of a key set are the bit positions at which branching occurs in the trie: equivalently, for every adjacent pair of sorted keys, the position of the highest bit at which they differ.

**Lemma.** A set of *n* keys has at most *n* − 1 important bits.

*Proof.* A binary trie with *n* leaves has at most *n* − 1 branching nodes, since each branching node splits the leaf set into two non-empty parts and this can happen at most *n* − 1 times. ∎

For the example: important bits are {0, 3}, and *n* − 1 = 2. ✓

This is the first real economy. Keys are 64 bits wide, but at most 7 bit positions matter when there are 8 of them.

### I.4 Sketches

**Definition — sketch.** For a key *x* and important bit positions *b*₀ < *b*₁ < … < *b*ᵣ₋₁, the sketch of *x* is the *r*-bit value formed by taking the bits of *x* at those positions and packing them down, lowest position into lowest output bit:

    sketch(x) = (bit b₀ of x) · 2⁰ + (bit b₁ of x) · 2¹ + … + (bit bᵣ₋₁ of x) · 2^(r−1)

For the example, with important bits {0, 3}:

| key | binary | bit 0 | bit 3 | sketch |
|---|---|---|---|---|
| 4 | `0100` | 0 | 0 | `00` = 0 |
| 5 | `0101` | 1 | 0 | `01` = 1 |
| 12 | `1100` | 0 | 1 | `10` = 2 |

**Theorem (order preservation).** On the stored key set, sketch is strictly increasing: if *x* < *y* are both stored, then sketch(*x*) < sketch(*y*).

*Proof.* Let *h* be the highest bit at which *x* and *y* differ. Because *x* and *y* are both stored and differ, *h* is a branching position, so *h* is an important bit — say it is *b*ⱼ. Above *h* the two keys agree, so their sketches agree at every important position above *b*ⱼ. At *b*ⱼ, *x* has 0 and *y* has 1 (since *x* < *y*). In the sketch, position *b*ⱼ maps to output bit *j*, which outranks every output bit below it. Therefore sketch(*x*) < sketch(*y*). ∎

So the sketches of 64-bit keys, which are at most 7 bits each, sort in exactly the same order as the keys themselves. That is the second economy, and it is what makes the next step possible.

### I.5 The parallel comparison

Now the trick that gives the structure its name. We want the **rank** of a query sketch among the stored sketches — how many are ≤ it — without looping over them.

Pack the sketches into one word, each occupying a **field** of *f* = *r* + 1 bits. The extra bit is a **sentinel**, left at 0:

```
 field 2      field 1      field 0
[0|sketch₂] [0|sketch₁] [0|sketch₀]
 ^sentinel
```

Build three constants once, at construction time:

- `packed` — the sketches in their fields, sentinels zero.
- `broadcast` — a 1 in the low bit of every field. Multiplying a small value by this replicates it into every field at once.
- `sentinels` — a 1 at every sentinel position.

To find the rank of a query sketch *s*:

1. Set *s*'s sentinel: *s* + 2ʳ.
2. Multiply by `broadcast`. Now every field contains *s* + 2ʳ.
3. Subtract `packed`. Field *i* now holds 2ʳ + *s* − sketchᵢ.
4. AND with `sentinels`, then count the bits.

**Why step 3 is safe.** Every sketch is strictly less than 2ʳ, so 2ʳ + *s* − sketchᵢ is between 0 and 2^(r+1) − 1: it fits in its field, and no field can borrow from its neighbour. The fields stay independent even though one subtraction was performed across the whole word.

**Why the sentinel bit answers the question.** The value 2ʳ + *s* − sketchᵢ is at least 2ʳ — that is, its sentinel bit survives — exactly when *s* ≥ sketchᵢ. So the count of surviving sentinel bits is the number of stored sketches ≤ *s*.

**Worked example.** Keys 4, 5, 12; important bits {0, 3}; *r* = 2; field width *f* = 3.

```
packed    = 0·2⁰ + 1·2³ + 2·2⁶ = 136 = 0b010 001 000
broadcast = 2⁰ + 2³ + 2⁶       =  73 = 0b001 001 001
sentinels = 2² + 2⁵ + 2⁸       = 292 = 0b100 100 100
```

Query *q* = 6 = `0110`. Its sketch is (bit 0 = 0, bit 3 = 0) = 0.

```
query = (0 + 2²) × 73          = 292 = 0b100 100 100
diff  = 292 − 136              = 156 = 0b010 011 100
diff & sentinels               =   4 = 0b000 000 100
popcount                       =   1
```

One stored sketch is ≤ 0 — namely sketch(4) = 0. Correct.

**Cost:** one addition, one multiplication, one subtraction, one AND, one POPCNT. Five instructions, regardless of whether there are 2 keys in the node or 8. *That* is word-level parallelism, and it is the entire point of the structure.

### I.6 Desketchifying — the step everyone gets wrong first

Sketching preserves order **on the stored keys**. It says nothing about an arbitrary query, and a query is exactly what you have.

Concretely: suppose 8 = `1000` is stored and bit 0 is not an important bit. Then sketch(7) = sketch(8), because sketches only look at important bits and 7 and 8 agree at all of them. The sketch comparison cannot tell you that 7 < 8. Ranking the query's sketch directly gives wrong answers, and this is the single most common source of bugs in fusion tree implementations.

The repair, following Fredman and Willard:

1. Rank sketch(*q*) to get a provisional position *i*.
2. The stored key sharing the **longest common prefix** with *q* is either keys[*i*−1] or keys[*i*]. Call the winner *y*. (This is the only place the sketch result is used, and it is used only as a pointer to a candidate, not as an answer.)
3. Let *h* be the highest bit at which *q* and *y* differ.
4. Every stored key that agrees with *q* above bit *h* must carry the **opposite** bit at *h* — if one carried the same bit, it would share a longer prefix with *q* than *y* does, contradicting *y*'s maximality. Those keys form one contiguous block: the **sibling subtree**.
5. Replace *q* with *e*, the far edge of that sibling subtree:
   - if *q* has a 1 at bit *h*, the whole block lies below *q*; take its maximum, *e* = *q* with bit *h* cleared and every bit below set, then **rank(*q*) = #{keys ≤ *e*}**;
   - if *q* has a 0 at bit *h*, the whole block lies above *q*; take its minimum, *e* = *q* with bit *h* set and every bit below cleared, then **rank(*q*) = #{keys < *e*}**.
6. Rank sketch(*e*) — this time faithfully.

**Why *e* is safe to sketch when *q* was not.** Take any stored key *x*.

- If *x* is inside the sibling block, then *x* and *e* agree above *h*, and below *h* the value *e* is all-ones (or all-zeros) and therefore dominates (or is dominated by) *x* bitwise at every important position. The sketch comparison lands on the correct side.
- If *x* is outside the block, then *x* first differs from *q* — and hence from *e* — at some bit *p* above *h*. But *y* is a stored key that agrees with *q* above *h*, so *p* is also the branching bit between the two stored keys *x* and *y*. Branching bits are important bits. So the sketch sees *p*, and the comparison is correct.

Both cases are covered, so the second rank is exact. ∎

> **CHECKPOINT — the bug that will find you.** The strict/non-strict distinction in step 5 is not decorative. Using ≤ where < is required, or the reverse, produces a structure that is right about 91% of the time — right often enough to pass a casual test and wrong often enough to invalidate every number you collect. The differential test in Part V exists precisely to catch this. When it was written for the reference implementation, it caught exactly this error twice: once with the branches inverted (67% of queries wrong) and once with *e* built from the wrong side of the trie (8.5% wrong).

### I.7 Where the branching factor limit comes from

Everything above holds for any node size *K*. Now count bits.

- Each sketch needs *r* ≤ *K* − 1 bits.
- Each field needs *f* = *r* + 1 ≤ *K* bits, allowing for the sentinel.
- All *K* fields must fit in one word: *K* · *f* ≤ *w*, so **K² ≤ w**.

For *w* = 64, this gives *K* ≤ 8.

The classical construction is more demanding still, because Fredman and Willard also require the sketch to be *computed* in O(1). Their method multiplies by a carefully chosen constant that shifts all *r* important bits into a contiguous window; the window is proved to be at most *r*⁴ bits wide, which forces *K* ≈ *w*^(1/5). For *w* = 64 that is *K* ≈ 2.3 — a fusion tree with a branching factor of two, which is to say a binary tree with expensive nodes.

**This is not a footnote. It is arguably the headline finding of your project**, and it is provable rather than merely measured. You can state it in one sentence: *at the word width of every computer ever built, the classical fusion tree degenerates.* Experiment 1 in Part VIII turns the "at most *r*⁴" worst case into an empirical distribution, so you can say how bad it actually is rather than how bad it could be.

---

## Part II — The physics of the machine

The word RAM says every operation costs one unit. Real hardware disagrees, and the disagreement is where your explanations will come from. You do not need semiconductor physics — you need four facts and their consequences.

### II.1 The memory hierarchy

A modern CPU core sits at the top of a pyramid of increasingly large, increasingly slow storage:

| Level | Typical size | Typical latency | In cycles at 3 GHz |
|---|---|---|---|
| Registers | ~1 KB | 0 | 0 |
| L1 data cache | 32–48 KB | ~1 ns | ~4 |
| L2 cache | 0.5–2 MB | ~4 ns | ~12 |
| L3 cache | 8–32 MB (shared) | ~15 ns | ~45 |
| Main memory (DRAM) | 8–64 GB | ~80 ns | ~240 |

Memory does not move one byte at a time. It moves in **cache lines**, almost universally 64 bytes — which is exactly eight 64-bit keys. Touching one key drags its seven neighbours along at no extra cost.

> **WHY this single fact decides your experiment.** A B-tree node holding 8 keys is one cache line: one memory transaction gives you all eight comparisons. **[Correction]** The *keys* are one cache line; with its child indices the whole node in this code tree is 112 bytes. A fusion node holding 8 keys must fetch its keys *and* its packed sketch word *and* its important-bit metadata. It does more arithmetic to save comparisons — but comparisons were never the expensive part. Memory traffic was. The fusion tree optimises the wrong resource, and it optimises it against a model that could not see the right one, because the word RAM was formalised before the memory wall became the dominant cost in computing.

### II.2 Latency versus throughput

Two different questions about an instruction:

- **Latency** — how long from issue to result available.
- **Throughput** — how many can be started per cycle.

A 64-bit integer multiply on a modern Intel core has latency ≈ 3 cycles and throughput ≈ 1 per cycle. So multiplications are cheap *if independent* and expensive *if chained*, because each must wait for the last.

The fusion node's comparison is a chain: add, multiply, subtract, AND, popcount — each depends on the previous. That is roughly 3 + 3 + 1 + 1 + 3 ≈ 11 cycles of pure dependency, and no amount of instruction-level parallelism helps. The B-tree's eight comparisons are *independent* and vectorise into two or three instructions with no chain at all.

This is a mechanism you can name, quantify, and defend. It is the sort of sentence that makes a report good: *"the fusion node loses not because it executes more instructions, but because its instructions form a dependency chain roughly 11 cycles deep, while the baseline's do not."*

### II.3 Branch prediction

The CPU guesses which way a branch will go and speculatively executes ahead. A correct guess is free; a wrong guess costs roughly 15–20 cycles of discarded work.

Binary search on random data branches unpredictably at every step: half the guesses are wrong. That is why the baseline in this project uses a **branchless** binary search, which replaces the branch with a conditional move. If you benchmark against a naive branching binary search, you will beat it — and the win will be an artefact of a weak baseline rather than a property of your structure. Always compare against the strongest reasonable opponent.

### II.4 What a "cycle" buys you

At 3 GHz, one cycle is about 0.33 nanoseconds. A predecessor query over a million keys takes about 20 levels of binary search, and if most of those levels miss cache, the query is dominated by roughly 20 × 80 ns of memory latency, not by arithmetic. This is why all three structures in this project converge toward similar costs at large *n*: at that point every one of them is simply waiting for DRAM, and the algorithm has stopped mattering.

That convergence is itself measurable and is a legitimate part of the finding.
---

## Part III — Setting up, tomorrow morning

This is Day 1. Budget two hours. Do it in order.

### Step 1 — Confirm you have a compiler

```
g++ --version
```

You want GCC 11 or newer (for C++20). On a lab Windows machine, install **MSYS2** from msys2.org, then in the MSYS2 terminal:

```
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make git
```

On Linux:

```
sudo apt install g++ make git
```

> **CHECKPOINT 1.** `g++ --version` prints a version ≥ 11. If it does not, stop and fix this before anything else.

### Step 2 — Record your hardware, permanently

Every number you produce is meaningless without the machine it came from. Create `results/machine.txt` on the first day and never edit it afterwards.

On Linux:

```
lscpu > results/machine.txt
free -h >> results/machine.txt
g++ --version >> results/machine.txt
```

On Windows (PowerShell):

```
Get-CimInstance Win32_Processor | Format-List * > results\machine.txt
```

What you need in there: CPU model, base clock, L1/L2/L3 cache sizes, RAM size, compiler version. You will quote all of these in the report.

### Step 3 — Check for BMI2

The fast sketch uses the `PEXT` instruction. Check:

```
grep -o bmi2 /proc/cpuinfo | head -1        # Linux
```

If `bmi2` appears, you have it. If not, remove `-mbmi2 -DFT_USE_PEXT` from the Makefile — everything still builds using a portable fallback, just more slowly. Either way, **record which one you used.**

### Step 4 — Create the project

```
mkdir fusion-tree && cd fusion-tree
git init
mkdir -p include src tests bench analysis results figures build
```

Then create the files from Part IV. Commit after each one:

```
git add -A && git commit -m "fusion node: parallel comparison"
```

> **WHY commit constantly.** When a benchmark result changes and you cannot remember what you changed, `git diff` answers in one second what guesswork answers in an hour. This is not bureaucracy; it is the difference between a project that finishes in November and one that does not.

### Step 5 — Build and test

```
make test
```

> **CHECKPOINT 2.** You should see `checks=8483288 failures=0` and then `checks=12000000 failures=0` (the counts before the node variants were added were 1720312 and 3600000). If either reports failures, go to Part IX before doing anything else. Do not benchmark a structure that is not correct — the numbers will be meaningless and you will not find out until December.

---

## Part IV — The code

The full code tree accompanies this manual. Five files:

| File | What it is |
|---|---|
| `include/bits.hpp` | The three hardware primitives: msb, popcount, bit extraction |
| `include/sketch_fast.hpp` | O(1) sketching via PEXT, and the Fredman–Willard multiplier search |
| `include/fusion_node.hpp` | The fusion node: important bits, sketches, parallel comparison, rank |
| `include/structures.hpp` | The fusion tree and the two baselines |
| `tests/`, `bench/`, `analysis/` | Correctness tests, the two experiments, the R analysis |

Read them in that order. Each is commented at the level of "why", not just "what". A few points that the comments cannot carry on their own:

**On `msb`.** `__builtin_clzll` counts *leading zeros*, so the index of the top set bit is 63 − clz. This compiles to a single `LZCNT` or `BSR` instruction, which is why the O(1) claim in the theory survives contact with the hardware. It is undefined for zero — the code asserts rather than silently returning garbage.

**On the naive versus fast sketch.** `extract_bits_naive` loops once per important bit, which is O(*r*), not O(1). Build with it first: it is obviously correct and it is your oracle. Then switch to `sketch_pext` and confirm the tests still pass. Keeping both, selected by a compile flag, means you can measure exactly what the O(1) sketch buys — which is a result in itself.

**On why the tree is static.** It is built once from a sorted array and never updated. Insertion into a fusion tree requires recomputing important bits, sketches and all three packed constants for the affected node, plus B-tree-style splitting. It is perhaps 400 additional lines and it does not change the question being asked. Say so explicitly in the report's limitations section; do not let a reader think you did not notice.

**On why the B-tree and the fusion tree share a shape.** They have identical node arity, identical separators, identical build procedure, identical descent loop. **[Correction]** Their memory layouts are *not* identical: a B-tree node is 112 bytes and a fusion node 184 bytes. `FusionNodeCompact` with 64-byte alignment makes both 128 bytes; Experiment 3 uses that pair. The *only* difference is the within-node search. This is deliberate: any measured difference is therefore attributable to the thing under study and not to some unrelated layout accident. Preserving this property is more important than making either structure marginally faster.

---

## Part V — Testing

Your correctness argument rests on **differential testing**: run the structure and a trivially correct oracle on the same inputs and require exact agreement.

Two oracles are used:

- For the node, a linear scan (`brute_rank`). Obviously correct.
- For the whole tree, `std::lower_bound`. Independently written, heavily used, not by you.

The input generator deliberately includes adversarial cases, because uniform random 64-bit keys are the *easiest* possible input for this structure — they share almost no prefixes, so important bits are all near the top and sketching is nearly faithful. The four modes are:

| Mode | Keys | What it stresses |
|---|---|---|
| 0 | Uniform over 2⁶⁴ | The easy case |
| 1 | Shared 40-bit prefix | Important bits pushed low; sketches nearly collide |
| 2 | Small range (< 1024) | Many shared high bits |
| 3 | Scattered high bits only | Non-contiguous important-bit sets |

Queries include every stored key, every stored key ± 1, and random values. The ±1 cases matter most: they are exactly the queries whose sketch collides with a stored key's sketch, which is where the desketchify logic either works or does not.

> **WHY 5.3 million checks and not 500.** The strict/non-strict bug described in Part I.6 was wrong on 8.5% of queries. A 500-query test would have caught it. But a subtler variant — wrong only when the query's divergence bit falls below all important bits *and* the node is full — might be wrong on 0.01% of queries, which a small test passes comfortably and which would corrupt your entire benchmark. Cheap insurance.

---

## Part VI — Benchmarking methodology

This is where projects like this are won or lost. Naive microbenchmarking reliably produces confident wrong numbers.

### The four rules, and what breaks without each

1. **Nothing may be optimised away.** Every query result is XORed into a checksum that is printed. Without this, the compiler observes that your loop has no effect and deletes it — and you report a structure that answers queries in 0.0 ns.
2. **No work inside the timed region that is not under test.** Queries are generated into an array beforehand. Generating random numbers inside the loop measures the random number generator, which on some settings costs more than the query.
3. **Repeat and take the median.** A single run that is interrupted by the OS scheduler is not an outlier to be explained; it is noise. Fifteen repetitions, median reported, interquartile range shown on the plot.
4. **Discard a warm-up pass.** The first pass pays for page faults, cold caches, and branch predictors that have never seen this code. Those are real costs but they are not the steady-state cost you are reporting.

### What to control

- **Close everything else.** A background browser tab will move your numbers more than any algorithmic difference you are trying to measure.
- **Pin the CPU** if you can: `taskset -c 2 ./build/bench_pred` on Linux. **[Correction]** On a hybrid CPU, CPU 2 may be an *efficiency* core. On the Core Ultra 7 255HX the performance cores are logical CPUs 0, 1, 6–9, 18 and 19. The benchmark now pins itself: `make bench CPU=8`.
- **Note whether turbo boost is on.** If the clock varies, the measurements vary. You cannot always disable it on a lab machine; you can always record it.
- **Run both compilers.** `make CXX=g++` and `make CXX=clang++`. If they disagree substantially, that is a finding about compilers, not about algorithms, and it belongs in the report.
- **Randomise the order** of structures across repetitions if you can. Systematic ordering can bake in cache-warming effects.

### The statistics you need, and no more

- **Median** — the central estimate. Robust to interrupted runs.
- **Interquartile range** — the spread. Report it on every figure.
- **Bootstrap confidence interval** — if you want an interval on the median, resample the repetitions with replacement 10,000 times and take the 2.5th and 97.5th percentiles of the resampled medians. Five lines of R; no distributional assumption required.

Do not run a t-test on timing data. It assumes normality that timing data does not have, and you do not need it: you are estimating a magnitude, not testing whether a difference exists.

---

## Part VII — The experiments

### Experiment 1 — How wide is the sketch window really?

**Question.** The theory bounds the multiplication sketch's window at *r*⁴ bits. For *r* = 7 that is 2401 bits — 37 machine words. Is the worst case the typical case?

**Method.** `bench/spread.cpp`. Generate random *K*-key sets, compute important bits, run the greedy multiplier search, record the achieved window width. Ask whether *K* fields of that width fit in 64 bits.

**Why it matters.** If the window is typically small, the classical construction is viable at *w* = 64 and the interesting question is purely about constants. If it is typically large, the classical construction is *impossible* at *w* = 64 and you have a structural result, not just a timing result.

### Experiment 2 — The timing campaign

**Question.** Nanoseconds per predecessor query for each structure across *n* = 2⁸ to 2²².

**Method.** `bench/bench_pred.cpp`, 15 repetitions per size, median reported. Run twice: once with the O(*r*) loop sketch, once with `-DFT_USE_PEXT`. The difference isolates the cost of the sketch operation itself.

**What to look for.** Not just which is faster — the *ratio* and how it moves with *n*. A ratio that falls as *n* grows implies a crossover somewhere; extrapolating tells you where. A flat ratio implies none.

### Experiment 3 — Hardware counters

**Question.** *Why* is one structure slower? Name the mechanism.

**Method (Linux):**

```
perf stat -e cycles,instructions,branch-misses,L1-dcache-load-misses,LLC-load-misses \
    ./build/bench_pred 5 20
```

Derive per-query figures by dividing by the total query count. The three numbers that matter:

- **Instructions per query** — is the fusion tree doing *more work*?
- **Cycles per instruction** — is it *stalling*?
- **Cache misses per query** — is it *moving more memory*?

If instructions are lower but cycles are higher, the answer is the dependency chain of Part II.2. If cache misses are higher, the answer is the memory traffic of Part II.1. Either is a real explanation; "constant factors" is not.

On Windows without `perf`, use Intel VTune (free) or fall back to Valgrind's `cachegrind`, which simulates rather than measures — usable, but say so.

> **[Update]** On the project laptop, WSL2 exposes no hardware performance counters (no `cpu` event source), and VTune is not installed. Experiment 3 was therefore done in three parts: `make node-lat` times one node search in cycles with everything in L1 (the dependency chain of Part II.2); `make traffic` counts the cache lines each query touches and runs them through a model of the cache hierarchy (like cachegrind, data only); and the timing campaign includes ablations that remove one mechanism at a time (branch-free rank, equal-footprint nodes).

### Experiment 4 — Does a wider word help?

**Question.** The whole limit is *K*² ≤ *w*. Modern CPUs have 512-bit vector registers. What if *w* = 512?

**Method.** Reimplement the parallel comparison over an AVX-512 register (or 256-bit AVX2, more widely available) instead of a scalar word. This permits *K* = 22 at *w* = 512. Measure whether the ratio moves, and by how much.

> **[Update]** The project laptop has AVX2 but no AVX-512, so Experiment 4 uses *w* = 256 and *K* = 16 (`include/fusion_node_wide.hpp`, structure `fusion16_w`), compared against a 16-key B-tree (`btree16_bl_a64`). `make spread` now also asks whether the classical multiplication sketch fits in 256 bits.

**Why this is the best use of your remaining time.** It converts a negative result into a quantitative statement about hardware: *"the fusion tree would need a word width of at least X bits to be competitive, which is Y times wider than anything that exists."* That is a considerably better thing to hand a reader than a second half-finished data structure. Treat it as optional — it is the first thing to cut if the schedule slips.

---

## Part VIII — Preliminary results

The reference implementation accompanying this manual was built and run before you received it. These numbers came from a virtualised Xeon at 2.1 GHz with a 48 KB L1 and a shared L3, which is *not* a clean measurement environment — a virtual machine adds noise and the shared cache is not yours alone. Treat them as an indication of what to expect, not as data for your report. **Your first real task after the tests pass is to reproduce these on your own hardware.**

**Experiment 1 — achieved sketch window width (400 random key sets per size):**

| *r* | median width | max width | theoretical bound *r*⁴ | *K* fields fit in 64 bits? |
|---|---|---|---|---|
| 2 | 4 | 15 | 16 | 100% |
| 3 | 11 | 18 | 81 | 82% |
| 4 | 18 | 30 | 256 | 0% |
| 5 | 30 | 45 | 625 | 0% |
| 6 | 42 | 66 | 1296 | 0% |
| 7 | 63 | 80 | 2401 | 0% |

Read that carefully, because it is a genuine finding. The achieved window is far below the *r*⁴ worst case — 63 bits rather than 2401 for *r* = 7, a factor of 38 better than the theory promises. **And it is still too wide.** A single sketch consumes essentially a whole machine word, so the fields cannot be packed at all beyond *K* = 4. The classical multiplication-based fusion node is not merely impractical at *w* = 64; it does not fit. What makes the working implementation possible is `PEXT`, an instruction that did not exist in 1993 and that performs the sketch exactly, in *r* bits, in one instruction.

**Experiment 2 — nanoseconds per predecessor query (median of 7 repetitions):**

| *n* | binary search | B-tree (K=8) | fusion (loop sketch) | fusion (PEXT) | PEXT / B-tree |
|---|---|---|---|---|---|
| 256 | 44.6 | 37.6 | 105.0 | 83.1 | 2.21× |
| 4 096 | 70.7 | 63.3 | 162.8 | 124.1 | 1.96× |
| 65 536 | 122.2 | 102.3 | 236.2 | 188.7 | 1.84× |
| 524 288 | 220.6 | 163.7 | 330.9 | 288.4 | 1.76× |
| 1 048 576 | 341.3 | 296.2 | 543.9 | 540.1 | 1.82× |

Three things to notice, all of which are things to investigate rather than conclusions:

1. The fusion tree loses by roughly a factor of two throughout. Expected.
2. The PEXT sketch is worth 20–25% at small *n* and almost nothing at large *n* — consistent with arithmetic mattering when data is cached and memory dominating when it is not.
3. **The ratio narrows as *n* grows**, from 2.21× to about 1.8×. That is the fusion tree's asymptotic advantage beginning to show. Whether it continues to narrow, and where the trend would cross 1.0, is exactly what your extrapolation in `analyse.R` estimates — and the honest answer will almost certainly be a value of *n* far beyond addressable memory. Say so with a number.

---

## Part IX — Troubleshooting

**`test_node` reports failures.** Almost always the desketchify step. Check in this order: (a) is the `<=` / `<` pairing in `rank` the right way round — high branch uses `<=`, low branch uses `<`; (b) is *e* built from the *sibling* subtree, i.e. is bit *h* being flipped rather than merely truncated; (c) is `imp_` sorted ascending before sketches are built. The test prints the exact failing key set — reproduce it in a five-line program rather than staring at the code.

**Everything is exactly 0.0 ns/query.** The compiler deleted your loop. The checksum is not reaching the output.

**Times vary by more than 20% between repetitions.** Something else is running. Close it. If it persists on a lab machine, run the campaign when the room is empty and note the conditions.

**`_pext_u64` not declared.** Missing `-mbmi2`, or a CPU without BMI2. Drop the flag and the `-DFT_USE_PEXT` define.

**The fusion tree beats the B-tree.** Be suspicious before being pleased. Check that the B-tree baseline is compiled with the same optimisation level, that its node scan is not accidentally quadratic, and that both are answering the same query. An unexpectedly good result is much more often a bug than a discovery — and catching that yourself, in the report, is a mark of a serious project.

---

## Part X — The schedule

| Phase | Dates | What "done" means |
|---|---|---|
| 0 Define | 8–11 Sep | Research question and success criteria written down and committed |
| 1 Research | 14–25 Sep | You can explain Part I to Mr Beam without notes |
| 2 Design and baselines | 21 Sep – 2 Oct | Harness, both baselines, first timing curves |
| 3 Primitives | 5–16 Oct | msb, popcount, both sketches, each unit-tested |
| 4 Assembly | 19–30 Oct | Full fusion tree; 10⁷ differential checks, zero failures |
| 5 Measure | 2–13 Nov | Complete raw dataset committed, both compilers, counters collected |
| 6 Analyse | 16–20 Nov | Figures, fitted ratio model, mechanism identified |
| 7 Refine | 23–25 Nov | Anything that looked like your bug rather than the algorithm's is re-run |
| 8 Communicate | 30 Nov – 4 Dec | Report finished, repository public, README written |

You are ahead of this schedule on arrival: Phases 3 and 4 have working, tested reference code in hand. Use the time you have gained on Experiment 4, not on relaxing the deadline.

### The report skeleton

1. **Introduction** — what a galactic algorithm is; why nobody has measured this one.
2. **Background** — Part I, compressed to two pages.
3. **Implementation** — what you built, what you simplified (static only), how correctness was established.
4. **Method** — the four benchmarking rules, the machine, the statistics.
5. **Results** — Experiments 1–3, figures, the ratio model.
6. **Discussion** — the mechanism; the *K*² ≤ *w* structural bound; what a wider word would buy.
7. **Limitations** — static structure; one machine family; extrapolation is extrapolation.
8. **Conclusion** — the number: crossover at *n* ≈ …, versus addressable memory of ….

Target eight to twelve pages. Release the repository with a README that reproduces every figure with one command; state the commit hash in the paper.

---

## Part XI — Glossary

**Branching node** — a trie node with two children; its depth is an important bit.
**Cache line** — the 64-byte unit in which memory actually moves.
**Desketchify** — the repair step converting a query into a value whose sketch ranks faithfully.
**Fusion node** — a node storing ≤ *K* keys, answering rank in O(1) word operations.
**Important bit** — a bit position at which two adjacent sorted keys first differ.
**PEXT** — BMI2 instruction extracting bits at masked positions; a one-instruction sketch.
**Predecessor query** — largest stored key strictly below *q*.
**Rank** — number of stored keys strictly below *q*.
**Sentinel bit** — the spare high bit per field that survives a subtraction to signal a comparison result.
**Sketch** — a key reduced to its bits at the important positions.
**Word-level parallelism** — doing many small operations at once inside one machine word.
**Word RAM** — the model in which arithmetic on a *w*-bit word costs one unit.

---

## Part XII — References

Bender, M. A., Demaine, E. D., and Farach-Colton, M. (2000). Cache-oblivious B-trees. *Proc. 41st IEEE Symposium on Foundations of Computer Science (FOCS)*, 399–409.

Chazelle, B. (2000). A minimum spanning tree algorithm with inverse-Ackermann type complexity. *Journal of the ACM* 47(6), 1028–1047. [Stage 2 of the project, if it happens.]

Cormen, T. H., Leiserson, C. E., Rivest, R. L., and Stein, C. (2009). *Introduction to Algorithms*, 3rd ed. MIT Press. [B-trees, tries, asymptotic notation.]

Demaine, E. D. (2012). *6.851 Advanced Data Structures*, Lecture 12: Fusion trees. MIT OpenCourseWare. [The clearest secondary exposition of the construction; read alongside the original.]

Fredman, M. L., and Willard, D. E. (1993). Surpassing the information theoretic bound with fusion trees. *Journal of Computer and System Sciences* 47(3), 424–436. doi:10.1016/0022-0000(93)90040-4 — **the primary source; read this one properly.**

Fredman, M. L., and Willard, D. E. (1994). Trans-dichotomous algorithms for minimum spanning trees and shortest paths. *Journal of Computer and System Sciences* 48(3), 533–551.

Intel Corporation. *Intel 64 and IA-32 Architectures Optimization Reference Manual*. [Instruction latencies and throughputs; cite the specific table you use.]

LaMarca, A., and Ladner, R. E. (1996). The influence of caches on the performance of heaps. *ACM Journal of Experimental Algorithmics* 1, Article 4. [The template for "the memory hierarchy beat the asymptotics".]

Larkin, D. H., Sen, S., and Tarjan, R. E. (2014). A back-to-basics empirical study of priority queues. *Proc. ALENEX*, 61–72; preprint arXiv:1403.0252. [Your methodological model. They report that L1 miss rate correlates strongly with wall-clock time — a finding you should try to replicate.]

Mytkowicz, T., Diwan, A., Hauswirth, M., and Sweeney, P. F. (2009). Producing wrong data without doing anything obviously wrong! *Proc. ASPLOS XIV*. [Read before Phase 5. Source of the measurement-bias controls in Part VI.]

Pătraşcu, M., and Thorup, M. (2014). Dynamic integer sets with optimal rank, select, and predecessor search. *Proc. 55th IEEE Symposium on Foundations of Computer Science (FOCS)*. [Where the theory went after fusion trees.]

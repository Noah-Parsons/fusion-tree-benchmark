# An explanation of the project and its current progress

This guide explains the entire project, in order, assuming you
know nothing about computer science.
Each chapter builds on the one before.

- To install and run things, use [START_HERE.md](START_HERE.md).
- To look up a word, use [GLOSSARY.md](GLOSSARY.md).

---

## Contents

1. [The problem](#chapter-1-the-problem)
2. [Why speed is about memory](#chapter-2-why-speed-is-about-memory)
3. [Three classic ways to search](#chapter-3-three-classic-ways-to-search)
4. [The fusion tree: a clever idea from 1993](#chapter-4-the-fusion-tree-a-clever-idea-from-1993)
5. [Part 1: racing the fusion tree](#chapter-5-part-1-racing-the-fusion-tree)
6. [How to race fairly](#chapter-6-how-to-race-fairly)
7. [Part 2: can anything beat the best tree?](#chapter-7-part-2-can-anything-beat-the-best-tree)
8. [Part 3: real data](#chapter-8-part-3-real-data)
9. [Part 4: learned indexes](#chapter-9-part-4-learned-indexes)
10. [Part 5: finding the weakness](#chapter-10-part-5-finding-the-weakness)
11. [Part 6: data that changes](#chapter-11-part-6-data-that-changes)
12. [Part 7: is it new?](#chapter-12-part-7-is-it-new)
13. [Part 8: the missing rivals, and where things stand now](#chapter-13-part-8-the-missing-rivals-and-where-things-stand-now)
14. [Lessons](#chapter-14-lessons)
15. [Check yourself](#chapter-15-check-yourself)

---

## Chapter 1: The problem

### Searching a sorted list

Imagine a list of numbers in order:

```text
3   8   15   21   40
```

Someone gives you a number, say **20**, and asks:

> What is the **biggest number in the list that is smaller than 20**?

The answer is **15**. A few more:

| Question (q) | Answer | Why |
|---|---|---|
| 20 | 15 | 15 is the biggest number below 20 |
| 21 | 15 | "smaller than" means 21 itself doesn't count |
| 100 | 40 | everything is below 100, and 40 is the biggest |
| 2 | none | nothing in the list is below 2 |

This is called a **predecessor search**. Each question is a **query**, and
each stored number is a **key**.

### Why it matters

Computers do this constantly:
- **Databases:** "find all orders placed before 3 PM".
- **Internet routers:** "which route covers this address?"
- **Programs:** "what was the last event before this moment?"

It has to be fast, because it happens billions of times.

### What this project does

It builds many different ways of answering this question, then **races them
on a real laptop**, fairly, to find out which is fastest, and **why**.

The lists are big: from 256 numbers up to **33.5 million** (written 2^25,
because computers count in powers of two).

---

## Chapter 2: Why speed is about memory

This chapter explains why some methods are fast and others slow. It is the
key to everything else.

### Two kinds of memory

A computer has:
- **Cache:** a tiny amount of very fast memory, right next to the processor.
  Like the books on your desk.
- **RAM:** a large amount of slower memory, further away. Like a library
  basement.

Getting something from RAM is about **100 times slower** than getting it from
the cache.

33 million numbers don't fit in the cache, so most of the list lives "in the
basement". **A fast search is mostly one that makes few trips to the
basement.**

Memory moves in chunks of 64 bytes, called **cache lines**. Fetching one number
brings along its neighbours in the same chunk for free.

### The processor guesses ahead

A modern processor doesn't wait for one step to finish before starting the
next. It **guesses** what comes next and starts early.
- When a program chooses between two paths (a **branch**, like "if the number
  is bigger, go left"), the processor guesses which path it will take.
- **A wrong guess wastes time.** With random data, guesses are often wrong.
- Code with no such choices is called **branch-free**, and there is nothing to
  guess wrong.

### Doing several things at once

Modern processors have special instructions (**SIMD**; the kind used here is
called **AVX2**) that compare **four numbers in a single step**.

### Two ways to measure speed

- **Throughput:** many independent searches, as fast as possible. The
  processor can **overlap** them, starting the next one while the last is
  still waiting for memory.
- **Latency:** one search at a time. Each search must wait for the previous
  one.

A method can win one and lose the other. This comes back again and again.

---

## Chapter 3: Three classic ways to search

### 1. Binary search (`sorted_array`)

Look at the middle number. Is q bigger or smaller? Throw away the wrong half.
Repeat.

For 33 million numbers that is only **25 looks**, which sounds fast. But each
look lands somewhere far away in the list: **almost every look is a trip to
the basement.** Result: **723 nanoseconds** per search. (A nanosecond is a
billionth of a second.)

### 2. B-tree (`btree8`)

Put the numbers into small **boxes (nodes) of 8**. Each box tells you which
box to open next.
- **One trip to memory brings back a whole box**, all 8 numbers.
- Splitting 9 ways each time means about **8 trips** instead of 25.

The "early-exit" version checks the box left to right and stops at the first
number that is too big. But the processor has to guess where it will stop, and
often guesses wrong.

### 3. Branch-free B-tree (`btree8_bl`)

The same boxes, but compare **all 8 numbers at once** with AVX2 and count how
many are smaller. No stopping means no guessing.

Result: **186 nanoseconds**, almost 4× faster than binary search. This is the
rival everything in Part 1 has to beat.

---

## Chapter 4: The fusion tree: a clever idea from 1993

### The promise

In 1993, Fredman and Willard invented the **fusion tree**. Mathematically it
is faster than any method that compares numbers one at a time. It is famous in
computer science, and almost never actually built, because people suspect it
is only fast on paper.

### The idea in three tricks

The fusion tree uses the same boxes as a B-tree, but searches each box
differently.

**Trick 1: important bits.** Numbers are stored as bits (0s and 1s). Among the
keys in a box, only a few bit positions matter for telling them apart. These
are the **important bits**.

**Trick 2: sketches.** Keep only the important bits of each key. This short
version is its **sketch**, and the sketches stay in the same order as the
keys. An instruction called **PEXT** makes a sketch in one step.

**Trick 3: comparing everything at once.** Pack all the sketches of a box into
one number. With **one subtraction**, compare q's sketch against every sketch
together.

### The catch: the repair step

q usually isn't one of the stored keys, and its sketch can put it in the wrong
place. So the fusion tree needs a **repair step**: find where q first differs
from its closest key, build a stand-in number, sketch that instead, and compare
again.

That repair makes each box search a **long chain of steps**.

### A worked example

Keys (6-bit numbers): A = 6, B = 19, C = 23, D = 49. The important bits are
positions 5, 4 and 2, so the sketches are A=`001`, B=`010`, C=`011`, D=`110`.

The query q = 20 (`010100`) has sketch `011`, the same as C. The sketch alone
says the answer is C = 23. **That's wrong:** 23 isn't smaller than 20. The
repair step notices that q and C differ at bit 1, builds the stand-in 22, and
finds the right answer, B = 19.

(You don't need every detail of the repair to follow the rest. What matters
is that it adds many steps to every box search.)

---

## Chapter 5: Part 1: racing the fusion tree

### The question

> Does a carefully built fusion tree ever beat a B-tree or binary search, on a
> real computer, at any list size? And if not, **why**?

### What was built

- the fusion tree (`fusion8`);
- three variations, each changing one thing:
  - no branches (`fusion8_bf`);
  - a smaller box (`fusion8_c`);
  - 16 keys on a wider number (`fusion16_w`);
- binary search and several B-trees.

**Each pair differs in exactly one thing**, so any difference in speed has one
cause.

### Experiment 1: can the 1993 version even be built?

The 1993 paper made sketches with a multiplication trick. Measured on random
keys, that trick **only fits 4 keys per box** in a 64-bit number, not 8. The
fusion trees raced here work only because of **PEXT**, an instruction added to
processors in 2013.

### Experiment 2: the race

Every size from 256 to 33.5 million, two compilers, both measuring modes,
three separate sessions: **48,600 timings** in all. The results at 33.5
million keys:

| Method | Throughput (ns) | Latency (ns) |
|---|---|---|
| branch-free B-tree | **186** | 475 |
| early-exit B-tree | 442 | 468 |
| fusion tree | 579 | 577 |
| binary search | 723 | 1407 |

**What it means:**
- **It does beat binary search** on big lists, from about 4 million keys in
  latency and 17 million in throughput.
- **But not because of the fusion trick.** The early-exit B-tree also compares
  numbers one at a time, yet it beats binary search even sooner. So the win
  comes from **the box shape** (fewer trips to memory), not the clever
  sketches.
- **It never beats the branch-free B-tree**, at any size. And the gap in
  nanoseconds **grows** as the list grows.

### Experiment 3: why it loses

Four separate measurements agree:
1. **Longer chain of steps:** one box search takes about **60** processor
   cycles for the fusion tree against **28** for the B-tree.
2. **More instructions:** hardware counters (Intel VTune) showed **2.2× more
   instructions** per search (441 against 200).
3. **Not memory:** a fusion box shrunk to exactly the B-tree box's size was
   **still 2.8× slower**.
4. **No crossover:** in latency mode the ratio shrinks (1.8× down to 1.2×),
   which looks like catching up. But the actual gap grows from **18 ns to
   102 ns**. Both just wait longer for memory on bigger lists.

### Experiment 4: a wider number?

With 256-bit numbers, 16 keys fit in a box. **It is no faster.**

### The lesson of Part 1

> **Being faster on paper is not the same as being faster on a real computer.**
> Real speed is about memory trips and short chains of steps, not about
> counting comparisons.

---

## Chapter 6: How to race fairly

A race is only worth something if it is fair. The following rules are applied:

### Correct answers first

Every method is checked against a slow method known to be correct, **millions
of times** (more than 27 million checks in the main test). A fast wrong answer
is worthless.

### Every race checks itself

All the answers in a race are combined into one number, a **checksum**. Every
method must produce the **same checksum**, which proves they all answered the
same questions correctly.

### Careful timing

- **Queries made in advance**, so the random number generator isn't timed too.
- **A warm-up pass**, thrown away.
- **Repeat, and take the median** (the middle value), so one slow moment can't
  skew the result.
- **Shuffle the order** of methods in every repetition.
- **Pin the program to one fast processor core.** This laptop has fast cores
  and slow cores, and Windows could otherwise move the program mid-run.

### A story: the compiler bug that nearly changed the answer

In the middle of Part 1, one number suddenly changed. The branch-free B-tree
took 443 ns in one program and 193 ns in another, **from identical code**. The
compiler had quietly turned the loop into fast SIMD instructions in one
program, but not in the other.

With the slow version, the fusion tree looked only 1.2× behind instead of
3.0×. The fix was to write the SIMD instructions by hand, and rerun everything.

**Lesson: when a number moves, find out why before believing it.**

### Writing the rules down first

From Part 2 onward, every race has a **plan file** (`PLAN.md`), written
**before** any timing. It says:
- what exactly will be measured;
- **how the winner will be decided;**
- **what we expect to happen.**

After the race, the outcome is added to the same file, including when the
expectation was **wrong**.

This stops anyone from changing the rules after seeing results.

### Confirm on fresh data

Settings are tuned on one set of random data. Then the real race runs on
**three new sets**. **A win only counts if it happens all three times.**

---

## Chapter 7: Part 2: can anything beat the best tree?

### The new rival: the S+ tree

The **S+ tree** (by Sergey Slotin) is the fastest known tree for data that
never changes:
- **no pointers:** a box's children are found by arithmetic;
- all keys in the bottom row;
- AVX2 comparisons.

The goal: **build something faster.**

### Try 1: RadixJump

**Idea:** skip the tree. Use the **first bits** of q as a position in a table,
like opening a dictionary straight to the right letter, then check the few
numbers there.

- **Evenly spread numbers:** **3.3× faster** than the S+ tree (throughput).
- **Clumped numbers:** **it falls apart.** Most of the table is empty, and a
  few spots hold huge piles of numbers.

### Try 2: SplineIndex

**Idea:** draw straight lines through the data (number against position), with
a guarantee of never being more than E places off. Pick the right line, guess
the position, then check nearby.

- Clumps don't hurt it.
- It **wins latency by 16%**, but **loses throughput by 21%**. Its search has
  too many steps, which is the fusion tree's problem again.

**An honest detail:** the rule written in advance picked a setting that won
nowhere. The better setting (E = 32) was picked **after** seeing results, so it
is labelled that way everywhere.

### Try 3: ClusterJump

**Idea:** RadixJump **plus a second, zoomed-in table for each region.** A
crowded region gets its own fine table, and an empty region costs almost
nothing.

Confirmed on three fresh data sets, against the faster S+ tree (below 1 means
ClusterJump is faster):

| | Evenly spread | Clumped |
|---|---|---|
| Throughput | **0.45** (2.2× faster) | **0.40** (2.5× faster) |
| Latency | 1.01 (tie) | **0.92** (8% faster) |

### Why they split into throughput versus latency

- **Throughput rewards short searches.** Many searches overlap while waiting
  for memory, and a long search crowds the others out.
- **Latency rewards fewer memory waits in a row.**

---

## Chapter 8: Part 3: real data

Made-up data can hide problems. Researchers use a standard set of **real**
data called **SOSD**, four lists of 200 million real numbers:
- **books:** Amazon book popularity;
- **wiki:** Wikipedia edit times;
- **fb:** Facebook user IDs;
- **osm:** map locations.

**Result:** ClusterJump beat the S+ tree on all four:
- **Throughput:** 1.6–2.3× faster.
- **Latency:** equal, or up to 5% faster.

Facebook IDs and map locations were harder (smaller wins), as predicted in the
plan.

---

## Chapter 9: Part 4: learned indexes

### What is a learned index?

Instead of a tree, a **learned index** learns the **shape** of the data. It's
like drawing a graph of "number" against "position in the list": fit lines to
it, then use the lines to predict where a number is, and check nearby.

Two of the best known:
- **RadixSpline** (MIT and TU Munich, 2020);
- **PGM-index** (University of Pisa, 2020).

### Keeping it fair

- **The same last step:** both finish with exactly ClusterJump's final search
  step, so nobody wins on the last step alone.
- **Tuned by a rule:** each tried its authors' recommended settings, and the
  best was chosen by a rule written in advance.
- **Memory measured** for everyone.

### Result: a split

| | ClusterJump against the learned indexes |
|---|---|
| **Throughput** | **ClusterJump wins on all four datasets:** 1.3–2.6× faster than RadixSpline, 2.2–2.4× faster than PGM |
| **Latency** | **The learned indexes win**, by 6–31% (except RadixSpline on Facebook IDs) |
| **Memory** | about equal: 8–9 bytes per number |

---

## Chapter 10: Part 5: finding the weakness

Good science tries to **break its own result**.

ClusterJump's second table adapts to clumps at **one scale**. What if the
clumps are **made of smaller clumps**? The plan predicted, before running,
that ClusterJump would lose.

**Result: it lost to every rival, 1.6–2.2× slower.** It was even slower than
the plain B-tree. So now we know exactly where ClusterJump breaks.

---

## Chapter 11: Part 6: data that changes

### The problem

Everything so far is built once and never changes. Real databases **add
numbers all the time**.

### ClusterJumpD

Every slot of ClusterJump's second table becomes **a small box of 16
numbers**:
- a new number goes into its box;
- a full box spills into an overflow list;
- when too much overflows, that region is rebuilt with more boxes.

### The rivals

- **TLX B+ tree:** a widely used tree for changing data.
- **ALEX:** the best-known learned index for changing data (Microsoft Research
  and MIT).

### Bugs found first

The correctness tests found **three bugs in ALEX itself**:
1. **Wrong answers** for questions far outside the stored numbers. Its model's
   guess overflows a small integer. This was worked around.
2. **Wrong answers** for huge, very closely spaced numbers.
3. **A crash** on one extreme input.

ClusterJumpD and TLX had **zero wrong answers**.

### The race

Operations were a mix of searching and adding numbers: 10% adds or 50% adds.

- ClusterJumpD beat **both** rivals on **five of the six** datasets.
- **The weak spot is Facebook IDs.** A few enormous IDs squeezed almost every
  number into one region, where they piled into huge overflow lists. In one of
  three sessions it was **up to 12× slower** than ALEX. It's the same
  clumps-inside-clumps weakness, triggered by real data.
- It uses **the most memory**.

---

## Chapter 12: Part 7: is it new?

**Answer: no, it isn't new.** In 2021, Andrew Crotty published the
**Hist-Tree**. It also splits the number range into equal slices and zooms into
crowded slices, and it keeps zooming **as many levels as needed**. That
unlimited zooming is exactly what fixes ClusterJump's clumps-inside-clumps
weakness.

ClusterJump is essentially **a two-level Hist-Tree.**

**So what is this project's contribution?**
- rules written in advance;
- fresh-data confirmation;
- identical final search steps for every method;
- failure cases found and explained;
- bugs found in a published library.

---

## Chapter 13: Part 8: the missing rivals, and where things stand now

### Why this part exists

ClusterJump still hasn't raced its two most
important rivals:
- **CHT (Compact Hist-Tree):** its closest relative.
- **RMI (Recursive Model Index):** the original learned index (Google and MIT,
  2018), which usually wins latency on this data. Latency is already where
  ClusterJump loses.

**Without them, the claims aren't complete.**

### What has been done

- **CHT** was downloaded and wrapped. Like another library before it, it
  needed one small fix to work on Windows, and it passes the correctness tests.
- **RMI** isn't a library. A separate tool **generates custom code** for each
  specific list of numbers. So:
  1. the exact 33.5-million-key lists for each session were saved to files;
  2. the RMI tool found the 10 best designs for each dataset;
  3. 40 RMIs were generated;
  4. all 40 were checked: **80 million answers, all correct**.

### What went wrong, and how it was fixed

This part explains a lot about running experiments safely.

1. **The laptop crashed (a blue screen).** The first attempt raced 25 methods
   at once, holding many large copies of the data. The crash report named the
   **NVIDIA graphics driver**, which had a bug that heavy memory use exposed.
   The benchmark doesn't use the graphics card, but updating the driver was
   still necessary.
2. **Memory hit 100%.** Even split into smaller groups, one CHT setting (1024
   slices with a small error limit) needed **many GB just to build** on the
   tightly packed Wikipedia data. The run was stopped by hand.
3. **A new, memory-safe program: `bench_one`.** It keeps **only one method in
   memory at a time**, and uses a **hard 8 GB memory limit enforced by
   Windows**. Any method that needs more is recorded as "too big" and skipped.
   Tests confirmed it: the too-big CHT setting was stopped safely, and normal
   methods used about half a GB.
4. **One more fix.** When a CHT build hit the limit at just the wrong moment,
   the program itself stopped. Now every CHT setting is **checked alone first**
   (a "pre-flight"), and only settings that fit are timed.

All of this is written down in `results/sosd_rivals/PLAN.md`.

### Exactly where the project is today

- Parts 1–7 are complete, confirmed and written up.
- CHT and RMI are set up, and pass their correctness tests.
- The memory-safe program works.
- The tuning race is finished for the **books** dataset.
- **The tuning race for the other three datasets stopped** with an error
  (exit code 4) that **has not been investigated yet**.
- Still to do after that:
  1. choose CHT's and RMI's best settings by the written rule;
  2. generate RMIs for the three fresh data sets;
  3. run the confirmation race;
  4. report honestly whether ClusterJump beats, ties or loses to each.

**What we expect** (written in the plan beforehand):
- **RMI and CHT will beat ClusterJump in latency.**
- **CHT may match or beat it even in throughput.**

If that happens, ClusterJump's remaining advantage is simplicity, not speed.

---

## Chapter 14: Lessons

1. **Real speed is about memory and short step chains**, not about the math of
   counting comparisons.
2. **Throughput and latency are different races.** Short searches win the
   first; fewer memory waits win the second.
3. **Check correctness before speed**, and check that every method gave the
   same answers.
4. **Write the rules down before looking at results**, and report when your
   prediction was wrong.
5. **Try to break your own result.** Nested clusters and Facebook IDs showed
   exactly where ClusterJump fails.
6. **Check whether it's new before claiming it.** ClusterJump turned out to be
   a two-level Hist-Tree.
7. **When a number changes unexpectedly, find out why.** That's how the
   compiler bug was caught.
8. **Protect the machine.** Big experiments need memory limits and small
   pieces.

---

## Chapter 15: Check yourself

Try to answer before looking at the answers below.

1. In the list 3, 8, 15, 21, 40, what is the predecessor of 21?
2. Why is binary search slow on a huge list, even though it needs only 25
   looks?
3. What does a branch-free B-tree do differently from an early-exit one?
4. The fusion tree beats binary search on big lists. Why isn't that proof its
   trick works?
5. What is the difference between throughput and latency?
6. What is a checksum for?
7. Why write a PLAN.md before timing?
8. What is ClusterJump's weakness, and why does it have it?
9. Why is ClusterJump not a new invention?
10. What stopped the Part 8 races, and what fixed it?

### Answers

1. **15.** 21 itself doesn't count, because the answer must be smaller.
2. Almost every look lands far away in memory, so it's a slow trip to RAM.
3. It compares all 8 numbers at once and counts, so the processor never has to
   guess where to stop.
4. An ordinary early-exit B-tree, which uses no trick at all, beats binary
   search even sooner. The win comes from the box shape, not from sketches.
5. Throughput lets many searches overlap; latency measures one search at a
   time.
6. It proves every method gave the same answers.
7. So the rules can't be changed after seeing the results.
8. Clumps inside clumps (and data with extreme outliers). Its second table
   zooms in only once, so a dense sub-clump piles into one slot.
9. The Hist-Tree (Crotty, 2021) already does the same thing, with as many
   zoom levels as needed.
10. Too much memory in use at once (and a graphics driver bug). The fix was
    `bench_one`: one method in memory at a time, a hard 8 GB limit, and
    pre-checking each CHT setting.

# Glossary: every word, in plain language

Alphabetical. If a word is used in this project and you don't know it, it
should be here.

---

**2^25 (two to the power 25)**
33,554,432, about 33.5 million. Sizes are written as powers of two because
computers work in binary. 2^8 = 256, 2^16 = 65,536, 2^20 ≈ 1 million,
2^22 ≈ 4 million, 2^24 ≈ 17 million.

**ALEX**
A published learned index (Microsoft Research and MIT, 2020) that can also
accept new numbers after it is built. One of the rivals in the changing-data
race.

**AVX2**
A set of processor instructions that compares several numbers **at the same
moment** (4 at a time for 64-bit numbers). Most fast methods here use it.
Your processor must support it.

**B-tree**
A way to organize a sorted list for fast searching. Numbers are grouped into
small boxes (nodes) of, say, 8. Each box tells you which box to look in next.
Because a whole box is fetched from memory at once, a B-tree makes far fewer
slow trips to memory than binary search.

**Benchmark**
A program that measures how fast something is.

**Binary search**
Look at the middle of the list; throw away the half that can't hold the
answer; repeat. Needs about 25 looks for 33 million numbers.

**BMI2 / PEXT**
BMI2 is a set of processor instructions. PEXT is one of them: it pulls
selected bits out of a number in one step. The fusion tree depends on it.

**Branch / branch-free**
A branch is a point where a program chooses between two paths ("if this,
do that"). The processor guesses which path is next so it can work ahead;
wrong guesses waste time. **Branch-free** code avoids such choices, so there
is nothing to guess wrong.

**Build**
(1) Turning source code into a runnable program. (2) Creating a search
structure from a list of numbers before searching it.

**Bulk load**
Building a structure from a whole sorted list at once, instead of adding
numbers one by one.

**Cache / cache line**
The cache is a small amount of very fast memory right next to the processor.
Main memory (RAM) is much bigger but about 100× slower to reach. Data moves
into the cache in 64-byte chunks called cache lines. Most of this project is
really about **making fewer slow trips to RAM**.

**CHT (Compact Hist-Tree)**
A published index (Crotty, 2021) that splits the number range into equal
slices, and splits crowded slices again, as many times as needed. It is the
closest published relative of ClusterJump.

**Checksum**
A single number made by combining all of a method's answers. If two methods
print the same checksum, they gave the same answers. Every race checks this.

**Clustered data / nested clusters**
Clustered: the numbers come in tight groups with big empty gaps between them.
Nested: each group is itself made of smaller groups.

**ClusterJump / ClusterJumpD**
The method built in this project. It uses the first bits of a number to jump
straight to roughly the right place, with a second, zoomed-in table for each
crowded region. ClusterJumpD is the version that can accept new numbers.

**Compiler (g++, clang)**
The program that turns C++ source code into a runnable program. Results are
checked with two different compilers.

**CSV**
A plain text table: one row per line, columns separated by commas. Opens in
Excel.

**Dataset**
A list of numbers used for a race. Some are made up (uniform, clustered), and
some are real (SOSD).

**Dependency chain**
A sequence of steps where each must wait for the one before. Long chains are
slow, because the processor cannot work on the steps in parallel.

**Fusion tree**
A search structure from 1993 (Fredman and Willard). In theory it beats every
method that compares numbers one at a time. In practice (this project) it does
not beat a good B-tree.

**Key**
One of the stored numbers.

**Latency**
How long **one** search takes when it cannot overlap with others. In latency
mode, each search is made to wait for the previous answer.

**Learned index**
A search method that **learns the shape** of the numbers (like fitting a line
to them) and uses that to guess where a number is, then checks nearby.
Examples here: RadixSpline, PGM-index, RMI.

**Median**
The middle value after sorting. Used instead of the average, because one slow
measurement (say, Windows doing something in the background) can pull an
average far off but barely moves a median.

**Memory cap**
A hard limit, enforced by Windows, on how much memory a program may use. The
`bench_one` program uses one so a race can never fill the computer's memory.

**Nanosecond (ns)**
One billionth of a second. Search times are measured in nanoseconds.

**Node**
A small box of numbers inside a tree structure.

**Pinning / P-core / E-core**
Pinning means telling the operating system to run a program on one specific
processor core, so it is not moved around mid-measurement. Some processors have
fast performance cores (P-cores) and slower efficiency cores (E-cores); the
benchmark should be pinned to a P-core. `CPU=-1` means "don't pin".

**PGM-index**
A published learned index (University of Pisa, 2020) that fits straight-line
pieces to the numbers.

**Pre-registered plan (PLAN.md)**
A file, written **before** a race was timed, saying exactly what would be
measured, how the winner would be decided, and what the expected result was.
This stops anyone (including us) from quietly changing the rules after seeing
the results.

**Predecessor search**
Finding the biggest stored number that is smaller than a given number q.
The single question this whole project is about.

**Query**
One search question: one number q.

**RadixSpline**
A published learned index (2020): a lookup table on a number's first bits,
pointing into a line-fitted model.

**Repetition (rep)**
Each measurement is repeated several times, and the median is reported.

**RMI (Recursive Model Index)**
The original learned index (Google and MIT, 2018). A small model picks a
bigger model, which guesses the position. Its code is **generated** for each
specific dataset by a separate tool.

**S+ tree**
The fastest known static search tree (Sergey Slotin, Algorithmica): a B-tree
with no pointers and SIMD comparisons.

**Seed**
A starting number for the random number generator. The same seed always gives
the same "random" data, so a race can be repeated exactly.

**Session**
One complete run of a race. Important results are confirmed in **three
separate sessions** with different seeds; a win only counts if it happens all
three times.

**SIMD**
"Single Instruction, Multiple Data": processor instructions that do the same
operation on several numbers at once. AVX2 is one kind.

**SOSD**
"Search On Sorted Data": a standard set of four real datasets (Amazon book
sales, Wikipedia edit times, Facebook user IDs, OpenStreetMap locations) that
researchers use to race search methods.

**Static / dynamic**
A static structure is built once and never changes. A dynamic one accepts new
numbers afterward.

**Throughput**
How many searches per second, when the processor is free to overlap several
searches. Reported as nanoseconds per search.

**TLX B+ tree**
A widely used B+ tree library, used as a rival in the changing-data race.

**Uniform data**
Random numbers spread evenly across the whole range.

**WSL**
"Windows Subsystem for Linux": a Linux system running inside Windows. Used
here only to run the RMI code generator.

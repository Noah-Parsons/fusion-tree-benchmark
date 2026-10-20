# Start here: from nothing to your first result

This guide assumes you know nothing about this project and have never built a
C++ program. Follow the steps in order. Every word you might not know is
explained in [GLOSSARY.md](GLOSSARY.md).

---

## 1. What is this project? (2-minute version)

A computer often has to answer this question very fast:

> Here is a big sorted list of numbers. Given a number **q**, what is the
> **biggest number in the list that is smaller than q**?

Example: the list is `3, 8, 15, 21, 40` and q is `20`. The answer is `15`.

This is called a **predecessor search**. Databases, internet routers and file
systems do it billions of times a day.

There are many ways to do it. This project **builds several of them and races
them against each other** on a real laptop, fairly and carefully, to find out
which is fastest and **why**.

It started with one famous idea from 1993, the **fusion tree**, which is faster
than everything else *in theory*. The project asks whether it is faster *in
practice*. (Short answer: no. It beats plain binary search on huge lists, but
it never beats a well-built B-tree.)

Later parts of the project race newer methods, including one built here called
**ClusterJump**. See [EXPERIMENTS.md](EXPERIMENTS.md) for every question and
answer.

---

## 2. What you need

### A computer that can run it

- **An x86-64 processor (Intel or AMD) from about 2013 or newer.** It must
  support two instruction sets, **AVX2** and **BMI2**.
  - Intel: 4th generation Core ("Haswell") or newer.
  - AMD: Ryzen or newer.
  - Apple Silicon Macs and Raspberry Pis **will not work**. They are ARM, not
    x86.
- **At least 8 GB of RAM** for the tests and small benchmarks. The large
  real-data races need **16 GB or more**. See the safety note in step 6.
- **Windows 10/11 or Linux.**

**Not sure whether your processor qualifies?** On Windows, open Task Manager →
Performance → CPU and search the processor's name online together with
"AVX2". On Linux, run `grep -o -m1 avx2 /proc/cpuinfo`; it prints `avx2` if
supported.

### Software

You need four free tools:

| Tool | What it is for |
|---|---|
| **Git** | downloading the project |
| **A C++ compiler (g++)** | turning the code into programs |
| **make** | running the build recipes in the `Makefile` |
| **R** (optional) | drawing the charts and summary tables |

---

## 3. Install the tools

### On Windows

1. **Install Git:** download it from <https://git-scm.com/download/win> and
   click through the installer with the default choices.
2. **Install MSYS2** (it provides g++ and make): download it from
   <https://www.msys2.org/> and install it to the default folder
   `C:\msys64`.
3. Open **"MSYS2 UCRT64"** from the Start menu and type:
   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc make
   ```
   Press Enter, then `Y` when it asks.
4. **(Optional) Install R** from <https://cran.r-project.org/>. Then open R and
   type `install.packages("ggplot2")`.

### On Linux (Ubuntu or Debian)

```bash
sudo apt update
sudo apt install git build-essential
```

On Arch Linux: `sudo pacman -S git gcc make`.

Optional, for charts: `sudo apt install r-base r-cran-ggplot2`.

---

## 4. Download the project

Open a terminal (PowerShell on Windows) and type:

```bash
git clone https://github.com/Noah-Parsons/fusion-tree-benchmark.git
cd fusion-tree-benchmark
```

You now have the whole project in a folder called `fusion-tree-benchmark`.

---

## 5. Build and check that everything is correct

Before timing anything, the project checks that every method gives
**correct answers**. It compares them against a slow method that is known to be
right, millions of times.

### On Windows (in PowerShell, inside the project folder)

Tell PowerShell where the MSYS2 tools are, for this window only:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
$env:TMP = "$PWD\build"; $env:TEMP = $env:TMP
New-Item -ItemType Directory -Force build | Out-Null
```

Then run the tests:

```powershell
make test
```

### On Linux

```bash
make test
```

If static linking fails on Linux, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

### What you should see

After a few minutes, several lines like these (the exact counts may differ):

```text
  <test name>          checks=... failures=0
  <test name>          checks=... failures=0
checks=27600000 failures=0
```

**Every line must say `failures=0`.** That means every method gave the right
answer every time. If you
see any failures, stop, and open an issue on GitHub.

---

## 6. Run your first race

This races the main methods on lists of 256 up to about 4 million numbers:

```bash
make bench CPU=-1 MAXLOG=22
```

- `CPU=-1` lets the operating system pick a processor core. Use this unless you
  know your computer's core layout (see "pinning" in the glossary).
- `MAXLOG=22` means the largest list has 2^22 ≈ 4 million numbers.

It takes a few minutes and writes **`results/timing_raw.csv`**.

> **Safety note: memory.** Small races like this one use under 1 GB. The big
> real-data races near the end of [EXPERIMENTS.md](EXPERIMENTS.md) can use many
> GB. On one laptop, an early version pushed memory to 100%. Before running
> anything large, read [PROGRAMS.md](PROGRAMS.md), keep Task Manager (or `top`
> on Linux) open, and close other heavy programs.

---

## 7. Read the result

Open `results/timing_raw.csv` in Excel, Google Sheets, or any text editor. Each
row is one measurement:

| column | meaning |
|---|---|
| `structure` | which search method |
| `n` | how many numbers were in the list |
| `rep` | which repetition (each is timed several times) |
| `ns_per_query` | **how long one search took, in nanoseconds. Smaller is faster.** |
| `checksum` | a fingerprint of all the answers. **It must be the same for every structure at the same `n`**, which proves they all answered the same questions |

To compare two methods, pick one `n`, take the **middle value** (median) of
each method's `ns_per_query`, and see which is smaller.

For charts, with R installed:

```bash
Rscript analysis/analyse.R results/timing_raw.csv tput
```

---

## 8. Where to go next

| I want to… | Read |
|---|---|
| understand the whole project, from zero to now | [Introduction.md](Introduction.md) |
| understand the words | [GLOSSARY.md](GLOSSARY.md) |
| understand each search method | [STRUCTURES.md](STRUCTURES.md) |
| see every question the project asked, and the answers | [EXPERIMENTS.md](EXPERIMENTS.md) |
| run a specific program or reproduce a result | [PROGRAMS.md](PROGRAMS.md) |
| find a file | [REPO_MAP.md](REPO_MAP.md) |
| fix an error | [TROUBLESHOOTING.md](TROUBLESHOOTING.md) |

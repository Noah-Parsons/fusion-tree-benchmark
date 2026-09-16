# Running the programs

Every program, what to type, how long it takes, how much memory it needs, and
what the output means.

On Windows, first set up PowerShell as in [START_HERE.md](START_HERE.md) step 5.
Commands are shown Linux-style (`./build/...`). On Windows, write
`build\...exe` instead.

---

## Before you run anything big: safety

- **Check the memory column in the tables below.** Close other heavy programs
  before a large race, and keep Task Manager (Windows) or `top` (Linux) open.
- **Prefer `bench_one` for large races.** It keeps only one method in memory
  at a time, and Windows enforces a hard memory cap on it.
- **Don't run anything else heavy at the same time.** That includes WSL jobs,
  which also slow the timing down.
- **If memory climbs past about 80%, stop the program:** press Ctrl+C, or end
  it in Task Manager.

---

## Quick reference

| What | Command | Time | Memory |
|---|---|---|---|
| All correctness tests | `make test` | few minutes | < 1 GB |
| Small race, made-up data | `make bench CPU=-1 MAXLOG=22` | few minutes | < 2 GB |
| Latency race | `make bench-lat CPU=-1 MAXLOG=22` | few minutes | < 2 GB |
| Experiment 1 (sketch width) | `make spread` | seconds | tiny |
| Experiment 3a (steps per box) | `make node-lat CPU=-1` | ~1 minute | tiny |
| Experiment 3b (memory traffic) | `make traffic MAXLOG=22` | minutes | < 2 GB |
| Changing-data test | see "bench_dyn" below | few minutes | < 1 GB |

**Build everything with clang instead of g++:**
`make test CXX=clang++ BUILD=build_clang`

---

## The build settings you can change

Add these to any `make` command:

| Setting | Default | Meaning |
|---|---|---|
| `CPU` | `8` | The core to run on. **Use `-1` unless you know your computer's core layout.** The default 8 is a fast core on the original laptop only. |
| `MAXLOG` | `22` | The largest list has 2^MAXLOG numbers. 25 = 33 million, and needs a lot more memory and time. |
| `REPS` | `15` | Repetitions per measurement. |
| `CXX` | `g++` | The compiler. |

---

## `bench_pred`: the main race program

```text
./build/bench_pred [reps] [max_log] [cpu] [tput|lat] [shuffled|blocked] [structures] [seed] [keys] [dump:path]
```

| Argument | Example | Meaning |
|---|---|---|
| reps | `15` | repetitions |
| max_log | `22` | largest size, as a power of 2 (always starts at 2^8) |
| cpu | `-1` | core to pin to, or -1 for none |
| mode | `tput` | `tput` = throughput, `lat` = latency |
| order | `shuffled` | `shuffled` = fresh random order each repetition (recommended) |
| structures | `sorted_array,btree8_bl,fusion8` | comma list, or `all`. Names are in [STRUCTURES.md](STRUCTURES.md) |
| seed | `20260909` | changes the random data |
| keys | `uniform` | `uniform`, `clustered`, `nested`, or `file:data/books_200M_uint64` |
| dump | `dump:data/samples/books_1` | also save the largest key set to a file |

**Example:** race three methods on clustered data up to 4 million numbers:

```bash
./build/bench_pred 7 22 -1 tput shuffled sorted_array,btree8_bl,clusterjump 1 clustered > my_race.csv
```

**Output columns:** `structure, n, rep, ns_per_query, checksum`.
Smaller `ns_per_query` is faster. Checksums must match at each `n`.

> **Memory warning.** bench_pred builds **every** listed method at once, at
> every size. With many methods at 2^25 on real data, that is many GB. For
> large races use `bench_one`.

---

## `bench_one`: the memory-safe race program (Windows)

Races **one size** (a saved 2^25-key sample), keeping **one method in memory
at a time**, under a **hard memory cap**.

```text
build\bench_one.exe <sample file> <tput|lat> <reps> <cpu> <structures> [query_seed] [min_free_gb] [memory_cap_gb]
```

- `reps` = `0` means **pre-check only:** build each method once and print
  whether it FITS under the cap, without timing anything.
- A method that needs more memory than the cap is printed as
  `OVER_MEMORY_CAP` and skipped. The computer is not affected.

**Example:**
```powershell
build\bench_one.exe data\samples\books_20260909 tput 3 -1 splus8,clusterjump,rs18_e16 20260913 6 8
```

**Output columns:** `structure, n, rep, ns_per_query, checksum, build_ms`.

The sample files are made by bench_pred's `dump:` option. See "Real datasets"
below.

---

## `bench_dyn`: the changing-data race

It needs the ALEX and TLX libraries (already in `third_party/`), and it must
be compiled as C++17:

```bash
g++ -O3 -std=c++17 -march=native -mbmi2 -Iinclude -Ithird_party/tlx -Ithird_party/ALEX/src/core -static bench/bench_dyn.cpp -o build/bench_dyn
./build/bench_dyn 5 -1 10 all 1 uniform 20
```

Arguments: `reps cpu insert_percent structures seed keys log_n`.
Output: `structure, n, write_pct, rep, ns_per_op, checksum, bytes_end`.
`log_n = 24` (17 million numbers) needs several GB; start with 20.

---

## Correctness tests

| Test | Build | What it checks |
|---|---|---|
| `test_node` | `make test` | the fusion box against a slow correct method |
| `test_structures` | `make test` | every static method against `std::lower_bound` |
| `test_dynamic` | `g++ -O3 -std=c++17 -march=native -mbmi2 -Iinclude -Ithird_party/tlx -Ithird_party/ALEX/src/core -static tests/test_dynamic.cpp -o build/test_dynamic` | methods that accept new numbers, against `std::set` |
| `test_rmi` | `make build/test_rmi` (needs generated RMIs) | every generated RMI |

Success means `failures=0` (or `PASS`).

---

## Real datasets (SOSD)

The real-data experiments need four files, which are **not** in the
repository because each is 1.6 GB. They come from the SOSD benchmark on Harvard
Dataverse (doi:10.7910/DVN/JGVF9A):

| File | Download (compressed) | What the numbers are |
|---|---|---|
| `wiki_ts_200M_uint64.zst` | 116 MB | Wikipedia edit times |
| `fb_200M_uint64.zst` | 314 MB | Facebook user IDs |
| `books_200M_uint64.zst` | 1.07 GB | Amazon book popularity |
| `osm_cellids_200M_uint64.zst` | 1.21 GB | OpenStreetMap locations |

Unpack each one with `zstd -d <file>.zst` and put the results in `data/`.

**Using a dataset:** pass `file:data/books_200M_uint64` as the `keys`
argument to `bench_pred`.

---

## Analysis scripts (R and Python)

| Script | What it does |
|---|---|
| `analysis/analyse.R <csv> <tag>` | charts and summary for a single race |
| `analysis/final.R <dir> <tag>` | the Part 1 campaign summary and figures |
| `analysis/confirm.R`, `learned_confirm.R`, `dynamic_confirm.R`, `sosd_confirm.R` | apply a plan's "who wins" rule to a confirmation race |
| `analysis/learned_select.R`, `sosd_select.R` | apply a plan's "which setting" rule to a tuning race |
| `analysis/rmi_build.sh`, `rmi_grid.py`, `rmi_registry.py` | generate RMI code (run in Linux or WSL) |

Each script's first lines explain exactly what it reads and writes.

---

## Generating RMIs (advanced)

RMI code must be generated for each key set by the reference RMI tool
(<https://github.com/learnedsystems/RMI>, written in Rust):

1. Build the tool: `cargo build --release` inside its folder (Linux or WSL).
2. Save the key set: run `bench_pred ... dump:data/samples/<dataset>_<seed>`.
3. Find the best configurations:
   `rmi <file>_uint64 --optimize data/rmi/optimize/opt_<dataset>.json`.
4. Generate the code: `bash analysis/rmi_build.sh <dataset> <seed> all`.
5. List the RMIs for the build: `python analysis/rmi_registry.py`.
6. Rebuild: `make build/bench_pred build/test_rmi`, then run `test_rmi` to
   check them.

**On Windows, run WSL steps from a `.sh` script file**, not by typing them into
PowerShell. PowerShell changes `$` signs and quotes before WSL sees them.

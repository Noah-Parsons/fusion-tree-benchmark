# Map of the repository

What every folder and important file is for.

---

## Top level

| Path | What it is |
|---|---|
| `README.md` | the project summary and headline results |
| `docs/` | **these guides**, the plain-language documentation (start with `THE_WHOLE_STORY.md` or `START_HERE.md`) |
| `Makefile` | build recipes (`make test`, `make bench`, …) |
| `LICENSE` | MIT: free to use, copy and change |
| `Experiments_Report_2026-09-10.docx` | the full write-up of Part 1 (the fusion tree study) |
| `Work_Log_2026-09-10.docx` | what was built in the first session, and why |
| `run_remaining_experiments.sh` | a script that reran several Part 1 experiments |

## `include/`: the search methods (C++ headers)

| File | Contains |
|---|---|
| `bits.hpp` | small bit tools shared by everything |
| `sketch_fast.hpp` | the fusion tree's sketch (PEXT, and the 1993 multiplication method) |
| `fusion_node.hpp`, `fusion_node_compact.hpp`, `fusion_node_wide.hpp` | the fusion tree's box, in three versions |
| `structures.hpp` | binary search, the B-trees, and the fusion tree |
| `splus_tree.hpp` | the S+ tree |
| `radix_jump.hpp` | RadixJump |
| `spline_index.hpp` | SplineIndex |
| `cluster_jump.hpp` | ClusterJump |
| `cluster_jump_dynamic.hpp` | ClusterJumpD (accepts new numbers) |
| `learned_indexes.hpp` | wrappers for RadixSpline and PGM-index |
| `sosd_rivals.hpp` | wrapper for CHT (Hist-Tree) |
| `rmi_index.hpp` | wrapper for generated RMIs |
| `dynamic_rivals.hpp` | wrappers for ALEX and the TLX B+ tree |

Each file starts with a comment explaining its method in plain terms.

## `tests/`: correctness checks

| File | Checks |
|---|---|
| `test_node.cpp` | the fusion box |
| `test_structures.cpp` | every static method |
| `test_dynamic.cpp` | methods that accept new numbers |
| `test_rmi.cpp` | generated RMIs |

## `bench/`: the programs that time things

| File | Measures |
|---|---|
| `bench_pred.cpp` | **the main race:** search time, all sizes |
| `bench_one.cpp` | the memory-safe race: one size, one method at a time, memory cap |
| `bench_dyn.cpp` | the changing-data race |
| `node_lat.cpp` | steps per box search, in processor cycles (Experiment 3a) |
| `traffic.cpp` | memory touched per search (Experiment 3b) |
| `spread.cpp` | sketch width (Experiment 1) |
| `index_bytes.cpp`, `index_bytes_samples.cpp` | memory used by each method |
| `rmi_none.cpp` | an empty RMI list, used when no RMIs are generated |

## `analysis/`: turning results into tables and charts

R, PowerShell, Python and shell scripts. See [PROGRAMS.md](PROGRAMS.md).
`final_campaign.ps1` and `sosd_stage*.ps1` run whole multi-session races.

## `results/`: every measurement

| Path | What it is |
|---|---|
| `machine.txt` | the laptop everything was measured on |
| `final_v3/` + `final_*_v3.csv` | **Part 1's main results** (use these) |
| `final/`, `final_v1_*`, `final_v2*` | **superseded.** Kept as a record of a compiler problem; don't use |
| `timing_*.csv`, `summary*.csv`, `ratios*.csv` | earlier single-session Part 1 races |
| `node_lat*.csv`, `traffic.csv`, `spread.csv`, `vtune_counters.csv` | Part 1, Experiments 1 and 3 |
| `factorial_*.csv` | run order × pinning check |
| `splus_pilot/`, `spline_pilot/`, `spline_confirm/` | Part 2 |
| `realdata/` | Part 3 |
| `learned/` | Part 4 |
| `nested/` | Part 5 |
| `dynamic/` | Part 6 |
| `RELATED_WORK.md` | Part 7: how ClusterJump compares with published work |
| `sosd_rivals/` | Part 8 (in progress) |
| `*_summary.csv` | the scored result of each race |

**Each experiment folder has a `PLAN.md`:** the rules written before timing,
then the outcome, added afterwards.

## `figures/`
Charts made by the R scripts.

## `third_party/`: other researchers' code

RadixSpline, PGM-index, ALEX, TLX and CHT, each with its own license.
`third_party/PATCHES.md` lists exactly where each came from, and every change
made (three one-line portability fixes, and no others).

---

## Not in the repository

These are ignored by git, so you won't see them after downloading:

| Path | Why |
|---|---|
| `build/`, `build_clang/`, `*.exe` | built programs; make your own with `make` |
| `data/` | the SOSD datasets (1.6 GB each), saved key samples, and generated RMI code. See [PROGRAMS.md](PROGRAMS.md) |
| `vt_*` | Intel VTune profiler output (large) |
| `Fusion_Tree_Project_Manual.md` | the original project manual, kept private |

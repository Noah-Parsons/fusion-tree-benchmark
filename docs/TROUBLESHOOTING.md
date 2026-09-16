# Troubleshooting

Find your problem below. Each entry says what you see, why it happens, and
what to do.

---

### "Cannot create temporary file in C:\Windows\" (Windows)
**Why:** the compiler needs a writable temporary folder, and Git Bash doesn't
give it one.
**Fix:** use **PowerShell**, not Git Bash, and set the folders first:
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
$env:TMP = "$PWD\build"; $env:TEMP = $env:TMP
```

### `make` or `g++` is "not recognized" (Windows)
**Why:** PowerShell doesn't know where MSYS2 installed them.
**Fix:** run the `$env:PATH = ...` line above in the same window. If it still
fails, check that MSYS2 is installed at `C:\msys64` and that you ran the
`pacman` step in [START_HERE.md](START_HERE.md).

### `cmd` behaves strangely inside scripts (Windows)
**Why:** with `C:\msys64\usr\bin` first on PATH, the name `cmd` finds an MSYS
script instead of Windows' command prompt.
**Fix:** in PowerShell scripts, use `$env:ComSpec` instead of `cmd`.

### "Illegal instruction", or the program closes immediately
**Why:** your processor lacks AVX2 or BMI2.
**Fix:** you need an Intel Haswell (2013) or newer, or an AMD Ryzen, processor.
Apple Silicon and ARM computers are not supported.

### "warning: could not pin to cpu 8"
**Why:** your computer doesn't have a core number 8, or pinning isn't allowed.
**Fix:** use `CPU=-1` (in `make`) or `-1` (as the cpu argument). The results
are still valid, just slightly noisier.

### Linking fails on Linux with messages about `-static`
**Why:** some Linux systems don't include the files needed to build fully
self-contained programs.
**Fix:** build without `-static`, for example:
```bash
make test CXXFLAGS="-O3 -std=c++20 -march=native -mbmi2 -DFT_USE_PEXT -Iinclude -Ithird_party/RadixSpline/include -Ithird_party/PGM-index/include -Ithird_party/CHT/include -Wall -Wextra"
```

### ALEX errors mentioning `rebind`
**Why:** ALEX uses an old C++ feature that C++20 removed.
**Fix:** compile programs that use ALEX with `-std=c++17` (see
[PROGRAMS.md](PROGRAMS.md)).

### R says "there is no package called 'ggplot2'"
**Fix:** open R and run `install.packages("ggplot2")`.

### "running scripts is disabled on this system" (PowerShell)
**Fix:** run the script like this, which allows it just for that one run:
```powershell
powershell -ExecutionPolicy Bypass -File analysis\final_campaign.ps1
```

### Memory climbs very high during a race
**Why:** `bench_pred` keeps every method in memory at once, at every size.
Some CHT settings can need many GB just to build.
**Fix:** stop the race (Ctrl+C, or end it in Task Manager). Then:
- race fewer methods at a time, or use a smaller `MAXLOG`;
- for big races, use `bench_one`, which has a hard memory cap;
- close other heavy programs, and shut down WSL (`wsl --shutdown`).

### The computer crashed (blue screen) during a race
**What happened on the original laptop:** heavy memory load exposed a bug in
the NVIDIA graphics driver (`nvlddmkm.sys`). The benchmark doesn't use the
graphics card, but updating the driver fixed it.
**Fix:**
- update your graphics and chipset drivers;
- run Windows Memory Diagnostic to check your memory chips;
- use `bench_one` for big races.

### Checksums differ between methods
**Why:** at least one method gave a wrong answer. **This is a real bug. Don't
use those timings.**
**Fix:** run the correctness tests (`make test`) to find which method is
wrong, and open an issue on GitHub.

### RMI methods are much slower than expected, or look like binary search
**Why:** no generated RMI matched the keys, so the wrapper fell back to a plain
search. RMIs only work on the exact key set they were generated for, and only
at that size.
**Fix:** generate RMIs for those keys (see [PROGRAMS.md](PROGRAMS.md)), then run
`test_rmi` to confirm they are found.

### WSL commands typed into PowerShell fail with odd syntax errors
**Why:** PowerShell rewrites `$` signs and quotes before WSL receives them.
**Fix:** put the commands in a `.sh` file and run
`wsl -d Ubuntu -e bash /mnt/c/path/to/script.sh`.

### A timing race gives very different numbers from the README
**Why:** a different computer, other programs running, battery power, heat, or
a different compiler.
**Fix:**
- close other programs and plug in the charger;
- pin to a fast core if you can;
- compare **ratios between methods in the same run**, not absolute
  nanoseconds. Ratios are far more stable.

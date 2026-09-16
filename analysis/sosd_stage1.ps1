# sosd_stage1.ps1 — Stage 1 (tuning) of results/sosd_rivals/PLAN.md, with bench_one.
#
# Two earlier attempts used bench_pred (all raced structures in memory at once);
# the first crashed the laptop and the second pushed memory to 100%. A third,
# with bench_one in one process per dataset and mode, ended with
# STATUS_STACK_OVERFLOW on wiki when a CHT build reached the memory cap and
# the thread could not grow its stack. So this version:
#
#   1. PRE-FLIGHT, once per dataset: builds each CHT setting alone, in its own
#      process under the 8 GB cap, timing nothing. Settings that do not fit
#      (or whose process dies) are excluded from that dataset's timed runs and
#      logged.
#   2. TIMED RUNS: one process per dataset and mode with the structures that
#      fit (one in memory at a time, fresh random order per repetition).
#
# Throughout, a watchdog stops the run if free RAM falls below 6 GB.
# Datasets already completed (a CSV with the expected number of rows) are
# skipped, so the script can be rerun after a stop.
#
# Run from PowerShell in the project root, with WSL shut down (wsl --shutdown):
#   powershell -ExecutionPolicy Bypass -File analysis/sosd_stage1.ps1
param([int]$Reps = 7, [int]$PauseSeconds = 20, [double]$CapGB = 8, [int]$WatchdogFreeMB = 6000)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$out = 'results\sosd_rivals'
New-Item -ItemType Directory -Force $out | Out-Null
$log = "$out\campaign.log"
$avail = New-Object System.Diagnostics.PerformanceCounter('Memory', 'Available MBytes')
$base = @('splus8', 'splus16', 'clusterjump')
$chts = @('cht64_e16', 'cht64_e32', 'cht64_e64', 'cht64_e128', 'cht256_e16', 'cht256_e32', 'cht256_e64', 'cht256_e128',
          'cht1024_e16', 'cht1024_e32', 'cht1024_e64', 'cht1024_e128')
$rmis = 1..10 | ForEach-Object { "rmi_r$_" }
$sets = @('books', 'wiki', 'fb', 'osm')

function Log($msg) { "$(Get-Date -Format o)  $msg" | Out-File -Append -Encoding ascii $log }

# Run bench_one under the watchdog. Returns @{exit; killed; peak; minfree; secs}.
function Invoke-Watched($argLine, $stdout, $stderr) {
    $p = Start-Process -FilePath 'build\bench_one.exe' -NoNewWindow -PassThru -ArgumentList $argLine `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $null = $p.Handle
    $minFree = 1e9; $peak = 0; $killed = $false; $t0 = Get-Date
    while (-not $p.HasExited) {
        $f = $avail.NextValue(); if ($f -lt $minFree) { $minFree = $f }
        try { $p.Refresh(); if ($p.PeakWorkingSet64 -gt $peak) { $peak = $p.PeakWorkingSet64 } } catch {}
        if ($f -lt $WatchdogFreeMB) { Stop-Process -Id $p.Id -Force; $killed = $true; break }
        Start-Sleep -Milliseconds 250
    }
    $p.WaitForExit()
    return @{ exit = $p.ExitCode; killed = $killed; peak = [math]::Round($peak / 1GB, 2);
              minfree = [math]::Round($minFree / 1024, 1); secs = [math]::Round(((Get-Date) - $t0).TotalSeconds) }
}

Log "stage 1 (bench_one + preflight) start seed=20260909 reps=$Reps cap_gb=$CapGB watchdog_free_mb=$WatchdogFreeMB"
foreach ($k in $sets) {
    $sample = "data\samples\${k}_20260909"
    # Expected rows per timed CSV if every structure fits: header + 25 * reps.
    $done = @('tput', 'lat') | Where-Object {
        $f = "$out\tune_$($_)_${k}_g1.csv"; (Test-Path $f) -and ((Get-Content $f | Measure-Object -Line).Lines -gt 1)
    }
    if ($done.Count -eq 2 -and $k -eq 'books') { Log "  $k already complete (both modes), skipped"; continue }

    # 1. Pre-flight.
    $fits = @()
    foreach ($c in $chts) {
        $pf = "$out\preflight_${k}_$c.txt"
        $r = Invoke-Watched "$sample tput 0 8 $c 20260913 6 $CapGB" $pf "$pf.err"
        $line = if (Test-Path $pf) { (Get-Content $pf | Select-Object -Last 1) } else { '' }
        $ok = ($r.exit -eq 0) -and (-not $r.killed) -and ($line -match ',FITS,')
        Log "  preflight $k $c exit=$($r.exit) killed=$($r.killed) peak_GB=$($r.peak) lowest_free_GB=$($r.minfree) result=$(if ($ok) { 'FITS' } else { "EXCLUDED ($line)" })"
        if ($r.killed) { Log "STOP (watchdog)"; exit 1 }
        if ($ok) { $fits += $c }
        Start-Sleep -Seconds 3
    }
    $list = ($base + $fits + $rmis) -join ','

    # 2. Timed runs.
    foreach ($m in @('tput', 'lat')) {
        $csv = "$out\tune_${m}_${k}_g1.csv"
        $r = Invoke-Watched "$sample $m $Reps 8 $list 20260913 6 $CapGB" $csv "$out\tune_${m}_${k}_g1.err"
        $over = (Select-String -Path "$out\tune_${m}_${k}_g1.err" -Pattern 'OVER_MEMORY_CAP' -ErrorAction SilentlyContinue | Measure-Object).Count
        Log "  ran ${m}_${k} structures=$(($base + $fits + $rmis).Count) exit=$($r.exit) killed=$($r.killed) seconds=$($r.secs) peak_GB=$($r.peak) lowest_free_GB=$($r.minfree) over_cap_events=$over"
        if ($r.killed -or $r.exit -ne 0) {
            $last = Select-String -Path "$out\tune_${m}_${k}_g1.err" -Pattern '^START' | Select-Object -Last 1
            Log "STOP (last structure started: $($last.Line))"
            exit 1
        }
        Start-Sleep -Seconds $PauseSeconds
    }
}
Log "stage 1 done"

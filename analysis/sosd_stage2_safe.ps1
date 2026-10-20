# sosd_stage2_safe.ps1 — Stage 2 (confirmation) of results/sosd_rivals/PLAN.md, in the bench_one
# harness that Stage 1 used, instead of bench_pred (all structures in memory at once, full 200M-key
# files), which crashed the laptop once and pushed memory to 100% once.
#
# Same measurement as Stage 1: one structure in memory at a time, fresh random order per
# repetition, an 8 GB job-object cap per process, and a watchdog that stops everything if free RAM
# falls below 6 GB. Racers: the five fixed structures plus every setting chosen by
# analysis/sosd_select.R (both modes' choices, raced in both modes). Seeds 20260910-12, the
# pre-dumped 2^25-key samples data/samples/<dataset>_<seed>, 15 repetitions.
#
# Before running: RMIs for the chosen ranks trained on each seed (analysis/rmi_build.sh, inside
# WSL), python analysis/rmi_registry.py, bench_one rebuilt, and WSL shut down (wsl --shutdown).
# Completed runs (CSV present with data) are skipped, so the script can be rerun after a stop.
#
#   powershell -ExecutionPolicy Bypass -File analysis/sosd_stage2_safe.ps1
param([string]$Chosen = 'cht256_e16,cht64_e16,rmi_r10,rmi_r7', [int]$Reps = 15,
      [int]$PauseSeconds = 20, [double]$CapGB = 8, [int]$WatchdogFreeMB = 6000)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$out = 'results\sosd_rivals'
$log = "$out\campaign.log"
$avail = New-Object System.Diagnostics.PerformanceCounter('Memory', 'Available MBytes')
$list = "splus8,splus16,clusterjump,rs18_e16,pgm16,$Chosen"
$chts = $Chosen.Split(',') | Where-Object { $_ -like 'cht*' }

function Log($msg) { "$(Get-Date -Format o)  $msg" | Out-File -Append -Encoding ascii $log }

function Invoke-Watched($argLine, $stdout, $stderr) {
    $p = Start-Process -FilePath 'build\bench_one.exe' -NoNewWindow -PassThru -ArgumentList $argLine `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $null = $p.Handle
    $p.PriorityClass = 'BelowNormal'
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

Log "stage 2 (bench_one, safe) start reps=$Reps structures=$list cap_gb=$CapGB watchdog_free_mb=$WatchdogFreeMB"
foreach ($i in 1..3) {
    $seed = 20260909 + $i
    foreach ($k in @('books', 'wiki', 'fb', 'osm')) {
        $sample = "data\samples\${k}_$seed"
        # Pre-flight: each chosen CHT setting built alone under the cap, timing nothing.
        $use = @('splus8', 'splus16', 'clusterjump', 'rs18_e16', 'pgm16') + ($Chosen.Split(',') | Where-Object { $_ -notlike 'cht*' })
        foreach ($c in $chts) {
            $pf = "$out\preflight2_${k}_${seed}_$c.txt"
            $r = Invoke-Watched "$sample tput 0 8 $c 20260913 6 $CapGB" $pf "$pf.err"
            $line = if (Test-Path $pf) { (Get-Content $pf | Select-Object -Last 1) } else { '' }
            $ok = ($r.exit -eq 0) -and (-not $r.killed) -and ($line -match ',FITS,')
            Log "  preflight $k s$i $c exit=$($r.exit) killed=$($r.killed) peak_GB=$($r.peak) lowest_free_GB=$($r.minfree) result=$(if ($ok) { 'FITS' } else { "EXCLUDED ($line)" })"
            if ($r.killed) { Log "STOP (watchdog)"; exit 1 }
            if ($ok) { $use += $c }
            Start-Sleep -Seconds 3
        }
        $runList = $use -join ','
        foreach ($m in @('tput', 'lat')) {
            $csv = "$out\conf_${m}_${k}_s$i.csv"
            if ((Test-Path $csv) -and ((Get-Content $csv | Measure-Object -Line).Lines -gt 1)) { Log "  conf_${m}_${k}_s$i exists, skipped"; continue }
            $r = Invoke-Watched "$sample $m $Reps 8 $runList 20260913 6 $CapGB" $csv "$out\conf_${m}_${k}_s$i.err"
            Log "  ran conf_${m}_${k}_s$i structures=$($use.Count) exit=$($r.exit) killed=$($r.killed) seconds=$($r.secs) peak_GB=$($r.peak) lowest_free_GB=$($r.minfree)"
            if ($r.killed -or $r.exit -ne 0) { Log "STOP"; exit 1 }
            Start-Sleep -Seconds $PauseSeconds
        }
    }
}
Log "stage 2 done"

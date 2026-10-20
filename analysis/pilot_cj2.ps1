# pilot_cj2.ps1 - pilot of results/best/DESIGN_cj2.md: tuning seed 20260909 only, same safe harness as
# sosd_stage2_safe.ps1 (one structure at a time, 8 GB cap, watchdog at 6 GB free), 7 repetitions.
#   powershell -ExecutionPolicy Bypass -File analysis/pilot_cj2.ps1
param([string]$Chosen = 'cht256_e16,cht64_e16,rmi_r10,rmi_r7', [int]$Reps = 7,
      [int]$PauseSeconds = 20, [double]$CapGB = 8, [int]$WatchdogFreeMB = 6000)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$out = 'results\best'
$log = "$out\campaign.log"
$avail = New-Object System.Diagnostics.PerformanceCounter('Memory', 'Available MBytes')
$list = "clusterjump,cj2_k2w8,cj2_k3w16,cj2_k4w16,splus8,rs18_e16,pgm16,$Chosen"
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

Log "pilot cj2 (bench_one, safe, seed 20260909) start reps=$Reps structures=$list cap_gb=$CapGB watchdog_free_mb=$WatchdogFreeMB"
foreach ($i in 0..0) {
    $seed = 20260909 + $i
    foreach ($k in @('books', 'wiki', 'fb', 'osm')) {
        $sample = "data\samples\${k}_$seed"
        # Pre-flight: each chosen CHT setting built alone under the cap, timing nothing.
        $use = @('clusterjump', 'cj2_k2w8', 'cj2_k3w16', 'cj2_k4w16', 'splus8', 'rs18_e16', 'pgm16') + ($Chosen.Split(',') | Where-Object { $_ -notlike 'cht*' })
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
            $csv = "$out\pilot_${m}_${k}.csv"
            if ((Test-Path $csv) -and ((Get-Content $csv | Measure-Object -Line).Lines -gt 1)) { Log "  pilot_${m}_${k} exists, skipped"; continue }
            $r = Invoke-Watched "$sample $m $Reps 8 $runList 20260913 6 $CapGB" $csv "$out\pilot_${m}_${k}.err"
            Log "  ran pilot_${m}_${k} structures=$($use.Count) exit=$($r.exit) killed=$($r.killed) seconds=$($r.secs) peak_GB=$($r.peak) lowest_free_GB=$($r.minfree)"
            if ($r.killed -or $r.exit -ne 0) { Log "STOP"; exit 1 }
            Start-Sleep -Seconds $PauseSeconds
        }
    }
}
Log "pilot cj2 done"

# final_campaign.ps1 — the final timing campaign (Experiments 2, 3 and 4).
#
# Three sessions. Each session runs every configuration once, pinned to a
# performance core, with the order of configurations rotated between
# sessions; sessions are separated by a pause. Each session draws its own
# key sets and queries (seed 20260909 + session - 1), because a single key
# set was found to carry effects of its own (the btree8_bl "dip" at
# n = 512; see Experiments_Report_2026-09-10.docx). Session 1 reproduces the seed of every
# earlier campaign. Output goes to
#   results/final/<compiler>_<mode>_<order>_s<session>.csv
# and a log with timestamps and the power state to results/final/campaign.log.
#
# Run from PowerShell in the project root:
#   powershell -ExecutionPolicy Bypass -File analysis/final_campaign.ps1
param(
    [string]$OutDir = 'results\final_v2',
    [int]$Sessions = 3,
    [int]$Reps = 15,
    [int]$MaxLog = 25,
    [int]$Cpu = 8,
    [int]$PauseSeconds = 180
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
if (-not $env:TMP -or -not (Test-Path $env:TMP)) { $env:TMP = Join-Path $root 'build'; $env:TEMP = $env:TMP }
$cmd = $env:ComSpec
New-Item -ItemType Directory -Force $OutDir | Out-Null
$log = Join-Path $root "$OutDir\campaign.log"

function Log($msg) { "$(Get-Date -Format o)  $msg" | Out-File -Append -Encoding ascii $log }

Log "build"
& $cmd /c "make build/bench_pred 2>&1" | Out-File -Append -Encoding ascii $log
& $cmd /c "make build_clang/bench_pred CXX=clang++ BUILD=build_clang 2>&1" | Out-File -Append -Encoding ascii $log

$configs = @(
    @('gcc',   'build\bench_pred.exe',       'tput', 'shuffled'),
    @('gcc',   'build\bench_pred.exe',       'lat',  'shuffled'),
    @('gcc',   'build\bench_pred.exe',       'tput', 'blocked'),
    @('gcc',   'build\bench_pred.exe',       'lat',  'blocked'),
    @('clang', 'build_clang\bench_pred.exe', 'tput', 'shuffled'),
    @('clang', 'build_clang\bench_pred.exe', 'lat',  'shuffled')
)

for ($s = 1; $s -le $Sessions; $s++) {
    $bat = Get-CimInstance Win32_Battery
    $seed = 20260909 + $s - 1
    Log "session $s start; seed=$seed; BatteryStatus=$($bat.BatteryStatus) (2 = on AC) charge=$($bat.EstimatedChargeRemaining)%"
    for ($i = 0; $i -lt $configs.Count; $i++) {
        $c = $configs[($i + $s - 1) % $configs.Count]
        $out = "$OutDir\$($c[0])_$($c[2])_$($c[3])_s$s.csv"
        Log "  run $out"
        & $cmd /c "$($c[1]) $Reps $MaxLog $Cpu $($c[2]) $($c[3]) all $seed > $out 2>> $log"
    }
    Log "session $s done"
    if ($s -lt $Sessions) { Start-Sleep -Seconds $PauseSeconds }
}
Log "campaign done"

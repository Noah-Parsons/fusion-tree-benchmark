# sosd_stage2.ps1 — Stage 2 (confirmation) of results/sosd_rivals/PLAN.md.
#
#   powershell -ExecutionPolicy Bypass -File analysis/sosd_stage2.ps1 -Chosen "cht256_e32,rmi_r7"
#
# -Chosen lists the settings picked by analysis/sosd_select.R (for both modes).
# RMIs for seeds 20260910-12 must be generated for the chosen ranks first
# (analysis/rmi_build.sh <dataset> <seed> <ranks>, then rmi_registry.py) and
# bench_pred rebuilt.
param(
    [Parameter(Mandatory = $true)][string]$Chosen,
    [int]$Reps = 15,
    [int]$PauseSeconds = 60
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$out = 'results\sosd_rivals'
$log = "$out\campaign.log"
$cmd = $env:ComSpec
$s = "splus8,splus16,clusterjump,rs18_e16,pgm16,$Chosen"
$sets = [ordered]@{ books = 'books_200M_uint64'; wiki = 'wiki_ts_200M_uint64'; fb = 'fb_200M_uint64'; osm = 'osm_cellids_200M_uint64' }
for ($i = 1; $i -le 3; $i++) {
    $seed = 20260909 + $i
    "$(Get-Date -Format o) stage 2 session $i start seed=$seed structures=$s" | Out-File -Append -Encoding ascii $log
    foreach ($k in $sets.Keys) {
        foreach ($m in @('tput', 'lat')) {
            "$(Get-Date -Format o)   run ${m}_${k}" | Out-File -Append -Encoding ascii $log
            & $cmd /c "build\bench_pred.exe $Reps 25 8 $m shuffled $s $seed file:data\$($sets[$k]) > $out\${m}_${k}_s$i.csv 2>> $log"
        }
    }
    "$(Get-Date -Format o) stage 2 session $i done" | Out-File -Append -Encoding ascii $log
    if ($i -lt 3) { Start-Sleep -Seconds $PauseSeconds }
}
"$(Get-Date -Format o) stage 2 done" | Out-File -Append -Encoding ascii $log

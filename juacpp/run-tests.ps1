# Runs every assertion-based test in ./test and reports a summary.
# Each test is self-checking: it exits 0 on success, non-zero on failure.
# NOTE: keep this file ASCII-only; Windows PowerShell 5.1 reads BOM-less files as ANSI.
param([string]$Exe = '../build/jua.exe')
$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$exePath = Join-Path $root $Exe
if (-not (Test-Path $exePath)) { throw "missing $exePath - run ./build.ps1 first" }
$exePath = (Resolve-Path $exePath).Path

Push-Location (Join-Path $root 'test')
try {
    $files = Get-ChildItem -Filter *.jua |
        Where-Object { $_.Name -ne 'harness.jua' } |
        Sort-Object Name
    if ($files.Count -eq 0) { throw 'no test files found' }

    $pass = 0
    $failed = @()
    foreach ($f in $files) {
        # native stderr must not abort the loop, so relax EAP just for the call
        $old = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        $out = & $exePath $f.BaseName 2>&1
        $code = $LASTEXITCODE
        $ErrorActionPreference = $old

        if ($code -eq 0) {
            $pass++
            '{0,-16} ok' -f $f.BaseName
        } else {
            $failed += $f.BaseName
            '{0,-16} FAIL (exit {1})' -f $f.BaseName, $code
            $out | ForEach-Object { '    ' + $_ }
        }
    }

    ''
    '{0} passed, {1} failed, {2} total' -f $pass, $failed.Count, $files.Count
    if ($failed.Count -gt 0) {
        'failed: ' + ($failed -join ', ')
        exit 1
    }
} finally {
    Pop-Location
}

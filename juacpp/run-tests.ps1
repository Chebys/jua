# Runs every assertion-based test in ./test and reports a summary.
#
# Two kinds of cases:
#   1. normal    - the script self-checks and exits 0; non-zero means failure
#   2. expected  - the first line is   // expect-error: <substring>
#                  the script MUST fail, and stderr/stdout must contain <substring>
#                  (used for syntax errors, which cannot be caught by try{} because
#                   parsing happens before execution)
#
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
        $firstLine = Get-Content -LiteralPath $f.FullName -TotalCount 1
        $expectError = $null
        if ($firstLine -match '^//\s*expect-error:\s*(.+)$') { $expectError = $Matches[1].Trim() }

        # native stderr must not abort the loop, so relax EAP just for the call
        $old = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        $out = & $exePath $f.BaseName 2>&1
        $code = $LASTEXITCODE
        $ErrorActionPreference = $old
        $text = ($out | Out-String)

        $reason = $null
        if ($expectError) {
            if ($code -eq 0) {
                $reason = "expected a failure containing '$expectError', but it succeeded"
            } elseif ($text -notmatch [regex]::Escape($expectError)) {
                $reason = "expected message containing '$expectError'"
            }
        } elseif ($code -ne 0) {
            $reason = "exit $code"
        }

        if ($reason) {
            $failed += $f.BaseName
            '{0,-16} FAIL ({1})' -f $f.BaseName, $reason
            $out | ForEach-Object { '    ' + $_ }
        } else {
            $pass++
            if ($expectError) { '{0,-16} ok (expected error)' -f $f.BaseName }
            else              { '{0,-16} ok' -f $f.BaseName }
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

# Smoke test for the REPL.
#
# The REPL is not covered by run-tests.ps1, which only runs module files.
# Each case feeds a small script through stdin and checks that the output
# contains the expected text. The REPL must decide by itself when a statement
# is complete, so no line ends with ';'.
#
# NOTE: keep this file ASCII-only; Windows PowerShell 5.1 reads BOM-less files as ANSI.
$ErrorActionPreference = 'Stop'

$exe = Join-Path $PSScriptRoot '../build/jua.exe'
if (-not (Test-Path $exe)) { throw 'missing ../build/jua.exe - run ./build.ps1 first' }
$exe = (Resolve-Path $exe).Path

$cases = @(
    @{ name = 'complete statement runs without a semicolon'
       input = @('print(1 + 2)')
       want = '3' },
    @{ name = 'object literal spans lines'
       input = @('let o = {', '    a = 1,', '    b = 2', '}', 'print(o.a + o.b)')
       want = '3' },
    @{ name = 'expression continues on the next line'
       input = @('let x = 1 +', '2', 'print(x)')
       want = '3' },
    @{ name = 'block statement spans lines'
       input = @('let n = 3', 'if(n > 2){', "    print('big')", '}')
       want = 'big' },
    @{ name = 'syntax error keeps the session alive'
       input = @('let = 3', "print('alive')")
       want = 'alive' },
    @{ name = 'variables survive across statements'
       input = @('let v = 40', 'v += 2', 'print(v)')
       want = '42' }
)

$failed = 0
foreach ($c in $cases) {
    # native stderr (the expected syntax error) must not abort the loop
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $out = ($c.input | & $exe 2>&1 | Out-String)
    $ErrorActionPreference = $old
    if ($out -like "*$($c.want)*") {
        'ok    ' + $c.name
    } else {
        $failed++
        'FAIL  ' + $c.name + " (expected '$($c.want)')"
        $out -split "`r?`n" | ForEach-Object { '      ' + $_ }
    }
}

''
if ($failed -gt 0) {
    "$failed failed"
    exit 1
}
"$($cases.Count) passed"

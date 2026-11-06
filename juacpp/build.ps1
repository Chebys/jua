# Build script for the Jua C++ implementation.
# Usage:
#   pwsh ./build.ps1         -> ../build/jua.exe    (CLI host: run one module, exit code 0/1)
#   pwsh ./build.ps1 -Repl   -> ../build/repl.exe   (REPL host)
# NOTE: keep this file ASCII-only; Windows PowerShell 5.1 reads BOM-less files as ANSI.
param([switch]$Repl)
$ErrorActionPreference = 'Stop'

$sources = @(
    'debug.cpp', 'value.cpp', 'gc.cpp', 'parser.cpp',
    'program.cpp', 'vm.cpp', 'm-math.cpp', 'm-json.cpp'
)
$target = '../build/jua.exe'
if ($Repl) { $sources += 'main.cpp'; $target = '../build/repl.exe' }
else       { $sources += 'test.cpp' }

New-Item -ItemType Directory -Force -Path '../build' | Out-Null
& g++ -std=c++20 -g -I./include @sources -o $target
if ($LASTEXITCODE -ne 0) { throw 'build failed' }
"built $target"

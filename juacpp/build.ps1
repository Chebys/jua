# Build script for the Jua C++ implementation.
# Produces ../build/jua.exe - a single host:
#   jua <module>   run one module (exit code 0/1/2)
#   jua            REPL
# NOTE: keep this file ASCII-only; Windows PowerShell 5.1 reads BOM-less files as ANSI.
$ErrorActionPreference = 'Stop'

$sources = @(
    'debug.cpp', 'value.cpp', 'gc.cpp', 'parser.cpp',
    'program.cpp', 'vm.cpp', 'm-math.cpp', 'm-json.cpp', 'main.cpp'
)
$target = '../build/jua.exe'

New-Item -ItemType Directory -Force -Path '../build' | Out-Null
& g++ -std=c++20 -g -I./include @sources -o $target
if ($LASTEXITCODE -ne 0) { throw 'build failed' }
"built $target"

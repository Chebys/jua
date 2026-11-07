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
# g++ 把警告写在 stderr 上；PowerShell 会把它变成错误记录，
# 在 $ErrorActionPreference='Stop' 下会直接终止脚本。这里临时放宽，只看退出码。
# -Wall -Wextra 常开：默认警告级别下漏掉过未初始化指针（见 改进清单 附C #16）。
$old = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& g++ -std=c++20 -g -Wall -Wextra -I./include @sources -o $target
$code = $LASTEXITCODE
$ErrorActionPreference = $old
if ($code -ne 0) { throw 'build failed' }
"built $target"

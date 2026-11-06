# Jua
Jua 是一门脚本语言。其名称的寓意是“长得像 JS 的 Lua 语言”。设计时主要参考了JS 的简洁语法和 Lua 的精简内核。

语言定位：通用，学起来快，写起来也快。<!--运行起来就不快了-->

## 了解 Jua
* [在线体验](https://x-ze.cn/html/jua) Jua 的语法
* 查阅[Jua 草案](draft/jua草案.md)以了解 Jua 的准确规定
* 看 [demo](demo) 里的脚本快速感受语法

## 使用 Jua
Jua 的官方实现是 [juacpp](juacpp)，目前可构建、可运行，但语言特性仍在补齐中。也许你需要自己实现它。

## 仓库结构

| 目录 | 角色 | 说明 |
|---|---|---|
| [draft](draft) | 规范 | 语言的准确规定。实现之间的分歧以它为准 |
| [juacpp](juacpp) | **官方实现（生产）** | C++20。唯一需要保证正确性与性能的实现 |
| [juacpp/test](juacpp/test) | 工程测试 | 断言式，自校验，失败即非零退出。**改实现前先跑它** |
| [demo](demo) | 演示脚本 | 给人看的语法示例。输出不保证稳定，**不参与测试** |
| [juajs](juajs) | **演示（非生产）** | JS 版原型，用于快速试语法、做在线体验。不保证与规范同步，也不作为正确性参照 |

> `juajs` 不需要补运行入口、不要求通过测试、它与 `juacpp` 的行为差异通常不算 bug。
> 但两侧分歧仍是发现**草案本身**歧义的好线索——分歧时应修订草案，而不是互相迁就。

## 构建与运行

需要 g++ 13+（用到 `<format>`）。

```powershell
cd juacpp
pwsh ./build.ps1          # -> ../build/jua.exe，只有一个宿主
pwsh ./run-tests.ps1      # 跑全部工程测试，打印 PASS/FAIL 汇总
```

`jua.exe` 一个二进制干两件事：

```powershell
../build/jua.exe           # 无参数：REPL（分号结束一条输入）
../build/jua.exe <module>  # 跑一个模块，退出码 0 = 成功 / 1 = 失败 / 2 = 用法错误
```

运行演示脚本（模块名不带 `.jua`，工作目录决定模块搜索路径）：

```powershell
cd demo
../build/jua.exe class
```

## 测试怎么写的

测试是**断言式**而非 golden 输出：用例自己判断对错，失败抛错，由 CLI 宿主转成非零退出码。这样输出格式变化不会造成假失败。

```jua
let {eq, ok, throws} = require 'harness'

eq(1 + 2, 3, 'add')
ok('a' in {a = 1}, 'membership')
throws(fun(){ null.missing }, 'missing property')
```

断言库是 [juacpp/test/harness.jua](juacpp/test/harness.jua)，纯 Jua 实现，不依赖宿主。

语法错误没法用 `try{}` 捕获（解析在求值之前），所以用文件首行的标记声明「本用例应当失败」：

```jua
// expect-error: Trailing block call requires a parameter list
let f = fun(){ return 1 }
f(){ }
```

`run-tests.ps1` 要求这类用例**必须非零退出**，且输出里包含标记的文本。

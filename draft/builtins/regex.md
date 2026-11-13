# regex 模块设计（草案）

> 参考 JS 的 `RegExp`，但贴合 Jua 的字节串语义与现有迭代/元方法机制。
> 本文件是设计稿；定稿后内容并入 `builtins/modules.md` 的 `## regex` 章节，本文件可删除。

## 1. 定位与约束

### 为什么单独成文
`modules.md` 里 `regex` 只有「是一个可构造类」和空标题 `regex.__match(self, str)`。整体未实现，需要先把语义定清楚再实现。

### 硬约束（来自现有规范，不可违背）
1. **字符串是字节串**（[值与类型](../值与类型.md#字符串)、[杂项·字节串](../杂项.md#字节串)）。regex 的匹配单位是**字节**，不是 Unicode 码点。
2. **迭代协议**（[抽象操作·迭代](../抽象操作.md#迭代)）：有 `next` 元方法的就是迭代器；`next(target, key)` 返回 `{done, key?, value}`。
3. **类是原型上带 `__class=true` 的对象**（[杂项·类与实例](../杂项.md#类与实例)）。`regex` 是可构造类，构造调用返回实例。

### 与 JS 的关键差异
| 维度 | JS RegExp | Jua regex |
|---|---|---|
| 匹配单位 | UTF-16 码元 | **字节**（`.` 匹配单个字节，0x00–0xFF） |
| 字符类 `\d \w \s` | Unicode 感知（`u` 标志下） | **ASCII only**：`\d`=`[0-9]`，`\w`=`[0-9A-Za-z_]`，`\s`=`[ \t\n\r\f\v]` |
| 字面量语法 | `/pattern/flags` | **不提供**。统一用 `regex(pattern, flags)` 构造 |
| `u` 标志 | Unicode 模式 | **不提供**。字节语义下无意义 |
| 后顾 | `(?<=...)` / `(?<!...)` | **不提供** |

> 字节语义的影响：对 UTF-8 多字节字符，`.` 只匹配一个字节，`\w` 不匹配非 ASCII 字母。需要「按字符」匹配时，用户应先用宿主/模块把字符串解码成码点数组再处理——这是字节串语言的固有取舍，与 `String.byte`/`String.slice` 一致。

## 2. 构造

`require 'regex'` 直接返回构造函数（可调用对象），无需解构：

```
let re = require 'regex'
```

### re(pattern, flags='')
* `pattern`：字符串，正则模式体。
* `flags`：字符串，标志组合。无参或空串为无标志。
* **返回**：regex 实例。
* 模式或标志非法时报错（`JuaSyntaxError`，`kind=Syntax`）。

利用 Jua 的尾随字符串实参语法，无标志构造可简写：

```
re(`\d+`)     // 等价于 re`\d+`
re`\d+`       // 尾随反引号字符串，最简写法
re(`\d+`, 'g')  // 带标志时仍需括号
```

实例的**自身属性**（构造时设置，可被用户读取但不建议改写）：
| 属性 | 类型 | 说明 |
|---|---|---|
| `source` | string | 原始模式串 |
| `flags` | string | 标志串（按 `gimsy` 规范化排序） |
| `lastIndex` | number | 下次匹配起始字节下标，初值 0。仅 `g`/`y` 下被 `exec`/`__match` 推进；无 `g`/`y` 时恒为 0 |

实例的**内部状态**（非属性，用 C++ 成员承载，不暴露）：
* 编译后的正则对象（官方实现用 `std::regex` 或自写 NFA；见 §6）。

### 标志
| 标志 | 名 | 语义 |
|---|---|---|
| `g` | global | `exec`/`__match` 每次推进 `lastIndex`，可重复迭代出所有匹配 |
| `i` | ignoreCase | 大小写不敏感（仅对 ASCII 字母） |
| `m` | multiline | `^`/`$` 匹配每行的首尾 |
| `s` | dotAll | `.` 匹配任意字节（含 `\n`） |
| `y` | sticky | 锚定在 `lastIndex` 处匹配 |

非法标志报错。`g` 与 `y` 可共存（JS 允许，语义：每次从 `lastIndex` 开始且必须在该处匹配）。

> **风格建议**：模式串用反引号字符串（不反转义，`\d`、`\w`、`\s` 等原样保留，避免 `'\\d+'` 双重转义）。无标志时用尾随字符串简写 `re`\d+``，带标志时用括号 `re(`\d+`, 'g')`。

## 3. 元方法

### regex.__match(self, str)  ← 规范已有的空标题
这是**匹配元方法**，对应 JS 的 `Symbol.match`。语义：

```
__match(self, str):
    if self has flag 'g':
        # 重复匹配，返回所有匹配构成的数组（空匹配算一次，不推进 lastIndex 跨过空匹配——同 JS）
        results = []
        loop:
            m = self:exec(str)
            if m is null: break
            push m[0] (整个匹配文本) to results   # 与 JS String.match 一致：g 下返回匹配子串数组
        return results
    else:
        m = self:exec(str)
        return m   # 数组或 null
```

`__match` 是**显式调用的匹配协议**，由 `String.match`（§4）调用。

## 4. 方法

### regex 实例方法

#### regex.exec(self, str)
在 `str` 上从 `lastIndex` 处执行一次匹配。
* `g`/`y` 标志下，匹配成功后把 `lastIndex` 设为匹配末尾下标；失败时重置为 0。
* 无 `g`/`y` 时，始终从 0 开始，不修改 `lastIndex`。
* 返回**数组**或 `null`。匹配成功时返回的数组额外带以下属性（同 JS）：
| 属性 | 类型 | 说明 |
|---|---|---|
| `index` | number | 匹配起始字节下标 |
| `input` | string | 原始输入串 |
| `groups` | object/null | 命名捕获组构成的对象（`{name = value}`）；无命名捕获时为 `null` |

数组元素 `[0]` 是整个匹配文本，`[1]`..`[n]` 是各捕获组；未参与匹配的组为 `null`。可直接解构：`let [whole, g1, g2] = pat.exec(s)`。

#### regex.test(self, str)
等价于 `self.exec(str) != null`，但不构造结果数组。
* `g`/`y` 下同样推进 `lastIndex`（与 JS 一致；这是 JS 著名的坑，但保持一致优于惊喜）。

### String 上新增的方法（依赖 regex）

这些放在 `classes.md` 的 `## String` 下，不是 regex 模块的成员，但属于 regex 设计的一部分：

#### String.match(self, pat)
* 若 `pat` 有 `__match` 元方法：以 `pat` 为 self、`self`（原字符串）为参数调用它，返回其结果。
* 若 `pat` 是字符串：隐式转为 `regex(pat)`（无标志），再走 `__match`。
* 无匹配返回 `null`。

#### String.matchAll(self, pat)
* 若 `pat` 是字符串：隐式转为 `regex(pat, 'g')`（自动加 `g` 标志）。
* 要求 `pat` 带 `g` 标志（否则报错——与 JS 一致，避免遗忘 `g` 导致死循环）。
* 返回**迭代器**：每次 `next` 产出一个匹配结果数组（同 `exec` 的返回值），直到匹配结束。
* 迭代器是一次性的（耗尽即止）。

#### String.search(self, pat)
* 返回第一个匹配的字节下标，无匹配返回 -1。
* 不修改 `pat.lastIndex`（JS 行为）。
* `pat` 为字符串时隐式构造。

#### String.replace(self, pattern, replacement)
* `pattern`：regex 实例或字符串。
    * 字符串 pattern：只替换第一个出现的子串（**不**做全局；JS 的字符串 replace 也只换第一个）。
    * regex pattern：按标志行为；`g` 换全部，否则换第一个。
* `replacement`：
    * **字符串**：支持 `$$`→`$`、`$&`→整个匹配、`` $1 ``..`` $9 ``→捕获组、`$<name>`→命名捕获组、`` $` ``→匹配前、`$'`→匹配后（同 JS）。
    * **函数** `fun(match, g1, g2, ..., index, input)`：返回替换文本。捕获组按实参传入；未匹配的组传 `null`。
* 返回新字符串（原串不变，字符串不可变）。

#### String.split(self, separator, limit?)
* `separator`：regex 实例、字符串或 null。
    * null/空串：按字节拆分（每个字节一个元素）。
    * 字符串：按字面子串拆分。
    * regex：按匹配拆分；捕获组并入结果（同 JS）。
* `limit`：最多保留前 `limit` 段；省略为全部。
* 返回数组。

> 上述 String 方法中，`match`/`matchAll`/`search` 是新增；`replace`/`split` 若已存在则**覆盖**为支持 regex 的版本（需在实现前确认现状，见 §5）。

## 5. 与现有规范/实现的衔接点

1. **`modules.md` 的 `## regex` 章节**：定稿后替换现有空标题，列全构造、标志、实例属性、方法。
2. **`classes.md` 的 `## String`**：补 `match`/`matchAll`/`search`/`replace`/`split`（均为新建，现有 String 无这些方法）。
3. **`__match` 元方法**：在 `抽象操作.md` 的「元方法 · 其他时机」补一行：`__match` 由 `String.match` 调用，非运算符触发。
4. **错误类型**：模式编译失败用 `JuaSyntaxError`（`kind=Syntax`）；实参类型不符用 `JuaTypeError`；`lastIndex` 越界用 `JuaRangeError`。

## 6. 用例（预期行为）

```jua
let re = require 'regex'

let pat = re`\d+`     // 无标志简写
let patg = re(`\d+`, 'g')  // 带标志用括号
let s = 'abc 12 def 345 ghi'

// exec 迭代
let m
m = patg:exec(s)     // {0='12', index=4, input=s}
m = patg:exec(s)     // {0='345', index=11}
m = patg:exec(s)     // null

patg.lastIndex = 0   // 重置
// String.match（g 下返回子串数组）
eq(s:match(patg):join(','), '12,345')

// test（注意 g 下推进 lastIndex）
patg.lastIndex = 0
ok(patg:test(s))     // true, lastIndex→6
ok(patg:test(s))     // true, lastIndex→14
ok(!patg:test(s))    // false, lastIndex→0

// replace
eq('a1b2c3':replace(re(`\d`, 'g'), 'X'), 'aXbXcX')        // g 换全部
eq('a1b2c3':replace(re`\d`, 'X'), 'aXb2c3')               // 无 g 换第一个
eq('a1b2':replace(re`(\d)`, '[$1]'), 'a[1]b[2]')          // 捕获组引用
eq('a1b2':replace(re`(\d)`, fun(m, g1){ return '[' + g1 + ']' }), 'a[1]b[2]')

// replaceAll
eq('a1b2c3':replaceAll('1', 'X'), 'aXb2c3')               // 字符串换全部
eq('a1b2c3':replaceAll(re(`\d`, 'g'), 'X'), 'aXbXcX')     // regex 须带 g

// search
eq('abc123':search(re`\d+`), 3)
eq('abc':search(re`\d+`), -1)

// split
eq('a1b2c3':split(re`\d`), ['a','b','c',''])

// 解构 exec 结果
let [whole, digits] = re`(\d+)`:exec('x42y')
eq(whole, '42')
eq(digits, '42')

// 命名捕获
let date = re`(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})`:exec('2026-11-13')
eq(date.groups.year, '2026')
eq(date.groups.month, '11')
let {year, day} = date.groups
eq(year, '2026')
eq(day, '13')
eq('2026-11-13':replace(date, '$<year>/$<month>'), '2026/11')   // $<name> 引用

// matchAll 迭代器
for(m in 'a1b2c3':matchAll(re(`\w(\d)`, 'g'))){
    print(m[0], m[1])   // a1 a / b2 b / c3 c
}
```

## 8. 推进顺序

1. 定稿内容并入 `modules.md` 的 `## regex`，`classes.md` 的 `## String` 补方法，`抽象操作.md` 的元方法表补 `__match`。
2. 实现 `m-regex.cpp`（构造 + exec + test + `__match`）。
3. 实现 String 的 `match`/`search`/`replace`/`split`/`matchAll`。
4. 新增 `test/regex.jua`，覆盖 §7 用例 + 边界（空匹配、lastIndex 回绕、捕获组未参与、sticky、标志组合）。
5. 更新 `完成度.md`：regex 的 2 个空章节清零，String 新增 5 个方法标题。

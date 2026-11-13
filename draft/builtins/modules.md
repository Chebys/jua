内置模块
======
可以直接通过 require 导入（无需查找模块文件）的模块。

<!--官方实现中（暂未实现）的非标准内置模块：fs, unicode, http, base64, cli-->
## math
三角函数与对数一律使用弧度与自然对数。
### math.PI
圆周率。
### math.E
自然对数的底。
### math.abs
返回参数的绝对值。
### math.acos
反余弦（弧度）。参数必须在 [-1, 1] 内，否则报错。
### math.asin
反正弦（弧度）。参数必须在 [-1, 1] 内，否则报错。
### math.atan
反正切（弧度）。
### math.ceil
向上取整。
### math.cos
余弦。
### math.exp
e 的幂。
### math.floor
向下取整。
### math.log
自然对数。参数必须为正数，否则报错。
### math.log2
以 2 为底的对数。参数必须为正数，否则报错。
### math.log10
以 10 为底的对数。参数必须为正数，否则报错。
### math.max
返回任意数量数字参数中的最大值。至少需要 1 个参数，否则报错。
### math.min
返回任意数量数字参数中的最小值。至少需要 1 个参数，否则报错。
### math.random
返回 [0, 1) 区间内的伪随机浮点数。无参数。
### math.round
四舍五入，恰好一半时远离零。
### math.sin
正弦。
### math.sqrt
平方根。参数必须非负，否则报错。
### math.tan
正切。
### math.trunc
截断（向零取整）。

## regex
是一个可构造类。`require 'regex'` 直接返回构造函数，无需解构。
* 匹配单位是**字节**（非 Unicode 码点）。`\d`=`[0-9]`，`\w`=`[0-9A-Za-z_]`，`\s`=`[ \t\n\r\f\v]`，`.` 匹配单个字节。
* 不提供字面量语法（`/pattern/flags`）；不提供 `u` 标志；不提供后顾断言（`(?<=...)` / `(?<!...)`）。
* 提供命名捕获（`(?<name>...)`）、反向引用（`\1`–`\9`）、前瞻（`(?=...)` / `(?!...)`）。

### regex(pattern, flags='')
* `pattern`：字符串，正则模式体。`flags`：字符串，标志组合，无参或空串为无标志。
* 模式或标志非法时报错。
* 利用尾随字符串实参语法，无标志构造可简写：`re`\d+`` 等价于 `re(`\d+`)`。建议模式串用反引号字符串（不反转义）。

#### 标志
| 标志 | 语义 |
|---|---|
| `g` | `exec`/`__match` 每次推进 `lastIndex` |
| `i` | 大小写不敏感（仅 ASCII 字母） |
| `m` | `^`/`$` 匹配每行首尾 |
| `s` | `.` 匹配任意字节（含 `\n`） |
| `y` | 锚定在 `lastIndex` 处匹配 |

`g` 与 `y` 可共存。非法标志报错。

#### 实例属性
| 属性 | 说明 |
|---|---|
| `source` | 原始模式串 |
| `flags` | 标志串（按 `gimsy` 排序） |
| `lastIndex` | 下次匹配起始字节下标，初值 0。仅 `g`/`y` 下被 `exec`/`__match` 推进 |

### regex.__match(self, str)
匹配元方法，对应 JS 的 `Symbol.match`，由 `String.match` 调用。
* 有 `g` 标志：重复匹配，返回所有匹配文本构成的数组（空匹配算一次）。
* 无 `g` 标志：执行一次匹配，返回结果数组或 `null`。

### regex.exec(self, str)
从 `lastIndex` 处执行一次匹配。
* `g`/`y` 下匹配成功后推进 `lastIndex` 至匹配末尾；失败时重置为 0。无 `g`/`y` 时始终从 0 开始，不修改 `lastIndex`。
* 返回**数组**或 `null`。数组 `[0]` 是整个匹配文本，`[1]`..`[n]` 是捕获组（未参与匹配的组为 `null`）。数组额外带属性：
    * `index`：匹配起始字节下标
    * `input`：原始输入串
    * `groups`：命名捕获组对象（`{name = value}`）；无命名捕获时为 `null`

### regex.test(self, str)
等价于 `self:exec(str) != null`，但不构造结果数组。
* `g`/`y` 下同样推进 `lastIndex`（与 JS 一致）。

## json
### json.encode(value)
把值编码为 JSON 文本。
* 支持 null、布尔、数字、字符串、数组与对象；函数被跳过。
* 对象的键来自自身的属性，顺序取决于实现。

### json.decode(str)
把 JSON 文本解码为值，非法输入报错。
* 结果是字面对象或数组，不携带类的原型。

## date
是一个标准可构造类。
### date.init(self, *args)
### date.now()
### date.getTime(self)

## promise
是一个可构造类。

<!-- package -->
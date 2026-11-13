内置模块
======
可以直接通过 require 导入（无需查找模块文件）的模块。

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
* 由此，对 UTF-8 多字节字符，`.` 只匹配其中一个字节，`\w` 也不匹配非 ASCII 字母。需要按字符处理时，应先把字符串解码成码点序列。
* 不提供字面量语法（`/pattern/flags`）；不提供 `u` 标志；不提供后顾断言（`(?<=...)` / `(?<!...)`）。
* 提供命名捕获（`(?<name>...)`）、反向引用（`\1`–`\9`）、前瞻（`(?=...)` / `(?!...)`）。

### regex(pattern, flags='')
* `pattern`：字符串，正则模式体。`flags`：字符串，标志组合，无参或空串为无标志。
* 模式或标志非法时报 `JuaSyntaxError`（类别 `syntax`）。
* 实参类型不符报 `JuaTypeError`；`lastIndex` 越界报 `JuaRangeError`。
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
* 带 `toJSON` 元方法的对象先调用它，再用返回值编码（例如 `date`）。

### json.decode(str)
把 JSON 文本解码为值，非法输入报错。
* 结果是字面对象或数组，不携带类的原型。

## date
是一个标准可构造类。`require 'date'` 直接返回它。

时间值是 **UTC 毫秒**，存为实例的自身属性 `time`（数字，可能为 NaN）。以下约定适用于整个模块：

* 一律按 UTC 解释，不读宿主时区。
* 无效日期的时间值为 NaN：取值方法与 `getTime` 返回 NaN，`toISOString` 报 range 错误。
* 月份为 **1–12**；星期为 **1–7**（1 = 周一）。
* 字段越界时**进位**：`date(2026, 13, 1)` 等于 2027 年 1 月。

### date.init(self, *args)
与 `date(...)` 相同：
* 无额外参数：当前时刻。
* 一个数字：毫秒时间值。
* 两个及以上数字：`(y, m, d=1, h=0, mi=0, s=0, ms=0)`，按 UTC 解释。

时间值超出 ±8.64e15 时得到无效日期，不报错；参数类型不符报 type 错误。

### date.now()
返回当前时刻的实例。

### date.parse(str)
解析 ISO 8601 子集，返回毫秒时间值；无法解析时返回 NaN（不报错）。

接受的文本：`YYYY-MM-DD`，可追加 `THH:mm`、`:ss`、`.sss`，最后可追加 `Z` 或 `±HH:MM`。
未带时区标记时按 UTC 解释。

### date.UTC(y, m, d=1, h=0, mi=0, s=0, ms=0)
由字段得到毫秒时间值，不构造实例。

### date.getTime(self)
返回毫秒时间值。

### 取值方法
| 方法 | 返回 |
|---|---|
| `getFullYear(self)` | 年 |
| `getMonth(self)` | 月（1–12） |
| `getDate(self)` | 月内第几天（1–31） |
| `getDay(self)` | 星期（1 = 周一 … 7 = 周日） |
| `getHours(self)` / `getMinutes(self)` / `getSeconds(self)` / `getMilliseconds(self)` | 时分秒毫秒 |

无效日期上一律返回 NaN。

### 设值方法
| 方法 | 说明 |
|---|---|
| `setTime(self, ms)` | 直接设置时间值 |
| `setFullYear(self, y[, m, d])` | 设置年月日 |
| `setMonth(self, m[, d])` | 设置月日 |
| `setDate(self, d)` | 设置月内第几天 |
| `setHours(self, h[, mi, s, ms])` | 设置时刻 |
| `setMinutes(self, mi[, s, ms])` | |
| `setSeconds(self, s[, ms])` | |
| `setMilliseconds(self, ms)` | |

* 返回设置后的毫秒时间值。
* 同样支持越界进位；中间结果超出范围则变为无效日期。
* 实参必须是数字，否则报 type 错误。

### 格式化方法
| 方法 | 返回 |
|---|---|
| `toISOString(self)` | `YYYY-MM-DDTHH:mm:ss.sssZ`；无效日期报 range 错误 |
| `toString(self)` | 同 `toISOString`；无效日期为 `"Invalid Date"` |
| `toJSON(self)` | 同 `toISOString`；无效日期为 null |

年份超出 0000–9999 时用扩展写法 `±YYYYYY`。

### 元方法
| 时机 | 名字 | 行为 |
|---|---|---|
| 对象转字符串 | `toString` | 见上 |
| `==` | `__eq` | 时间值相等 |
| `<` `<=` | `__lt` `__le` | 按时间值比较；任一侧无效则为 false |
| `-` | `__sub` | 日期减日期得毫秒差；日期减数字得新日期 |

## promise
是一个可构造类。

**未来计划，尚未定义。** 它依赖 `async` / `await` 的语义，要先在[执行模型](../抽象操作.md#执行模型)里规定多任务模式，才能定下单次回调、链式 `.then` 与错误传播的形式。

<!-- package -->
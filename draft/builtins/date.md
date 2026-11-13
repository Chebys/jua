# date 模块设计（草案）

> 参考 JS 的 `Date`，但贴合 Jua 的数字、字节串与迭代/元方法机制。
> 本文件是设计稿；定稿后内容并入 `builtins/modules.md` 的 `## date` 章节，本文件可删除。

## 1. 定位与约束

### 为什么单独成文
`modules.md` 里 `date` 只有「是一个标准可构造类」和三个空标题（`date.init`、`date.now`、`date.getTime`）。整体未实现，先把语义定清楚。

### 硬约束（来自现有规范）
1. **数字是 double**（[值与类型](../值与类型.md#数字)）：整数在 ±2^53 内精确。毫秒时间值约 1.7e12，远在范围内。
2. **字符串是字节串**：不引入本地化名称（月份名、星期名、时区缩写）。
3. **类是原型上带 `__class` 的对象**（[杂项·类与实例](../杂项.md#类与实例)）；`date` 是标准可构造类，`require 'date'` 直接返回它。
4. 没有 `undefined`，一律用 `null`；错误用 `JuaTypeError` / `JuaRangeError`。

### 与 JS 的关键差异
| 维度 | JS `Date` | Jua `date` |
|---|---|---|
| 时区 | 本地时区 + 一整套 `getUTC*` | **只有 UTC**：不读宿主时区，行为可复现 |
| 无效日期 | `Invalid Date`（时间值是 NaN） | 相同：时间值为 NaN |
| 月份下标 | 0–11 | **1–12**（待定，见 §7） |
| 星期 | 0=周日 | **1=周一 … 7=周日**（待定） |
| 字符串解析 | 多种遗留格式 | **只认 ISO 8601 子集** |
| 本地化 | `toLocaleString` 等 | 不提供 |
| 时间值范围 | ±8.64e15 ms | 相同 |

> UTC-only 的理由：时区数据库是宿主的责任，规范里无法固定；而「同一份代码在不同机器上打印出不同时间」会让测试不可复现。需要本地时间时由宿主另行提供（例如 `host.utcOffset()`），不属于 `date` 模块。

## 2. 构造

```
let date = require 'date'

date()                                  // 当前时刻
date(ms)                                // 从毫秒时间值
date(y, m, d=1, h=0, mi=0, s=0, ms=0)   // 从字段，按 UTC 解释
```

* 字段越界时**进位**（同 JS）：`date(2026, 13, 1)` 等于 2027 年 1 月；`date(2026, 1, 0)` 等于 2025 年 12 月 31 日。
* 时间值超出 ±8.64e15 时，得到时间值为 NaN 的日期（无效日期），不报错。
* 参数类型不符时报 `JuaTypeError`。

## 3. 实例方法与属性

实例没有可写属性；时间值存在内部属性里（[值与类型·内部属性](../值与类型.md#内部属性)）。

### 取值
| 方法 | 返回 |
|---|---|
| `getTime(self)` | 毫秒时间值；无效日期为 NaN |
| `getFullYear(self)` | 四位年份 |
| `getMonth(self)` | 月份 |
| `getDate(self)` | 月内第几天（1–31） |
| `getDay(self)` | 星期 |
| `getHours(self)` / `getMinutes(self)` / `getSeconds(self)` | 时分秒 |
| `getMilliseconds(self)` | 毫秒 |

无效日期上，上述方法一律返回 NaN。

### 设值
| 方法 | 说明 |
|---|---|
| `setTime(self, ms)` | 直接设置时间值，返回新的毫秒值 |
| `setFullYear(self, y[, m, d])` | 设置年月日 |
| `setMonth(self, m[, d])` | 设置月日 |
| `setDate(self, d)` | 设置月内第几天 |
| `setHours(self, h[, mi, s, ms])` | 设置时刻 |
| `setMinutes(self, mi[, s, ms])` | |
| `setSeconds(self, s[, ms])` | |
| `setMilliseconds(self, ms)` | |

* 设值同样支持越界进位，返回设置后的毫秒时间值。
* 任一中间结果超出范围，日期变为无效日期。

### 格式化
| 方法 | 返回 |
|---|---|
| `toString(self)` | 同 `toISOString`（同时作为 `toString` 元方法） |
| `toISOString(self)` | `YYYY-MM-DDTHH:mm:ss.sssZ` |
| `toJSON(self)` | 同 `toISOString`；无效日期返回 `null` |

* 无效日期上 `toISOString` 抛 `JuaRangeError`（同 JS）。
* 年份超出 0000–9999 时按 ISO 8601 扩展年份写法 `±YYYYYY`。

## 4. 静态方法

| 方法 | 说明 |
|---|---|
| `date.now()` | 返回当前时刻的 `date` 实例 |
| `date.parse(str)` | 解析 ISO 8601 子集，返回毫秒时间值；无法解析返回 NaN |
| `date.UTC(y, m, d=1, h=0, mi=0, s=0, ms=0)` | 由字段得到毫秒时间值，不做时区转换 |

`date.parse` 接受的形式（其余一律 NaN，不报错）：

```
YYYY-MM-DD
YYYY-MM-DDTHH:mm
YYYY-MM-DDTHH:mm:ss
YYYY-MM-DDTHH:mm:ss.sss
上述四种 + 'Z'
上述四种 + '±HH:MM'
```

* 未带时区标记时按 UTC 解释（与 JS 对 `YYYY-MM-DD` 的解释不同，见 §7）。

## 5. 元方法

| 时机 | 名字 | 行为 |
|---|---|---|
| 对象转字符串 | `toString` | 同 `toISOString` |
| `==` | `__eq` | 时间值相等（NaN 与自身也不相等） |
| `<` `<=` | `__lt` `__le` | 按时间值比较 |
| `-` | （待定） | `d1 - d2` 得毫秒差；`d - n` 得新日期 |

## 6. 与现有规范的衔接点

1. **`modules.md` 的 `## date`**：定稿后替换三个空标题，列全构造、取值、设值、格式化、静态方法。
2. **`json.encode`**：是否让带 `toJSON` 元方法的对象先调用它（JS 的 `JSON.stringify` 如此）。若采纳，`json.encode(date)` 得到 ISO 字符串；否则得到 `{}`（实例没有自身属性）。**待定**。
3. **`Object.toString` 与 `date.toString`**：`date` 实例的原型上有 `toString`，走[对象转字符串](../抽象操作.md#对象转字符串)第 2 步。
4. **`__eq` / `__lt` / `__le`**：按[抽象操作·元方法](../抽象操作.md#运算符触发)的规则，查找在原型上。

## 7. 需要拍板的点

1. **月份下标**：JS 是 0–11。Jua 用 1–12 更不易出错，但与 JS 用户的直觉相反。
2. **星期下标**：JS 是 0=周日；ISO 8601 是 1=周一…7=周日。
3. **设值方法要不要全套**：`setFullYear`/`setMonth`/… 共 8 个，是 JS 的样子；也可以只给 `setTime` 加构造重建。
4. **`json.encode` 是否认 `toJSON`**。
5. **是否提供日期减法**（`__sub`），以及减法结果（毫秒差 vs 新日期）如何按参数类型分派。
6. **`date.parse` 遇到无时区标记的日期**：按 UTC（本文建议）还是按本地时区（JS 对 `YYYY-MM-DD` 按 UTC，对 `YYYY-MM-DDTHH:mm` 按本地，容易踩坑）。

## 8. 用例（预期行为）

```jua
let date = require 'date'

let x = date(0)
eq(x:getFullYear(), 1970, 'epoch')
eq(x:toISOString(), '1970-01-01T00:00:00.000Z', 'epoch 的 ISO')

let y = date(2026, 13, 1)                  // 越界进位
eq(y:getFullYear(), 2027, '月份进位')
eq(y:getMonth(), 1, '进位后的月份')

let bad = date(1e16)                       // 超出范围，成为无效日期
ok(bad:getTime() != bad:getTime(), '无效日期的时间值是 NaN')  // NaN 不等于自身

let a = date(1000), b = date(3000)
ok(a < b, '日期可比较')
eq(b:getTime() - a:getTime(), 2000, '时间值相减')

eq(date.parse('2026-11-13T19:39:20Z'), date.UTC(2026, 11, 13, 19, 39, 20), 'ISO 解析')
let nn = date.parse('not a date')
ok(nn != nn, '无法解析时返回 NaN')
```

## 9. 推进顺序

1. 定下 §7 的六点，把本文并入 `modules.md` 的 `## date`，删除本文件。
2. 实现 `m-date.cpp`：时间值存取、字段与时间值互转（用 `std::chrono` 或手写 civil-from-days 算法）、`init`、`now`、`getTime`、`toISOString`。
3. 补取值/设值/格式化与静态方法。
4. 实现元方法（`toString`、`__eq`、`__lt`、`__le`；`__sub` 视第 5 点决定）。
5. 新增 `test/date.jua`：epoch、闰年、月末进位、无效日期、ISO 解析的合法与非法输入、比较与减法。
6. 更新 `完成度.md` 与 `改进清单` 的「待实现」。

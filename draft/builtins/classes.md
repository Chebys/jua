内置类
======
关于“类”的定义，见[这里](../杂项.md#类与实例)。

所有内置类都是可构造类。
<!--内置类的内置属性不应被改变，否则行为未定义。-->

若无特别说明，`self` 表示对应类的实例。

## Object
Object 是所有普通对象的原型。

### Object.assign(target, *sources)
把各源的**自身属性**依次复制到 target，返回 target。
* target 必须是对象；源必须是对象，null 被忽略。
* 后出现的源覆盖先出现的源；值按引用复制，不做深拷贝。
* 不复制继承来的属性。

例：`Object.assign({}, {a = 1}, {b = 2})` 得到 `{a = 1, b = 2}`。

### Object.del(obj, key)
删除 obj 的自身属性 key，返回 null。
* obj 必须是对象，key 必须是字符串，否则报错。

### Object.entries(obj)
对象自身的所有键值对（二元数组）构成的数组
### Object.get(obj, key, default=null)
对 obj（未必是对象）进行[属性访问](../抽象操作.md#属性访问)
* 相比直接抽取，不会因为属性不存在而报错
* 属性不存在时返回 default

### Object.getOwn(obj, key, default=null)
返回 obj 的自身属性 key；不存在时返回 default。
* obj 必须是对象，key 必须是字符串，否则报错。
* 与 `Object.get` 的区别：只查自身属性，不沿原型链查找。
### Object.getProto(v)
返回 v 的原型；v 没有原型时返回 null。
* v 可以是任意值。数字、字符串等内置类型都有自己的原型；字面对象没有。
### Object.hasOwn
返回布尔值：obj 是否具有自身属性 key。
* obj 必须是对象，key 必须是字符串，否则报错。

### Object.id(v)
返回 v 的 id，一个正整数。
* v 必须是对象或函数，否则报 type 错误。
* 同一个值多次取用得到同一个 id；不同值的 id 不同。
* id 不复用，见[垃圾回收](../垃圾回收.md#标识)。
### Object.keys(obj)
返回对象自身所有属性键构成的数组。
* 键的顺序取决于实现。
### Object.new(proto, *args)
创建并返回一个以 proto 为原型的新对象。
* proto 省略或为 null 时，新对象没有原型。
* proto 必须是对象或 null，否则报 type 错误。
* 若沿原型链能找到 `init` 元方法，则以新对象为首个实参、`args` 为其余实参调用它；没有 `init` 时直接返回新对象。
* 调用一个类等价于对它执行 Object.new：`K(args)` 就是 `Object.new(K, args)`。

等效于：

    fun(proto, *args){
		let obj = {}
		Object.setProto(obj, proto)
		if(type(proto.init) == 'function')
			proto.init(obj, *args)
		return obj
	}

### Object.next(obj, key)
用于获取对象的“下一个”键。
* 键的实际顺序取决于实现。

若传入的key是最后一个键，或对象没有任何属性，则返回`{done=true}`。

否则，设下一个键为 nextkey，返回`{done=false, key=nextkey, value=nextkey}`。

本函数是对象的默认迭代函数，见[迭代](../抽象操作.md#迭代)。
### Object.pairs(obj)
返回可迭代对象，产生对象自身的所有键值对，见[属性迭代](../抽象操作.md#属性迭代)。
* 相比 Object.entries 更加节省内存

### Object.set
设置 obj 的自身属性 key 为 value，返回 null。
* obj 必须是对象，key 必须是字符串，否则报错。

### Object.setProto
设置对象的原型。
### Object.toString
返回对象的文本表示：`{a, b}`，即自身属性的键列表（不含值）。
* 键的顺序取决于实现。
* 与[对象转字符串](../抽象操作.md#对象转字符串)不同：后者在对象没有 `toString` 元方法时得到 `[object]`。


## Number
Number 是所有数字的原型。`Number(v)` 把数字或数字字符串转换为数字。
### Number.LITTLE_ENDIAN
若平台使用小端序，则为 true，否则为 false。
### Number.toString(num)
按[数字转字符串](../抽象操作.md#数字转字符串)的规则返回字符串。
### Number.range
值为 [Range](#range非全局变量)。
### Number.__range(self, end)
return Number.range(self, end, 1)

### Number.isInt(num)
num 有限且没有小数部分时返回 true。
* 非数字返回 false；无穷与 NaN 返回 false。

### 编解码函数
所有编码函数以 `Number.encodeXXX` 命名，解码函数以 `Number.decodeXXX` 命名，其中 `XXX` 是编码名称。

| 编码名 | 类型 | 宽度（字节） |
|---|---|---|
| `Int8` / `Uint8` | 有符号 / 无符号 8 位整数 | 1 |
| `Int16` / `Uint16` | 16 位整数 | 2 |
| `Int32` / `Uint32` | 32 位整数 | 4 |
| `Float32` / `Float64` | 浮点数 | 4 / 8 |

* `Number.encodeXXX(*nums)`：把每个实参按其类型换成 `XXX`，依次追加原始字节，返回字节串。
* `Number.decodeXXX(str)`：读 `str` 开头 `XXX` 宽度的字节，返回对应的数字；`str` 不足该宽度时报错。
* 字节序是宿主的原生顺序，由 `Number.LITTLE_ENDIAN` 指示。

例：`Number.encodeUint8(97, 98)` 得到 `'ab'`；`Number.decodeUint16(Number.encodeUint16(513))` 得到 `513`。
### Number.parse(str)
把字符串解析为数字。
* 解析规则同数字字面量：可选符号、整数部分、小数部分、指数部分。
* 无法解析时返回 `null`。

## String
String 是所有字符串的原型。`String(v)` 返回 v 的 [ToString](../抽象操作.md#tostring)；无参时为空串。

字符串是字节串，下列操作一律以**字节**为单位。
### String.byte(self, i=0)
返回第 i 个字节的数值（0～255）。
* i 为负数从末尾算起；其余按长度取模。
* 空字符串报错。

### String.escape(str)
把字符串中的特殊字节转义为 `\xXX` 形式（`x` 加两位十六进制，见[反转义](../词法.md#字面字符串求值)），使其可安全嵌入单引号/双引号字符串字面量。
* 被转义的字节：`\`、引号、以及非可打印 ASCII 字节（0x00–0x1F、0x7F）。
* 其余字节不变。

* 必须用 `\xXX` 而不是 `\XX`：`\1`～`\9` 在 Jua 里是语法错误，`\41` 无法回读。
### String.fromByte(*bytes)
由实参依次取低 8 位构成的字符串。
### String.hasItem(self, item)
item 为字符串时判断 self 是否包含它（子串）；否则为 false。
### String.getItem(self, i)
返回第 i 个字节构成的单字节字符串。
* 索引按[下标](../值与类型.md)规则回绕，负数从末尾算起。
### String.len(str)
字节数。
### String.lower(str)
把 ASCII 大写字母转成小写，其余字节不变。
### String.match(self, pat)
* 若 `pat` 有 `__match` 元方法：以 `pat` 为 self、`self` 为参数调用它，返回其结果。
* 若 `pat` 是字符串：隐式转为 `regex(pat)`（无标志），再走 `__match`。
* 无匹配返回 `null`。
### String.matchAll(self, pat)
* 若 `pat` 是字符串：隐式转为 `regex(pat, 'g')`。
* 要求 `pat` 带 `g` 标志，否则报错。
* 返回一次性迭代器，每次产出一个匹配结果数组（同 `regex.exec` 的返回值）。
### String.replace(self, pattern, replacement)
* `pattern` 为字符串时只替换第一个出现的子串；为 regex 实例时按标志行为（`g` 换全部，否则换第一个）。
* `replacement` 为字符串时支持占位符：`$$`→`$`、`$&`→整个匹配、`$1`..`$9`→捕获组、`$<name>`→命名捕获组、`` $` ``→匹配前、`$'`→匹配后。
* `replacement` 为函数 `fun(match, g1, g2, ..., index, input)` 时，返回替换文本；未参与匹配的组传 `null`。
* 返回新字符串。
### String.replaceAll(self, pattern, replacement)
* `pattern` 为字符串时替换所有出现的子串；为 regex 实例时要求带 `g` 标志（否则报错）。
* `replacement` 的占位符与函数形式同 `replace`。
* 返回新字符串。
### String.search(self, pat)
返回第一个匹配的字节下标，无匹配返回 -1。不修改 `pat.lastIndex`。`pat` 为字符串时隐式构造。
### String.split(self, separator, limit?)
* `separator` 为 null/空串时按字节拆分；为字符串时按字面子串拆分；为 regex 实例时按匹配拆分（捕获组并入结果）。
* `limit` 最多保留前 `limit` 段，省略为全部。
* 返回数组。
### String.unescape(str)
`String.escape` 的逆操作：把 `\xXX` 形式的转义序列还原为对应字节。
* 非法的转义序列报错。
### String.upper(str)
把 ASCII 小写字母转成大写，其余字节不变。
### String.slice(self, i, j)
返回 `[i, j)` 区间的子串。
* i 省略为 0，j 省略为长度；负数从末尾算起；越界钳制；`i >= j` 时为空串。
### String.toHex
每个字节转成两位小写十六进制，用空格分隔。

## Boolean
Boolean 是所有布尔值的原型。
## Function
Function 是所有函数（原生函数与自定义函数）的原型。
## Array
Array 是所有数组的原型。`Array(iterable)` 把可迭代对象的元素收集为新数组。
### Array.entries(self)
返回可迭代对象，产生 `[index, value]` 二元数组。
### Array.getItem(self, i)
返回第 i 个元素。
* 索引按[下标](../值与类型.md)规则回绕，负数从末尾算起。
### Array.join(self, sep='')
把元素依次转换为字符串，再用 sep 连接。self 可以是任意可迭代对象。
### Array.len(self)
元素个数。
### Array.pop(self)
删除并返回末尾元素；空数组返回 null。
### Array.push(self, item)
把实参依次追加到末尾，返回 null。
### Array.of(*items)
由实参构成的新数组。
### Array.resize(self, len)
调整数组长度为 len。
* len 大于当前长度时，新增元素填充为 `null`。
* len 小于当前长度时，截断尾部元素。
### Array.setItem(self, i, val)
设置第 i 个元素为 val。
* 索引按[下标](../值与类型.md)规则回绕，负数从末尾算起。
### Array.shift(self)
删除并返回开头元素；空数组返回 null。
### Array.splice(self, start, deleteCount, *items)
从 start 起删除 deleteCount 个元素，再在原位插入 items，返回 null。
* start 为负时从末尾算起；deleteCount 为负按 0 处理；越界钳制。
### Array.toString(self)
返回 `[a, b]` 形式的字符串，元素用 [ToString](../抽象操作.md#tostring)。self 可以是任意可迭代对象。
### Array.unshift(self, item)
把实参依次插到开头，返回 null。

## Buffer
Buffer 是可构造类：`Buffer(n)` 创建 n 字节的缓冲区。
### Buffer.read(self, start, end)
返回 `[start, end)` 区间的字节构成的字符串。
* start 省略为 0，end 省略为长度；负数从末尾算起；越界钳制。
* `start >= end` 时报错。

### Buffer.write(self, bytes, pos)
把字符串写入缓冲区，返回 null。
* pos 省略为 0；负数从末尾算起。
* 超出缓冲区范围时报错。

### Buffer.clear(self)
把缓冲区所有字节置为 0。
### Buffer.len(self)
缓冲区的字节长度。

## Error
Error 是可构造类：`Error(message)` 创建错误对象。
### Error.init(self, message='')
设置错误对象的 `message` 属性。message 省略为空串。
### Error.name
值为 `'Error'`。
### Error.toString(self)
返回 `name: message`。
* name 缺省为 `"Error"`，message 缺省为 `"unknown"`。


## Range（非全局变量）
是一个标准可构造类。通过 `a..b` 运算或 `Number.range(a, b, step)` 构造。

实例属性：`start`、`end`、`step`（均为数字）。
### Range.init(self, start, end, step=1)
设置区间的起点、终点与步长。
### Range.next(self, i)
迭代函数，迭代键即当前值。
* 首次调用（key 不是数字）从 start 开始，否则为上一次的值加 step。
* 当前值 `>= end` 时返回 `{done=true}`；否则返回 `{done=false, key=当前值, value=当前值}`。

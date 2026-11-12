内置类
======
关于“类”的定义，见[这里](../杂项.md#类与实例)。

所有内置类都是可构造类。
<!--内置类的内置属性不应被改变，否则行为未定义。-->

## Object
Object 是所有普通对象的原型。

### Object.del(obj, key)
删除 obj 的自身属性 key，返回 null。
* obj 必须是对象，key 必须是字符串，否则报错。

### Object.entries(obj)
对象自身的所有键值对（二元数组）构成的数组
### Object.get(obj, key, default=null)
对 obj（未必是对象）进行[属性访问](../抽象操作.md#属性访问)
* 相比直接抽取，不会因为属性不存在而报错
* 属性不存在时返回 default

### Object.getOwn
### Object.getProto
获取一个值（未必对象）的原型。
### Object.hasOwn
返回布尔值：obj 是否具有自身属性 key。
* obj 必须是对象，key 必须是字符串，否则报错。

### Object.id
用于获取引用类型值（包括对象和函数）的 id。

满足：
* id 是一个非负整数
* 不同引用具有不同的 id
### Object.keys(obj)
### Object.new(proto, *args)
等效于

    fun(proto, *args){
		let obj = {}
		Object.setProto(obj, proto)
		if(type(proto.init)=='function')
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
### Number.parse(str)

## String
String 是所有字符串的原型。`String(v)` 返回 v 的 [ToString](../抽象操作.md#tostring)；无参时为空串。

字符串是字节串，下列操作一律以**字节**为单位。
### String.byte(self, i=0)
返回第 i 个字节的数值（0～255）。
* i 为负数从末尾算起；其余按长度取模。
* 空字符串报错。

### String.escape(str)
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
### String.unescape(str)
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
### Array.getItem(self, i)
### Array.join(self, sep='')
把元素依次转换为字符串，再用 sep 连接。对任意可迭代对象有效。
### Array.len(self)
元素个数。
### Array.pop(self)
删除并返回末尾元素；空数组返回 null。
### Array.push(self, item)
把实参依次追加到末尾，返回 null。
### Array.of(*items)
由实参构成的新数组。
### Array.resize(self, len)
### Array.setItem(self, i)
### Array.shift(self)
删除并返回开头元素；空数组返回 null。
### Array.splice
	splice(self, start, deleteCount, *items)
从 start 起删除 deleteCount 个元素，再在原位插入 items，返回 null。
* start 为负时从末尾算起；deleteCount 为负按 0 处理；越界钳制。
### Array.toString(self)
返回 `[a, b]` 形式的字符串，元素用 [ToString](../抽象操作.md#tostring)。对任意可迭代对象有效。
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
### Buffer.len(self)

## Error
Error 是可构造类：`Error(message)` 创建错误对象。
### Error.name
值为 `'Error'`。
### Error.toString(self)
返回 `name: message`。
* name 缺省为 `"Error"`，message 缺省为 `"unknown"`。


## Range（非全局变量）
是一个标准可构造类。
### Range.init(self, start, stop, step)
设置区间的起点、终点与步长；step 省略为 1。
### Range.next(self, i)
迭代函数，迭代键即当前值。
* 首次调用（key 不是数字）从 start 开始，否则为上一次的值加 step。
* 当前值 `>= stop` 时返回 `{done=true}`；否则返回 `{done=false, key=当前值, value=当前值}`。

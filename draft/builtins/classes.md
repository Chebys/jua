内置类
======
关于“类”的定义，见[这里](../杂项.md#类与实例)。

所有内置类都是可构造类。
<!--内置类的内置属性不应被改变，否则行为未定义。-->

## Object
### Object.new(proto, *args)
等效于

    fun(proto, *args){
		let obj = {}
		Object.setProto(obj, proto)
		if(type(proto.init)=='function')
			proto.init(obj, *args)
		return obj
	}

### Object.get
### Object.set
### Object.del
### Object.hasOwn
### Object.setProto
设置对象的原型。
### Object.getProto
获取一个值（未必对象）的原型。
### Object.next(obj, key)
用于获取对象的“下一个”键。
* 键的实际顺序取决于实现。

若传入的key是最后一个键，或对象没有任何属性，则返回`{done=true}`。

否则，设下一个键为 nextkey，返回`{done=false, key=nextkey, value=nextkey}`。

本函数是对象的默认迭代函数。

### Object.id
用于获取引用类型值（包括对象和函数）的 id。

满足：
* id 是一个非负整数
* 不同引用具有不同的 id
### Object.toString


## Number
Number 是所有数字的原型。
### Number.LITTLE_ENDIAN
若平台使用小端序，则为 true，否则为 false。
### Number.toString(num)
### Number.range
值为 [Range](#range非全局变量)。
### Number.__range(self, end)
return Number.range(self, end, 1)

### Number.isInt(num)
### 编解码函数
所有编码函数以 Number.encodeXXX 命名，解码函数以 Number.decodeXXX 命名，其中。。。
### Number.parse(str)

## String
String 是所有字符串的原型。
### String.byte(self, i)
### String.fromByte(*bytes)
### String.hasItem(self, i)
### String.getItem(self, i)
### String.len(str)
### String.lower
### String.upper
### String.slice
### String.toHex

## Boolean
## Function
## Array
### Array.of(*items)
### Array.join(self)
### Array.toString(self)
### Array.getItem(self, i)
### Array.setItem(self, i)
### Array.len(self)

## Buffer
### Buffer.read(self, start, end)
### Buffer.write(self, bytes, pos)
### Buffer.clear(self)
### Buffer.len(self)

## Error
### Error.name
值为 `'Error'`。
### Error.toString(self)


## Range（非全局变量）
是一个标准可构造类。
### Range.init(self, start, stop, step)
### Range.next(self, i)
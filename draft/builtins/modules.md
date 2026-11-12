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
### math.acos
### math.asin
### math.atan
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
### math.log10
### math.max
### math.min
### math.random
### math.round
四舍五入，恰好一半时远离零。
### math.sin
正弦。
### math.sqrt
平方根。参数必须非负，否则报错。
### math.tan
正切。
### math.trunc

## regex
是一个可构造类。
### regex.__match(self, str)

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
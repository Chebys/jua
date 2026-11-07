内置函数
======
## class
效果等同于：

    fun class(proto){
        Object.setProto(proto, Object:getProto())
        return proto
    }

## print
将任意数量的值写入[标准输出](../抽象操作.md#标准输出)。

* 值如何转换为文本由宿主决定，不必等于 [ToString](../抽象操作.md#tostring)。
* 官方实现：没有 `toString` 元方法的对象显示其键，如 `{a, b}`；其余值用 ToString。

## require
接收一个字符串作为模块名，并导入模块，返回模块返回值。

## throw
## try
## type
返回代表值类型的字符串。可能为 "null", "number", "string", "boolean", "object", "function"。

## useNS
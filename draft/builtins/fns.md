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

* 模块是一个[函数体](../抽象操作.md#语句块)，它的返回值就是导入该模块得到的结果；没有 `return` 时结果为 null。
* 模块执行完毕后才写入缓存。同一个模块再次导入时直接返回缓存的值，不会再次执行。
* 因为缓存在执行之后才写入，模块在执行过程中导入自己（直接或间接）会使它被重复执行，这种情况不被允许；实现应当报告错误。
* 模块的查找方式由宿主决定。官方实现：内置模块名（如 `math`、`json`）由宿主直接提供，其余在当前目录下查找 `<模块名>.jua`。

## throw
## try
## type
返回代表值类型的字符串。可能为 "null", "number", "string", "boolean", "object", "function"。

## useNS
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
	throw(value)
抛出错误，终止当前计算。
* `value` 为字符串时，它成为错误消息。
* `value` 为 [Error](classes.md#error) 的实例时，对象本身随错误一起抛出：[try](#try) 结果里的 `error` 与 `catch` 的实参都是这个对象。
* `value` 为其他类型时报错。
* 抛出的错误一律归类为 `client`，见 [try](#try)。
* 未被 `try` 捕获时，宿主打印错误并以非零退出码结束本次运行。

* 与 Lua 的 `error` 一样，`throw` 是普通函数，必须写成 `throw(value)`：两个相邻的表达式不构成调用（见[调用](../表达式.md#调用)）。`throw '文本'` 之所以可行，是因为以字面字符串开头的调用形式。

## try
	try(fn, *args)
以 `args` 为实参调用 `fn`，返回一个结果对象，把「成功」与「失败」都变成数据。

成功时返回：

	{status = true, value = 返回值}

失败时返回：

	{status = false, error = 错误对象或错误文本, errorKind = 错误类别}

* 抛出的若是 [Error](classes.md#error) 实例，`error` 就是该对象；否则 `error` 是带类型前缀的文本，如 `JuaTypeError: ...`。
* `errorKind` 是类别名，见[错误处理](../抽象操作.md#错误处理)。
* 参数校验也在保护范围内：`try(1)` 不抛错，而是返回 `status = false`。
* 结果对象具有 `catch` 方法，因此 `try{...}:catch(err){...}` 等价于 `try(fun(){...}):catch(fun(err){...})`：
	* `status` 为真时 `catch` 不调用传入的函数，直接返回结果对象本身；
	* 否则以 `error` 为唯一实参调用它，其返回值即整个表达式的结果。
## type
返回代表值类型的字符串。可能为 "null", "number", "string", "boolean", "object", "function"。

## useNS
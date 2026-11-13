# fs 模块设计（草案）

> 文件系统访问。**非标准模块**：它属于官方实现的扩展，[草案](../draft/)不涉及它，其它实现可以不提供。
> 本文件是设计稿；定稿后并入 `juacpp/README.md` 的模块一节，本文件可删除。

## 1. 定位与约束

### 为什么单独成文
`fs` 依赖宿主的操作系统，规范无法给出可移植的语义（路径分隔符、权限、符号链接、大小写不敏感等）。因此它只作为官方实现的扩展存在，语义以本文件为准。

### 硬约束（来自现有规范）
1. **没有异步**：`async`/`await` 尚未定义，所有函数都是同步的，调用返回即完成。
2. **字符串是字节串**（[值与类型](../draft/值与类型.md#字符串)）：读写都以字节为单位，不做编码转换。
3. **错误要能区分**：类型不符报 `JuaTypeError`，IO 失败报 `JuaError`（类别 `error`），消息里带路径与系统原因。

### 与 Node.js `fs` 的差异
| 维度 | Node `fs` | Jua `fs` |
|---|---|---|
| 同步/异步 | 两套 API | **只有同步** |
| 编码参数 | `'utf8'` 等 | **没有**：一律字节串 |
| 回调/Promise | 有 | **没有**：直接返回或抛错 |
| 路径 | Buffer/URL/字符串 | **只有字符串** |

## 2. 约定

* 路径是字符串，含非 ascii 字符时按 UTF-8 解释；相对路径以进程当前工作目录为基准。
* 不做通配符展开，`*`、`?` 按普通字符处理。
* 读到的内容是字节串，需要文本时由使用者自行解码。
* 写入接受字符串，也接受 `Buffer`（按字节写）。

## 3. 读取与写入

| 函数 | 说明 |
|---|---|
| `fs.read(path)` | 读整个文件，返回字节串 |
| `fs.write(path, data)` | 覆盖写入；文件不存在则创建。返回 null |
| `fs.append(path, data)` | 追加；文件不存在则创建。返回 null |

* 父目录不存在时报错，不自动创建。
* `data` 必须是字符串或 `Buffer`，否则报 type 错误。

## 4. 查询

| 函数 | 说明 |
|---|---|
| `fs.exists(path)` | 路径是否存在，返回布尔 |
| `fs.stat(path)` | 返回 `{type = 'file' \| 'directory' \| 'other', size, mtime}`；不存在返回 null |
| `fs.list(dir)` | 返回目录项名字构成的数组；不是目录时报错 |

* `size` 是字节数；对目录无意义，取 0。
* `mtime` 是修改时间，单位毫秒（与 [`date`](../draft/builtins/modules.md#date) 的时间值一致，可直接 `date(mtime)`）。

## 5. 目录与改名

| 函数 | 说明 |
|---|---|
| `fs.mkdir(path)` | 创建目录，逐级创建（同 `mkdir -p`）；已存在不报错 |
| `fs.remove(path)` | 删除文件或**空**目录；不存在报错 |
| `fs.rename(from, to)` | 改名或移动；跨设备移动由宿主决定是否支持 |

* `fs.remove` 不递归删除非空目录——要删整棵树请自行遍历。

## 6. 错误

| 情形 | 类别 |
|---|---|
| 参数类型不符 | `type` |
| 文件或目录不存在（`exists`/`stat` 以外） | `error` |
| 权限不足、磁盘错误等 IO 失败 | `error` |

* 消息格式：`fs: <操作> <路径>: <系统原因>`，例如 `fs: read a.txt: No such file or directory`。
* `exists` 与 `stat` 不因「不存在」抛错，这是它们存在的意义。

## 7. 用例（预期行为）

```jua
let fs = require 'fs'

fs.write('tmp.txt', 'hello')
ok(fs.exists('tmp.txt'))
eq(fs.read('tmp.txt'), 'hello', 'round trip')

fs.append('tmp.txt', ' world')
eq(fs.read('tmp.txt'), 'hello world', 'append')

let st = fs.stat('tmp.txt')
eq(st.type, 'file', 'stat type')
eq(st.size, 11, 'stat size')
eq(fs.stat('nope.txt'), null, 'missing path gives null')

eq(fs.exists('nope.txt'), false, 'exists on missing path')

fs.mkdir('tmp/a/b')
ok(fs.stat('tmp/a/b').type == 'directory', 'mkdir creates parents')
fs.write('tmp/a/b/f.txt', 'x')
eq(fs.list('tmp/a'), ['b'], 'list directory')

fs.rename('tmp.txt', 'tmp2.txt')
eq(fs.exists('tmp.txt'), false, 'rename moves away')
fs.remove('tmp2.txt')
eq(fs.exists('tmp2.txt'), false, 'remove deletes')

throws(fun(){ fs.read('nope.txt') }, 'read on missing file throws')
throws(fun(){ fs.remove('tmp/a') }, 'remove refuses non-empty directory')
```

## 8. 需要拍板的点

1. **`fs.mkdir` 是否逐级创建**：本文建议逐级（`mkdir -p`），已存在不报错。另一种是只建一级、已存在报错（更接近系统调用）。
2. **`fs.remove` 是否允许递归删除**：本文建议不允许，避免误删整棵树。
3. **`fs.stat` 的时间字段**：用毫秒数字（本文建议，与 `date` 配合），还是直接给 `date` 实例（会让 fs 依赖 date 模块）。
4. **是否需要 `fs.cwd()` / `fs.chdir()`**：本文暂不提供——改变进程工作目录会影响模块查找。
5. **`fs.list` 的返回**：只给名字（本文建议），还是给 `{name, type}` 对象（多一次 stat，慢但有用）。
6. **符号链接**：跟随还是保留？本文暂不特殊处理，交给宿主。

## 9. 推进顺序

1. 定下 §8 的六点，把本文并入 `juacpp/README.md` 的模块一节，删除本文件。
2. 实现 `m-fs.cpp`，用 `std::filesystem`；路径按 UTF-8 → 宽字符转换（Windows）。
3. 新增 `test/fs.jua`：用临时目录做读写、追加、目录、改名、删除与错误路径。
4. 测试要能清理自己创建的临时文件，避免污染仓库。

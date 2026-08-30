# Lua 直接调用 SMA — amxx2_call_pawn / amxx2_call_native

在 lua 模块中实现（`luaMngr.cpp` → `LuaCallPawnOrNative`），Lua 脚本可**直接调用任意
已加载 SMA 插件的 public 函数和 native**，无需 function.txt、无需
`lua_register_function` 中转注册、无需第三方插件。

- `amxx2_call_pawn` — 调用插件的 **public 函数**（`amx_Exec`）
- `amxx2_call_native` — 调用 **native**（核心 / 模块 / `register_native` 注册的均可；
  从插件 natives 表取 C 函数指针直调，register_native 的动态桩会正常路由到提供方插件）

## amxx2_call_native 语法

```lua
local ret, ref1, ... = amxx2_call_native(name, spec, ...)          -- 自动跨插件搜索调用上下文
local ret, ref1, ... = amxx2_call_native(plugin, name, spec, ...)  -- 限定插件（.amxx 后缀识别，可传 nil）
```

spec 与返回值规则和 `amxx2_call_pawn` 完全一致（见下）。

> **注意**：native 必须真实存在于某个已加载插件的 natives 表中——即该插件编译时
> `native` 声明过它（Pawn 编译器会裁剪未被引用的 native 声明）。核心/模块 native
> 几乎每个插件都有；`register_native` 的 native 需要有插件声明（消费方插件本来就
> 要声明，否则它自己也调不了）。
>
> 带长度/计数参数的 native（如 `get_mapname(name[], len)`），长度参数需显式传递：
> `amxx2_call_native("get_mapname", "v:Si", "", 63)`。
>
> **可选参数必须传全**：Pawn 插件内调用时编译器会自动为缺省参数压栈，Lua 直调没有
> 这个环节。部分核心 native 不检查参数个数直接读可选槽（如 `get_players` 无条件把
> `params[3]` 当 flags 字符串地址），漏传会读到随机值，轻则行为错误重则访问冲突
> 崩溃。模块已将未填充槽位置 0 兜底（不会野指针），但行为正确性靠传全参数保证：
> ```lua
> amxx2_call_native("get_players", "v:AIs", "")      -- flags 空串不能省
> amxx2_call_native("get_playersnum", "i:i", 0)      -- 核心会读 params[1]
> ```
>
> **可变参数 native（`...` 部分）**：按 Pawn ABI，`...` 后的实参一律以**引用**压栈，
> 核心 `atcprintf` 对每个格式符都做解引用。因此 `%d` 的值要用 `I` 传（不是 `i`），
> `%f` 用 `F`，`%s` 仍用 `s`（字符串本来就是地址）：
> ```lua
> -- formatex(buf, len, "HP:%d/%d", 75, 100)
> local _, s = amxx2_call_native("formatex", "v:SisII", "", 63, "HP:%d/%d", 75, 100)
> -- client_print(0, print_console, "round %d", n)
> amxx2_call_native("client_print", "v:iisI", 0, 2, "round %d", n)
> ```

```lua
-- 核心 native
amxx2_call_native("server_print", "v:s", "hello from lua")
local _, map = amxx2_call_native("get_mapname", "v:Si", "", 63)
local n = amxx2_call_native("get_playersnum", "i:")
local r = amxx2_call_native("random_num", "i:ii", 5, 10)

-- register_native（由别的插件注册）
local money = amxx2_call_native("cs_get_user_money", "i:i", id)
local ok = amxx2_call_native("zp50_main.amxx", "zp_infect_user", "i:iii", id, 0, 0)
```

## 语法

```lua
local ret, ref1, ref2, ... = amxx2_call_pawn(plugin, func, spec, ...)
```

| 参数 | 说明 |
|---|---|
| `plugin` | 插件文件名，如 `"zp50_main.amxx"`；传 `nil` 或 `""` 表示在**所有已加载插件**中搜索该函数（取第一个匹配） |
| `func` | public 函数名 |
| `spec` | 类型描述串，格式 `"[ret]:args"`，见下 |
| `...` | 与 `args` 一一对应的调用参数 |

**返回值**：成功时第一个返回值为函数返回值（按 `ret` 类型转换），之后按 `spec`
声明顺序追加引用参数（`I`/`F`/`S`）的输出值；失败返回 `nil, 错误信息`。

## spec 描述串

- 无冒号：整个串视为参数列表，返回类型为 `i`
- `"f:"` 这类左侧字符为返回类型；`""` 表示无参数、整数返回

### 返回类型（ret）

| 字符 | 含义 |
|---|---|
| `i` | 整数（默认） |
| `f` | 浮点 |
| `v` | 忽略返回值（返回 `true`） |

### 参数类型（args，每个字符对应一个实参）

| 字符 | 含义 | Lua 实参 |
|---|---|---|
| `i` | 整数 | number |
| `b` | 布尔 → cell 0/1 | boolean |
| `f` | 浮点 | number |
| `s` | 字符串（传入，unpacked） | string |
| `v` | 向量 → `Float[3]` | `{x, y, z}` table |
| `a` | 整数数组 | table |
| `n` | 浮点数组 | table |
| `I` | 整数引用（进/出），输出追加到返回值 | number（初始值） |
| `F` | 浮点引用（进/出），输出追加到返回值 | number（初始值） |
| `S` | 字符串缓冲（进/出，1024 字节），输出追加到返回值 | string（初始内容，可省略） |

## 示例

```lua
-- 整数加法: public test_add(a, b) { return a + b; }
local sum = amxx2_call_pawn("test.amxx", "test_add", "i:ii", 40, 2)  --> 42

-- 浮点: public Float:test_mul(Float:a, Float:b)
local m = amxx2_call_pawn("test.amxx", "test_mul", "f:ff", 2.5, 4.0)  --> 10.0

-- 字符串: public test_echo(const s[])
local len = amxx2_call_pawn("test.amxx", "test_echo", "i:s", "hello") --> 5

-- 引用输出: public test_byref(a_ref[], Float:f_ref[])
local ret, a, f = amxx2_call_pawn("test.amxx", "test_byref", "i:IF", 7, 1.5)

-- 字符串缓冲输出: public get_name(buf[]) { copy(buf, 1023, "pawn") }
local _, name = amxx2_call_pawn("test.amxx", "get_name", "v:S", "")

-- 向量: public Float:test_vecsum(Float:v[3])
local s = amxx2_call_pawn("test.amxx", "test_vecsum", "f:v", {1.0, 2.0, 3.0})

-- 数组: public test_sum(const arr[], n)
local t = amxx2_call_pawn("test.amxx", "test_sum", "i:ai", {1,2,3}, 3)

-- 跨插件搜索: 不知道函数在哪个插件里时
local r = amxx2_call_pawn(nil, "zp_get_user_zombie", "i:i", playerId)

-- 错误处理
local r, err = amxx2_call_pawn("no.amxx", "x", "i:")
if r == nil then amxx2_print(err) end
```

## 实现要点（模块侧）

- 通过 `MF_GetScriptAmx` / `MF_GetScriptName` 遍历已加载插件（文件名按 basename
  不区分大小写匹配），`MF_AmxFindPublic` 定位 public 函数
- 参数按 Pawn 调用约定**逆序**压栈（`MF_AmxPush`）；字符串/数组/向量/引用通过
  `MF_AmxAllot` 在目标插件堆上分配（unpacked 格式，与核心 callfunc 一致）
- `MF_AmxExec` 执行；执行错误时复位 `amx->error`（与核心 forward 行为一致，避免
  插件被标记为出错状态）
- 调用前后保存/恢复 `amx->hea`，等价 `amx_Release` 释放全部临时分配，**无堆泄漏**
- 单次调用最多 32 个参数；`S` 缓冲固定 1024 字节

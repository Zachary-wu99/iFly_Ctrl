# iFly Shell 学习与使用指南

本文档针对 `D:\personal\iFly_Ctrl_mavlink_qgc\iFly\lib\shell` 下的 `shell.hpp` 和 `shell.cpp` 编写。目标不是只解释函数名，而是让你能按步骤理解这个 Shell 模块如何被接入、如何运行、如何注册命令、如何注册函数、如何注册参数，以及遇到问题时怎样定位。

## 1. 先知道这个模块解决什么问题

这个目录实现了一个面向嵌入式设备的交互式命令行 Shell。

它的作用是把串口、USB CDC、MAVLink 透传等字节流包装成一个可交互终端。用户连上终端后，可以看到横幅、按激活键、输入密码、输入命令，然后 Shell 根据输入执行内置命令、用户注册命令、用户注册函数，或者读写注册参数。

这个 Shell 模块本身不关心底层到底是 UART、USB 还是别的链路。它只需要两类能力：

1. 能判断链路是否连接。
2. 能读入字节、写出字节。

这两类能力可以来自 `SerialIoBase`，也可以来自一个直接输出回调加 `ProcessInput()` 的手动输入通道。

## 2. 目录内两个文件分别负责什么

`shell.hpp` 是接口文件。你从外部使用 Shell 时主要看这个文件。

它定义了：

1. `class Shell`。
2. 注册命令用的 `Command`。
3. 注册函数用的 `Function`。
4. 注册参数用的 `Parameter`。
5. 命令、函数、参数回调类型。
6. 会话配置接口，比如 `SetPrompt()`、`SetPassword()`、`SetActivationKey()`。
7. IO 接口，比如 `BindIo()`、`SetOutput()`、`SetConnected()`、`ProcessInput()`、`Poll()`。

`shell.cpp` 是实现文件。你想理解 Shell 内部为什么这样工作时看这个文件。

它实现了：

1. 会话状态机。
2. 串口轮询。
3. 字节级输入处理。
4. 回车、退格、普通字符处理。
5. 密码登录。
6. 命令行拆词。
7. 内置命令。
8. 用户命令、函数、参数的查找和调用。
9. 文本输出和格式化输出。

## 3. 先从最小使用方式学起

最小使用分为两种模式。

第一种是绑定 `SerialIoBase`，适合 Shell 自己从底层 IO 对象读写。

```cpp
#include "shell.hpp"

iFly::Shell shell;

void Init()
{
  shell.BindIo(&your_serial_io);
  shell.SetPrompt("ifly> ");
  shell.SetPassword("ifly");
}

void Loop()
{
  shell.Poll();
}
```

这段代码的每一步含义如下：

1. 创建 `Shell shell`。
2. 调用 `BindIo(&your_serial_io)`，把一个继承自 `SerialIoBase` 的对象交给 Shell。
3. 调用 `SetPrompt("ifly> ")` 设置命令提示符。
4. 调用 `SetPassword("ifly")` 设置登录密码。
5. 在主循环中不断调用 `Poll()`。
6. `Poll()` 每次会检查 IO 是否连接。
7. 如果刚刚连接，它会打印横幅并进入登录流程。
8. 如果有收到字节，它会逐字节送入 Shell 状态机。

第二种是直接输出回调模式，适合外层已经有自己的收发通道，只想让 Shell 处理输入内容。

```cpp
#include "shell.hpp"

iFly::Shell shell;

uint32_t ShellOutput(void *context, const uint8_t *data, uint32_t length)
{
  // 把 data[0..length-1] 发送到你的链路。
  // 返回实际发送成功的字节数。
  return YourTransportWrite(data, length);
}

void Init()
{
  shell.SetOutput(&ShellOutput, nullptr);
  shell.SetConnected(true);
}

void OnBytesReceived(const uint8_t *data, uint32_t length)
{
  shell.ProcessInput(data, length);
}
```

这段代码的每一步含义如下：

1. `SetOutput()` 不绑定 `SerialIoBase`，而是设置一个输出回调。
2. `SetConnected(true)` 告诉 Shell 当前直接通道已经连接。
3. Shell 第一次感知到连接后，会启动新会话。
4. 外部收到输入字节后，主动调用 `ProcessInput()`。
5. `ProcessInput()` 不会自己读底层硬件，它只处理你传给它的字节数组。

实际工程里的 `FlightCtrlCli` 使用的就是这类思路。它封装了一个 `Shell shell_`，对外提供 `SetOutput()`、`SetConnected()`、`ProcessInput()`，再把调用转给内部 Shell。

## 4. 这个 Shell 的完整启动流程

下面按执行顺序说明一次新会话从无到可输入命令的完整过程。

### 第 1 步：构造 Shell

构造函数是：

```cpp
Shell::Shell()
{
  ClearPassword();
  ResetSession();
}
```

构造时发生两件事：

1. `ClearPassword()` 把 `password_[0]` 设为 `'\0'`，表示默认没有密码。
2. `ResetSession()` 把会话状态重置为未连接。

默认情况下 Shell 没有绑定 IO，没有输出回调，也没有密码。

### 第 2 步：绑定输入输出方式

你必须在 `BindIo()` 和 `SetOutput()` 两种方式中选一种。

`BindIo(SerialIoBase *io)` 的特点：

1. 让 Shell 通过 `io_->IsConnected()` 判断连接状态。
2. 让 Shell 通过 `io_->Read()` 读取输入。
3. 让 Shell 通过 `io_->Write()` 输出内容。
4. 使用时必须周期性调用 `Poll()`。

`SetOutput(OutputHandler output, void *context)` 的特点：

1. 只设置输出方向。
2. 输入方向由外部调用 `ProcessInput()` 主动喂给 Shell。
3. 连接状态由外部调用 `SetConnected()` 主动设置。
4. 适合被更高层通信协议或测试代码包起来使用。

注意：调用 `BindIo()` 会清掉直接输出回调；调用 `SetOutput()` 会清掉绑定的 `io_`。这两个模式不要混用。

### 第 3 步：配置提示符、横幅、密码、激活键

常用配置如下：

```cpp
shell.SetBanner("iFly Flight Controller",
                "iFly flight controller CLI | transport=mavlink");
shell.SetPrompt("iFly> ");
shell.SetPassword("ifly");
shell.SetActivationKey(' ', "Press SPACE to enter the iFly secure terminal.");
```

每个配置的效果如下：

1. `SetBanner(title, subtitle)` 设置连接后显示的标题和副标题。
2. `SetPrompt(prompt)` 设置登录成功后的命令提示符。
3. `SetPassword(password)` 设置密码。设置非空密码后，用户必须登录。
4. `ClearPassword()` 清空密码。清空后不需要登录。
5. `SetActivationKey(key, promptText)` 设置激活键。比如传入 `' '` 表示用户必须先按空格。
6. `DisableActivationKey()` 关闭激活键机制。

重要细节：

1. `SetPassword()` 会把密码复制到 `password_` 内部缓冲区。
2. `SetBanner()`、`SetPrompt()`、`SetActivationKey()` 的文本指针只是保存指针，不复制字符串内容。
3. 因此传给横幅、提示符、激活提示的字符串必须在 Shell 使用期间一直有效。
4. 不要传临时局部数组给这些接口，除非这个数组的生命周期长于 Shell。

### 第 4 步：连接建立后启动会话

会话真正开始于 `StartConnectedSession()`。

如果你用 `BindIo()`，`Poll()` 发现 `io_->IsConnected()` 从不可用变成可用时会调用它。

如果你用直接输出模式，`SetConnected(true)` 或 `ProcessInput()` 发现连接尚未激活时会调用它。

`StartConnectedSession()` 做这些事情：

1. 标记 `connectionActive_ = true`。
2. 清空当前输入行。
3. 打印横幅。
4. 如果配置了激活键，进入 `kActivationPrompt` 状态并打印激活提示。
5. 如果没有激活键但配置了密码，进入 `kPasswordPrompt` 状态并打印 `Password: `。
6. 如果既没有激活键也没有密码，进入 `kReady` 状态并打印命令提示符。

也就是说，可能出现三条路径。

有激活键、有密码：

```text
连接建立
打印横幅
等待激活键
播放会话动画
等待密码
进入命令状态
```

无激活键、有密码：

```text
连接建立
打印横幅
等待密码
进入命令状态
```

无激活键、无密码：

```text
连接建立
打印横幅
直接进入命令状态
```

### 第 5 步：用户输入被逐字节处理

无论输入来自 `Poll()` 还是 `ProcessInput()`，最后都会进入 `ProcessByte(uint8_t byteValue)`。

它对每个字节按当前状态处理：

1. 如果在 `kActivationPrompt` 状态，只接受激活键。其他字节直接忽略。
2. 如果在 `kSessionAnimation` 状态，普通输入字节直接忽略。
3. 如果正在抑制 CRLF 中的 LF，并且当前字节是 `'\n'`，则丢掉这个 LF。
4. 如果当前字节是 `'\r'` 或 `'\n'`，表示一行输入结束。
5. 如果当前字节是退格 `0x08` 或删除 `0x7F`，删除输入行最后一个字符。
6. 如果当前字节小于 32 或大于 126，忽略。
7. 如果输入行已满，忽略后续字符。
8. 否则把这个字节追加到 `inputLine_`。

这里有几个必须记住的限制：

1. 只接受 ASCII 可打印字符，范围是 32 到 126。
2. 中文、方向键、功能键、Escape 序列不会作为命令内容处理。
3. 输入行缓冲区大小是 `kInputLineBufferSize = 128`，实际最多输入 127 个字符。
4. 密码输入时，终端只回显 `*`。
5. 普通命令输入时，终端回显原字符。
6. 收到 `'\r'` 后会抑制紧跟着的 `'\n'`，所以 Windows 常见的 CRLF 不会触发两次回车。

### 第 6 步：一行输入结束后分发

当收到回车或换行时，`HandleCompletedLine()` 执行。

它的步骤是：

1. 先输出一个换行。
2. 如果当前状态是 `kPasswordPrompt`，调用 `HandlePasswordLine()`。
3. 如果当前状态是 `kReady`，调用 `HandleCommandLine()`。
4. 清空输入行。
5. 如果连接还有效，并且还在密码状态，重新打印 `Password: `。
6. 如果连接还有效，并且在命令状态，重新打印提示符。

密码行处理很直接：

1. 比较 `inputLine_` 和 `password_`。
2. 如果一致，状态变成 `kReady`，输出 `Login successful.`。
3. 如果不一致，输出 `Password incorrect.`，状态保持 `kPasswordPrompt`。

命令行处理分三层：

1. 先调用 `Tokenize()` 拆参数。
2. 再调用 `ExecuteBuiltin()` 尝试执行内置命令。
3. 如果不是内置命令，再调用 `ExecuteCommand()` 尝试执行用户注册命令。
4. 如果仍然找不到，输出 `Unknown command: <name>`。

## 5. 会话状态机逐个解释

Shell 内部状态是 `SessionState`。

`kDisconnected`：

表示没有连接。`ResetSession()`、底层断开、`SetConnected(false)` 都会回到这个状态。

`kActivationPrompt`：

表示已经连接，但还没按激活键。这个状态只等待 `activation_key_`。

`kSessionAnimation`：

表示激活键已按下，正在运行会话动画。动画回调返回 `true` 后进入下一阶段。

`kPasswordPrompt`：

表示等待密码输入。这个状态下输入字符显示为 `*`。

`kReady`：

表示已经可以执行命令。`IsLoggedIn()` 只有在这个状态才返回 `true`。

状态推进关系如下：

```text
kDisconnected
  |
  | 连接建立
  v
kActivationPrompt  如果配置了激活键
  |
  | 收到激活键
  v
kSessionAnimation  如果配置了动画
  |
  | 动画完成
  v
kPasswordPrompt    如果配置了密码
  |
  | 密码正确
  v
kReady
```

如果没有激活键，会跳过 `kActivationPrompt`。如果没有动画，会跳过 `kSessionAnimation`。如果没有密码，会跳过 `kPasswordPrompt`。

## 6. 内置命令完整说明

内置命令优先级高于用户注册命令。也就是说，如果你注册了一个名字叫 `help` 的用户命令，用户输入 `help` 时仍然会执行内置 `help`。

### help 和 ?

命令：

```text
help
?
help commands
help functions
help params
```

行为：

1. `help` 和 `?` 打印内置命令摘要。
2. `help commands` 打印用户注册的普通命令列表。
3. `help functions` 打印用户注册的函数列表。
4. `help params` 打印用户注册的参数列表和值。

注意：`help commands` 不打印内置命令列表，它打印的是 `RegisterCommand()` 注册进去的普通命令。

### clear

命令：

```text
clear
```

行为：

1. 输出 ANSI 清屏序列 `ESC[2J`。
2. 输出 ANSI 光标回到左上角序列 `ESC[H`。

终端支持 ANSI 控制序列时，屏幕会被清空。

### logout

命令：

```text
logout
```

行为：

1. 如果当前没有启用密码，输出 `Password login is disabled.`。
2. 如果启用了密码，状态回到 `kPasswordPrompt`。
3. 下一次输入必须重新输入密码。

### passwd

命令：

```text
passwd <old_password> <new_password>
```

行为：

1. 如果没有启用密码，输出 `Password login is disabled.`。
2. 如果参数数量不是 3 个，输出用法。
3. 如果旧密码不匹配，输出 `Current password is incorrect.`。
4. 如果旧密码正确，调用 `SetPassword(argv[2])` 更新密码。

注意：新密码最大长度由 `kMaxPasswordLength = 31` 限制，超过部分会被截断。

### func

命令：

```text
func list
func call <name> [args...]
```

行为：

1. `func` 或 `func list` 打印函数列表。
2. `func call <name> [args...]` 查找函数并执行。
3. 函数不存在时输出 `Unknown function: <name>`。
4. 函数回调返回 `false` 时输出 `Function failed: <name>`。

传给函数回调的 `argc` 和 `argv` 只包含 `[args...]`，不包含 `func`、`call`、函数名。

### call

命令：

```text
call <name> [args...]
```

行为：

1. 这是 `func call` 的短写。
2. 查找 `RegisterFunction()` 注册的函数。
3. 找到后调用函数回调。

传给函数回调的 `argc` 和 `argv` 只包含 `[args...]`，不包含 `call` 和函数名。

### param

命令：

```text
param list
param print
param get <name>
param set <name> <value>
```

行为：

1. `param`、`param list`、`param print` 打印全部参数。
2. `param get <name>` 打印指定参数当前值。
3. `param set <name> <value>` 设置指定参数，然后打印新值。
4. 参数不存在时输出 `Unknown parameter: <name>`。
5. 参数没有 setter 时输出 `Parameter is read-only: <name>`。
6. setter 返回 `false` 时输出 `Parameter set failed: <name>`。

### get

命令：

```text
get <name>
```

行为：

这是 `param get <name>` 的短写。

### set

命令：

```text
set <name> <value>
```

行为：

这是 `param set <name> <value>` 的短写。

如果值中有空格，需要用双引号：

```text
set user.name "flight controller"
```

### params 和 print

命令：

```text
params
print
```

行为：

1. 两者都会打印全部参数。
2. `print` 在这里不是通用打印命令，它只是参数列表别名。

## 7. 命令行拆词规则

命令行由 `Tokenize(char *line, const char *argv[])` 拆成参数。

规则如下：

1. 分隔符只有空格 `' '` 和制表符 `'\t'`。
2. 多个连续空白会被当成一个分隔区域。
3. 双引号可以把包含空格的内容合成一个参数。
4. 双引号本身不会进入参数内容。
5. 不支持反斜杠转义。
6. 不支持单引号。
7. 不支持环境变量展开。
8. 不支持管道、重定向、通配符。
9. 最多拆出 `kMaxArgumentCount = 10` 个参数。
10. 超过 10 个之后的参数会被忽略。

示例：

```text
输入: set sys.loop_hz 1000
拆成: argv[0]="set", argv[1]="sys.loop_hz", argv[2]="1000"

输入: set device.name "iFly Controller"
拆成: argv[0]="set", argv[1]="device.name", argv[2]="iFly Controller"

输入: call status
拆成: argv[0]="call", argv[1]="status"
函数回调实际收到 argc=0
```

注意：`Tokenize()` 会修改原始 `inputLine_`，把分隔位置改成 `'\0'`。所以拆词后 `argv` 指向的是 `inputLine_` 内部的不同片段，不是新分配的字符串。

## 8. 三种可扩展入口

Shell 提供三种扩展入口。

第一种是普通命令 `Command`。

普通命令像内置命令一样直接输入命令名执行。例如你注册了 `dump`，用户输入 `dump all` 就会执行它。

第二种是函数 `Function`。

函数必须通过 `call <name>` 或 `func call <name>` 执行。例如注册 `status` 后，用户输入 `call status`。

第三种是参数 `Parameter`。

参数通过 `get`、`set`、`param get`、`param set` 访问。例如注册 `sys.loop_hz` 后，用户输入 `get sys.loop_hz` 或 `set sys.loop_hz 1000`。

这三种入口的区别如下：

| 类型 | 注册接口 | 用户输入 | 适合场景 |
| --- | --- | --- | --- |
| 普通命令 | `RegisterCommand()` | `name [args...]` | 自定义命令语法 |
| 函数 | `RegisterFunction()` | `call name [args...]` | 简单动作、诊断函数、复位函数 |
| 参数 | `RegisterParameter()` | `get/set/param` | 读写配置值或运行状态 |

## 9. 如何注册一个普通命令

普通命令结构如下：

```cpp
struct Command final {
  const char *name;
  const char *help;
  CommandHandler handler;
  void *context;
};
```

回调类型如下：

```cpp
bool (*CommandHandler)(Shell *shell,
                       void *context,
                       uint8_t argc,
                       const char *const *argv);
```

每个参数的意义：

1. `shell` 是当前 Shell 对象，可以用它输出文本。
2. `context` 是注册时传进去的上下文指针。
3. `argc` 是参数数量。
4. `argv` 是参数数组。
5. 对普通命令来说，`argv[0]` 是命令名本身。

示例：

```cpp
struct DumpContext {
  uint32_t counter;
};

bool DumpCommand(iFly::Shell *shell,
                 void *context,
                 uint8_t argc,
                 const char *const *argv)
{
  DumpContext *dump = reinterpret_cast<DumpContext *>(context);

  if ((shell == nullptr) || (dump == nullptr)) {
    return false;
  }

  if (argc != 2U) {
    shell->WriteLine("Usage: dump <name>");
    return false;
  }

  shell->Printf("dump target=%s counter=%lu\r\n",
                argv[1],
                static_cast<unsigned long>(dump->counter));
  return true;
}

DumpContext dump_context {};

void RegisterShellCommands(iFly::Shell &shell)
{
  const bool ok = shell.RegisterCommand(
      {"dump", "dump named diagnostic data", &DumpCommand, &dump_context});

  if (!ok) {
    // 注册失败通常是 name 为空、handler 为空，或者命令表满了。
  }
}
```

注册成功后，用户可以输入：

```text
dump imu
```

Shell 执行步骤如下：

1. `HandleCommandLine()` 调用 `Tokenize()`。
2. 得到 `argv[0] = "dump"`，`argv[1] = "imu"`。
3. `ExecuteBuiltin()` 发现它不是内置命令。
4. `ExecuteCommand()` 调用 `FindCommand("dump")`。
5. 找到后调用 `DumpCommand(shell, context, 2, argv)`。
6. 如果回调返回 `true`，Shell 不额外输出错误。
7. 如果回调返回 `false`，Shell 输出 `Command failed: dump`。

普通命令注意事项：

1. `RegisterCommand()` 只是把 `Command` 结构浅拷贝到内部数组。
2. `name`、`help`、`context` 指针必须保持有效。
3. Shell 不检查重复命令名。
4. 如果注册两个同名命令，`FindCommand()` 会找到第一个。
5. 内置命令优先于普通命令。
6. 普通命令最多注册 `kMaxCommandCount = 12` 个。

## 10. 如何注册一个函数

函数结构如下：

```cpp
struct Function final {
  const char *name;
  const char *help;
  FunctionHandler handler;
  void *context;
};
```

回调类型如下：

```cpp
bool (*FunctionHandler)(Shell *shell,
                        void *context,
                        uint8_t argc,
                        const char *const *argv);
```

函数和普通命令最大的区别是调用方式。

普通命令直接输入命令名：

```text
dump imu
```

函数必须通过 `call` 或 `func call`：

```text
call status
func call status
```

函数回调收到的 `argv` 不包含函数名，只包含函数名后面的参数。

示例：

```cpp
bool StatusFunction(iFly::Shell *shell,
                    void *context,
                    uint8_t argc,
                    const char *const *argv)
{
  (void)context;
  (void)argv;

  if (shell == nullptr) {
    return false;
  }

  if (argc != 0U) {
    shell->WriteLine("Usage: call status");
    return false;
  }

  shell->WriteLine("System status:");
  shell->WriteLine("  link: connected");
  shell->WriteLine("  mode: debug");
  return true;
}

void RegisterShellFunctions(iFly::Shell &shell)
{
  (void)shell.RegisterFunction(
      {"status", "print system status", &StatusFunction, nullptr});
}
```

用户输入：

```text
call status
```

执行步骤：

1. `ExecuteBuiltin()` 识别 `call`。
2. `HandleFunctionCallAlias()` 检查参数数量。
3. `FindFunction("status")` 查找函数。
4. 找到后调用 `StatusFunction(shell, context, 0, argv + 2)`。
5. 因为没有额外参数，所以函数回调看到 `argc = 0`。

函数注册注意事项：

1. `RegisterFunction()` 要求 `name` 和 `handler` 非空。
2. 最多注册 `kMaxFunctionCount = 16` 个。
3. 不检查重名。
4. 重名时查找第一个。
5. 函数适合做动作，不适合表达复杂命令语法。

## 11. 如何注册一个参数

参数结构如下：

```cpp
struct Parameter final {
  const char *name;
  const char *help;
  ParameterGetter getter;
  ParameterSetter setter;
  void *context;
};
```

getter 类型如下：

```cpp
bool (*ParameterGetter)(void *context,
                        char *buffer,
                        uint32_t bufferSize);
```

setter 类型如下：

```cpp
bool (*ParameterSetter)(void *context,
                        const char *value);
```

getter 的职责：

1. 根据 `context` 找到真实参数。
2. 把参数当前值格式化为字符串。
3. 写入 `buffer`。
4. 确保字符串以 `'\0'` 结束。
5. 成功返回 `true`，失败返回 `false`。

setter 的职责：

1. 根据 `context` 找到真实参数。
2. 解析用户输入的 `value`。
3. 校验范围。
4. 写入真实参数。
5. 成功返回 `true`，失败返回 `false`。

如果 `setter` 是 `nullptr`，这个参数就是只读参数。

示例：

```cpp
struct LoopHzContext {
  uint32_t value;
};

bool GetLoopHz(void *context, char *buffer, uint32_t bufferSize)
{
  LoopHzContext *ctx = reinterpret_cast<LoopHzContext *>(context);
  if ((ctx == nullptr) || (buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  const int written = snprintf(buffer,
                               bufferSize,
                               "%lu",
                               static_cast<unsigned long>(ctx->value));
  return (written > 0) && (static_cast<uint32_t>(written) < bufferSize);
}

bool SetLoopHz(void *context, const char *value)
{
  LoopHzContext *ctx = reinterpret_cast<LoopHzContext *>(context);
  if ((ctx == nullptr) || (value == nullptr)) {
    return false;
  }

  char *end = nullptr;
  const unsigned long parsed = strtoul(value, &end, 10);
  if ((end == value) || (*end != '\0')) {
    return false;
  }

  if ((parsed < 50UL) || (parsed > 4000UL)) {
    return false;
  }

  ctx->value = static_cast<uint32_t>(parsed);
  return true;
}

LoopHzContext loop_hz {1000U};

void RegisterShellParameters(iFly::Shell &shell)
{
  (void)shell.RegisterParameter(
      {"sys.loop_hz",
       "control loop frequency",
       &GetLoopHz,
       &SetLoopHz,
       &loop_hz});
}
```

用户输入：

```text
get sys.loop_hz
set sys.loop_hz 500
params
```

执行 `get sys.loop_hz` 的步骤：

1. `ExecuteBuiltin()` 识别 `get`。
2. `HandleParameterGetAlias()` 要求 `argc == 2`。
3. `FindParameter("sys.loop_hz")` 查找参数。
4. `PrintParameterValue()` 调用 getter。
5. getter 把值写入本地 `valueBuffer`。
6. Shell 输出 `sys.loop_hz = 1000 : control loop frequency`。

执行 `set sys.loop_hz 500` 的步骤：

1. `ExecuteBuiltin()` 识别 `set`。
2. `HandleParameterSetAlias()` 要求 `argc == 3`。
3. `FindParameter("sys.loop_hz")` 查找参数。
4. 如果 setter 为空，报只读。
5. 如果 setter 非空，调用 `SetLoopHz(context, "500")`。
6. setter 解析并校验范围。
7. setter 写入真实值并返回 `true`。
8. Shell 再调用 getter 打印新值。

参数注册注意事项：

1. `RegisterParameter()` 要求 `name` 和 `getter` 非空。
2. `setter` 可以为空，表示只读。
3. 最多注册 `kMaxParameterCount = 24` 个。
4. `PrintParameterValue()` 使用的临时值缓冲区大小是 `kValueBufferSize = 96`。
5. 参数名只是普通字符串，没有层级解析逻辑。
6. 像 `control.speed_pid.kp` 这样的点号只是命名约定。

## 12. 实际工程中的 FlightCtrlCli 是怎样接入的

工程里的 `app/cli/flight_ctrl_cli.cpp` 展示了这个 Shell 的真实用法。

初始化步骤大致是：

```cpp
void FlightCtrlCli::Init()
{
  parameter_manager_.ResetToDefaults();
  ResetIntroAnimation();
  shell_.ClearRegistrations();
  shell_.SetPrompt(kCliPrompt);
  shell_.SetPassword(ConfiguredCliPassword(parameter_manager_.Data()));
  shell_.SetActivationKey(' ', kActivationPrompt);
  shell_.SetSessionAnimation(&FlightCtrlCli::IntroAnimation, this);
  active_transport_name_ = "mavlink";
  UpdateShellBanner();
  RegisterParameters();
  RegisterFunctions();
}
```

逐步解释：

1. `parameter_manager_.ResetToDefaults()` 先把工程参数恢复为默认值。
2. `ResetIntroAnimation()` 重置 CLI 开场动画状态。
3. `shell_.ClearRegistrations()` 清空 Shell 的命令、函数、参数注册表。
4. `shell_.SetPrompt(kCliPrompt)` 把提示符设置为 `iFly> `。
5. `shell_.SetPassword(...)` 从工程参数读取 CLI 密码，如果没配置则用默认密码 `ifly`。
6. `shell_.SetActivationKey(' ', kActivationPrompt)` 设置空格为激活键。
7. `shell_.SetSessionAnimation(&FlightCtrlCli::IntroAnimation, this)` 设置开场动画回调。
8. `active_transport_name_ = "mavlink"` 记录当前传输通道名称。
9. `UpdateShellBanner()` 生成横幅副标题并调用 `shell_.SetBanner()`。
10. `RegisterParameters()` 把飞控参数注册到 Shell。
11. `RegisterFunctions()` 把 `status`、`sys.reboot` 等函数注册到 Shell。

实际用户进入 CLI 的路径是：

```text
连接建立
看到横幅
按空格
看到开场信息
输入密码 ifly
进入 iFly> 提示符
输入 help、params、call status 等命令
```

`FlightCtrlCli` 注册了很多参数，例如：

```text
control.speed_pid.kp
control.speed_pid.ki
control.angle_pid.kp
control.position_pid.kp
sys.loop_hz
sys.arm_locked
sys.transport
sys.uptime_ms
```

其中：

1. PID 和系统控制类参数是可读写的。
2. `sys.transport` 是只读的。
3. `sys.uptime_ms` 是只读的。

`FlightCtrlCli` 注册了函数：

```text
status
sys.reboot
```

用户可以这样调用：

```text
call status
call sys.reboot
```

## 13. 会话动画怎么工作

动画回调类型是：

```cpp
bool (*SessionAnimation)(Shell *shell, void *context, bool start);
```

参数含义：

1. `shell` 用于输出动画内容。
2. `context` 是注册时传入的上下文。
3. `start` 表示这次调用是不是动画开始。
4. 返回 `true` 表示动画完成。
5. 返回 `false` 表示动画还没完成，下次 `Poll()` 或 `ProcessInput()` 后继续调用。

启动过程：

1. 用户按下激活键。
2. `HandleActivationTrigger()` 把状态设为 `kSessionAnimation`。
3. 立即调用 `session_animation_(shell, context, true)`。
4. 如果这次返回 `true`，直接进入下一阶段。
5. 如果返回 `false`，保持 `kSessionAnimation`。
6. 后续每次 `Poll()` 或 `ProcessInput()` 末尾都会以 `start=false` 再调用动画。
7. 直到动画返回 `true`。

当前 `FlightCtrlCli::UpdateIntroAnimation()` 在 `start=true` 时一次性输出开场内容并返回 `true`，所以它表现为立即完成的动画。Shell 本身支持非阻塞多帧动画，只要回调在未完成时返回 `false` 即可。

## 14. 输出链路的细节

对外输出接口有三个：

```cpp
void Write(const char *text);
void WriteLine(const char *text);
void Printf(const char *format, ...);
```

区别如下：

1. `Write()` 原样输出字符串。
2. `WriteLine()` 先输出字符串，再输出 `"\r\n"`。
3. `Printf()` 使用 `vsnprintf()` 格式化到本地缓冲区，再输出。

`Printf()` 的本地缓冲区大小是 192 字节。格式化结果超过缓冲区会被截断。

真正写字节的是 `WriteBytes()`。

`WriteBytes()` 的规则：

1. 如果设置了 `output_`，调用输出回调。
2. 如果没有设置 `output_` 但有 `io_`，调用 `io_->Write()`。
3. 如果两者都没有，返回 0。
4. 如果一次写入返回 0，会重试。
5. 连续最多允许 `kMaxWriteAttempts = 16` 次零写入。
6. 写入成功后继续发送剩余字节。

输出回调必须遵守一个约定：返回值应该是实际写出的字节数，不能故意返回比请求长度更大的值。Shell 对输出回调做了上限保护，但底层 `io_->Write()` 本身也应遵守正常写接口约定。

## 15. Poll 模式的完整数据流

使用 `BindIo()` 后，主循环应不断调用：

```cpp
shell.Poll();
```

`Poll()` 内部步骤如下：

1. 如果 `io_ == nullptr`，直接返回。
2. 调用 `io_->IsConnected()`。
3. 如果未连接，清空会话并回到 `kDisconnected`。
4. 如果已连接但 `connectionActive_ == false`，调用 `StartConnectedSession()`。
5. 定义 32 字节接收缓冲区。
6. 调用 `io_->Read(rxBuffer, sizeof(rxBuffer))`。
7. 只要读到数据，就逐字节调用 `ProcessByte()`。
8. 继续读，直到 `Read()` 返回 0。
9. 如果当前是动画状态，调用动画回调推进动画。

这里依赖 `SerialIoBase` 的几个接口：

1. `IsConnected()` 判断链路可用。
2. `Read(data, len)` 从接收队列读字节。
3. `Write(data, len)` 写字节到底层链路。

`SerialIoBase::Read()` 在读取前会调用 `BeforeRead()`，派生类可以重写它，把硬件暂存区的数据搬到统一接收队列。

## 16. 直接输入模式的完整数据流

使用 `SetOutput()` 后，Shell 不会自己读输入。你要做三件事：

1. 设置输出回调。
2. 设置连接状态。
3. 收到输入时调用 `ProcessInput()`。

典型流程：

```cpp
shell.SetOutput(&ShellOutput, context);
shell.SetConnected(true);
shell.ProcessInput(reinterpret_cast<const uint8_t *>("ifly\r\n"), 6U);
```

`SetConnected(true)` 的步骤：

1. 记录 `direct_connected_ = true`。
2. 如果之前没有激活连接，调用 `StartConnectedSession()`。

`SetConnected(false)` 的步骤：

1. 记录 `direct_connected_ = false`。
2. 清掉 `connectionActive_`。
3. 状态回到 `kDisconnected`。
4. 清空输入行。
5. 关闭 CRLF 抑制标记。

`ProcessInput(data, length)` 的步骤：

1. 如果 `data == nullptr` 或 `length == 0`，直接返回。
2. 如果 `IsConnected()` 为 false，直接返回。
3. 如果连接还没激活，调用 `StartConnectedSession()`。
4. 遍历输入数组，逐字节调用 `ProcessByte()`。
5. 如果当前是动画状态，调用动画回调推进动画。

## 17. 容量限制一览

这些限制在 `shell.hpp` 中定义为 `static constexpr`。

| 常量 | 当前值 | 含义 |
| --- | ---: | --- |
| `kMaxCommandCount` | 12 | 最多普通命令数 |
| `kMaxFunctionCount` | 16 | 最多函数数 |
| `kMaxParameterCount` | 24 | 最多参数数 |
| `kMaxArgumentCount` | 10 | 单条命令最多参数个数 |
| `kInputLineBufferSize` | 128 | 输入行缓冲区大小，含结尾 `'\0'` |
| `kValueBufferSize` | 96 | 参数值格式化缓冲区大小 |
| `kMaxPasswordLength` | 31 | 最大密码长度 |

如果你要增加注册数量，直接改这些常量即可。但要记住：

1. 这些数组是 `Shell` 对象的成员。
2. 增大数量会增大每个 `Shell` 对象占用的 RAM。
3. 嵌入式环境中 RAM 通常有限，不能随意放大。

## 18. 内部成员变量怎么理解

核心成员可以分成五组。

第一组是 IO：

```cpp
SerialIoBase *io_;
OutputHandler output_;
void *output_context_;
```

含义：

1. `io_` 用于 `BindIo()` 模式。
2. `output_` 和 `output_context_` 用于直接输出模式。
3. 两种模式互斥。

第二组是连接和状态：

```cpp
SessionState sessionState_;
bool connectionActive_;
bool direct_connected_;
bool suppressNextLf_;
```

含义：

1. `sessionState_` 是当前会话状态。
2. `connectionActive_` 表示 Shell 是否已经对当前连接启动过会话。
3. `direct_connected_` 是直接输入模式下的连接标记。
4. `suppressNextLf_` 用于处理 `\r\n`，避免一次回车触发两次。

第三组是界面配置：

```cpp
const char *bannerTitle_;
const char *bannerSubtitle_;
const char *prompt_;
const char *activation_prompt_;
uint8_t activation_key_;
SessionAnimation session_animation_;
void *session_animation_context_;
```

含义：

1. 横幅标题和副标题。
2. 命令提示符。
3. 激活提示文本和激活键。
4. 会话动画回调和上下文。

第四组是输入和密码：

```cpp
char password_[kMaxPasswordLength + 1U];
char inputLine_[kInputLineBufferSize];
uint16_t inputLength_;
```

含义：

1. `password_` 保存登录密码。
2. `inputLine_` 保存当前正在输入的一行。
3. `inputLength_` 保存当前输入长度。

第五组是注册表：

```cpp
Command commands_[kMaxCommandCount];
Function functions_[kMaxFunctionCount];
Parameter parameters_[kMaxParameterCount];
uint8_t commandCount_;
uint8_t functionCount_;
uint8_t parameterCount_;
```

含义：

1. 三个固定数组保存注册项。
2. 三个计数器表示当前有效项数量。
3. `ClearRegistrations()` 只是把计数器清零，旧数组内容不会被清除，但已经不可达。

## 19. 从源码角度阅读的推荐顺序

如果你是第一次读这两个文件，建议不要从头硬读。按下面顺序更容易理解。

第一步，看 `shell.hpp` 顶部的常量和回调类型。

你要先知道 Shell 的容量限制、回调签名和注册结构。

第二步，看公开接口。

重点看：

```cpp
BindIo()
SetOutput()
SetConnected()
ProcessInput()
SetBanner()
SetPrompt()
SetPassword()
SetActivationKey()
RegisterCommand()
RegisterFunction()
RegisterParameter()
Poll()
Write()
WriteLine()
Printf()
```

第三步，看 `SessionState`。

状态机是 Shell 的骨架。先理解状态，再看实现会轻松很多。

第四步，看 `Poll()` 和 `ProcessInput()`。

这两个函数是输入进入 Shell 的两个入口。

第五步，看 `StartConnectedSession()`。

它决定连接后先显示什么、进入哪个状态。

第六步，看 `ProcessByte()`。

它决定每个输入字节如何影响输入行和状态。

第七步，看 `HandleCompletedLine()`。

它决定一整行输入结束后应该当作密码还是命令。

第八步，看 `HandleCommandLine()`、`Tokenize()`、`ExecuteBuiltin()`、`ExecuteCommand()`。

这是命令执行主干。

第九步，看各个 `HandleXxxCommand()`。

这些是内置命令的具体行为。

第十步，看 `FindCommand()`、`FindFunction()`、`FindParameter()`。

这三个函数解释了注册表查找规则。

第十一步，看 `PrintXxx()` 系列。

这些函数负责显示横幅、帮助、列表和参数值。

第十二步，看 `WriteBytes()`。

它解释输出最终如何落到底层链路。

## 20. 如何给这个 Shell 添加一个新的内置命令

通常更推荐使用 `RegisterCommand()` 添加普通命令。只有当命令是 Shell 通用能力的一部分时，才应该加成内置命令。

例如你想加一个内置命令 `version`。

需要改这些地方：

第一步，在 `shell.hpp` 的 private 函数声明区添加：

```cpp
bool HandleVersionCommand(uint8_t argc, const char *const *argv);
```

第二步，在 `shell.cpp` 的 `ExecuteBuiltin()` 里加判断：

```cpp
if (strcmp(argv[0], "version") == 0) {
  return HandleVersionCommand(argc, argv);
}
```

第三步，在 `shell.cpp` 里实现函数：

```cpp
bool Shell::HandleVersionCommand(uint8_t argc, const char *const *argv)
{
  (void)argv;

  if (argc != 1U) {
    WriteLine("Usage: version");
    return true;
  }

  WriteLine("iFly Shell version 1.0");
  return true;
}
```

第四步，在 `PrintHelpSummary()` 里补一行：

```cpp
shell->WriteLine("  version");
```

第五步，手动测试：

```text
help
version
version extra
```

确认：

1. `help` 能看到 `version`。
2. `version` 能正常输出版本。
3. `version extra` 能输出用法。

## 21. 如何排查常见问题

### 问题 1：连接后没有任何输出

按顺序检查：

1. 是否调用了 `BindIo()` 或 `SetOutput()`。
2. 如果用 `BindIo()`，是否周期性调用了 `Poll()`。
3. 如果用 `BindIo()`，`io_->IsConnected()` 是否返回 true。
4. 如果用直接模式，是否调用了 `SetConnected(true)`。
5. 输出回调或 `io_->Write()` 是否返回了正确写入字节数。
6. 终端波特率、USB CDC、透传链路是否正常。

### 问题 2：按键没有反应

按顺序检查：

1. 当前是否在 `kActivationPrompt` 状态。
2. 如果设置了激活键，是否按的是正确的键。
3. 例如 `SetActivationKey(' ', ...)` 需要按空格，不是回车。
4. 输入字节是否真的传给了 `ProcessInput()` 或被 `Poll()` 读到。
5. 字节是否是 ASCII 可打印字符。
6. 方向键、中文输入、功能键不会被当成命令字符。

### 问题 3：一直要求输入密码

按顺序检查：

1. 密码是否被 `SetPassword()` 设置过。
2. 输入密码时终端看到的是 `*`，这是正常现象。
3. 当前密码是否超过 31 个字符导致被截断。
4. 工程中是否有参数变化回调重新设置了密码。
5. `FlightCtrlCli` 中默认密码是 `ifly`，除非工程参数 `cli.password` 非空。

### 问题 4：命令总是 Unknown command

按顺序检查：

1. 命令是否是内置命令。
2. 如果是普通命令，是否已经调用 `RegisterCommand()`。
3. `RegisterCommand()` 返回值是否为 true。
4. 命令名是否完全一致，区分大小写。
5. 是否注册表已满。
6. 是否在注册后又调用了 `ClearRegistrations()`。
7. 如果你注册的是 `Function`，不能直接输入函数名，要输入 `call <name>`。

### 问题 5：call 函数失败

按顺序检查：

1. 是否调用了 `RegisterFunction()`。
2. 注册时 `name` 和 `handler` 是否非空。
3. 用户输入是否是 `call name` 或 `func call name`。
4. 函数回调是否因为参数数量不符返回了 false。
5. 回调里是否错误地以为 `argv[0]` 是函数名。函数回调收到的 `argv[0]` 是第一个实际参数。

### 问题 6：参数无法设置

按顺序检查：

1. 参数是否已注册。
2. 注册时 `setter` 是否为 `nullptr`。
3. 如果 `setter == nullptr`，参数只读，无法设置。
4. `set` 命令的值中是否有空格。若有空格，需要双引号。
5. setter 是否解析失败。
6. setter 是否范围校验失败。
7. setter 是否写入底层参数管理器失败。

### 问题 7：params 显示 `<unavailable>`

这表示 getter 返回了 false。

按顺序检查：

1. getter 的 `context` 是否有效。
2. getter 的 `buffer` 和 `bufferSize` 是否被正确处理。
3. 真实参数是否读取得到。
4. `snprintf()` 返回值是否大于 0 且小于 `bufferSize`。
5. 输出内容是否超过 `kValueBufferSize = 96`。

### 问题 8：回车执行了两次或没有执行

当前代码支持 `\r`、`\n`、`\r\n`。

如果看起来执行两次，检查：

1. 外部是否重复调用了 `ProcessInput()`。
2. 同一批输入字节是否被既送到直接模式，又被底层 `Poll()` 读到。
3. 是否混用了 `BindIo()` 和 `SetOutput()`。

如果看起来没有执行，检查：

1. 终端是否真的发送了回车或换行。
2. 输入是否卡在激活键或密码状态。
3. 输入行是否已经超过 127 字符导致后续字符被忽略。

## 22. 这个模块没有实现什么

当前 Shell 是轻量嵌入式实现，它没有实现以下功能：

1. 命令历史。
2. 上下箭头浏览历史。
3. Tab 自动补全。
4. 左右移动光标。
5. 行内插入编辑。
6. Unicode 输入。
7. 多用户权限。
8. 加密传输。
9. 密码哈希存储。
10. 命令管道和重定向。

这些不是 bug，而是当前设计取舍。它更关注固定内存、简单可靠、容易嵌入。

如果要添加命令历史，需要新增历史缓冲区、处理 ESC 序列、处理上下箭头，并修改 `ProcessByte()` 的输入编辑逻辑。

如果要添加 Tab 补全，需要在 `ProcessByte()` 中识别 `'\t'`，然后根据当前输入匹配内置命令、普通命令、函数名或参数名。

## 23. 内存和生命周期注意事项

这个模块没有动态分配内存，所有核心存储都在 `Shell` 对象内部。

优点：

1. 行为可预测。
2. 不依赖堆。
3. 适合 MCU。
4. 注册表容量固定，RAM 占用可估算。

需要注意：

1. 注册项是浅拷贝。
2. `name`、`help`、`context` 都是指针。
3. 这些指针指向的数据必须比注册项活得更久。
4. 不要把局部变量地址作为长期 `context` 注册进去。
5. 不要把临时拼接出来的局部字符串作为 `name` 或 `help` 注册进去。
6. `ClearRegistrations()` 不会释放任何外部资源。

错误示例：

```cpp
void BadRegister(iFly::Shell &shell)
{
  char local_name[] = "bad";

  shell.RegisterCommand(
      {local_name, "bad command", &BadCommand, nullptr});
}
```

这个函数返回后，`local_name` 失效，Shell 内部保存的 `name` 指针变成悬空指针。

正确示例：

```cpp
static constexpr char kCommandName[] = "good";

void GoodRegister(iFly::Shell &shell)
{
  shell.RegisterCommand(
      {kCommandName, "good command", &GoodCommand, nullptr});
}
```

## 24. 回调返回值的约定

普通命令回调：

1. 成功完成返回 `true`。
2. 参数错误、执行失败、上下文无效时返回 `false`。
3. 返回 `false` 后 Shell 会输出 `Command failed: <name>`。

函数回调：

1. 成功完成返回 `true`。
2. 参数错误、执行失败、上下文无效时返回 `false`。
3. 返回 `false` 后 Shell 会输出 `Function failed: <name>`。

参数 getter：

1. 成功写入字符串返回 `true`。
2. 失败返回 `false`。
3. 返回 `false` 后参数显示为 `<unavailable>`。

参数 setter：

1. 成功解析、校验、写入返回 `true`。
2. 失败返回 `false`。
3. 返回 `false` 后 Shell 输出 `Parameter set failed: <name>`。

输出回调：

1. 返回实际写出的字节数。
2. 返回 0 表示暂时没有写出，Shell 会重试有限次数。
3. 不应该阻塞太久。
4. 不应该返回超过请求长度的值。

动画回调：

1. 返回 `true` 表示动画结束。
2. 返回 `false` 表示动画未结束。
3. 非阻塞动画应靠外部周期调用 `Poll()` 或 `ProcessInput()` 推进。

## 25. 如何写一个最小测试用例

因为 Shell 支持直接输出回调和直接输入，所以可以不用真实串口也能测试。

思路：

1. 用一个内存缓冲区接收输出。
2. 调用 `SetOutput()` 设置输出回调。
3. 调用 `SetConnected(true)` 模拟连接。
4. 调用 `ProcessInput()` 模拟用户输入。
5. 检查输出缓冲区内容。

示例：

```cpp
struct CaptureOutputContext {
  char buffer[4096];
  uint32_t used;
};

uint32_t CaptureOutput(void *context,
                       const uint8_t *data,
                       uint32_t length)
{
  CaptureOutputContext *capture =
      reinterpret_cast<CaptureOutputContext *>(context);
  if ((capture == nullptr) || (data == nullptr)) {
    return 0U;
  }

  uint32_t writable = sizeof(capture->buffer) - capture->used - 1U;
  if (length < writable) {
    writable = length;
  }

  memcpy(capture->buffer + capture->used, data, writable);
  capture->used += writable;
  capture->buffer[capture->used] = '\0';
  return writable;
}

void TestShellHelp()
{
  iFly::Shell shell;
  CaptureOutputContext output {};

  shell.SetOutput(&CaptureOutput, &output);
  shell.ClearPassword();
  shell.SetConnected(true);

  const char input[] = "help\r\n";
  shell.ProcessInput(reinterpret_cast<const uint8_t *>(input),
                     static_cast<uint32_t>(strlen(input)));

  // 此时 output.buffer 里应该能看到横幅、提示符和 help 输出。
}
```

如果启用了密码，测试输入要先输入密码：

```cpp
shell.SetPassword("ifly");
shell.SetConnected(true);

const char input[] = "ifly\r\nhelp\r\n";
shell.ProcessInput(reinterpret_cast<const uint8_t *>(input),
                   static_cast<uint32_t>(strlen(input)));
```

如果启用了空格激活键，测试输入要先发送空格：

```cpp
shell.SetActivationKey(' ', "Press SPACE.");
shell.SetPassword("ifly");
shell.SetConnected(true);

const char input[] = " ifly\r\nhelp\r\n";
shell.ProcessInput(reinterpret_cast<const uint8_t *>(input),
                   static_cast<uint32_t>(strlen(input)));
```

这里第一个字符是空格，用来通过激活阶段。

## 26. 修改 Shell 时的检查清单

改完源码后建议按下面顺序检查。

第一步，检查编译。

重点看：

1. 是否新增了函数声明但没实现。
2. 是否新增了实现但没在头文件声明。
3. 是否缺少 include。
4. 是否 C 和 C++ 类型转换不匹配。

第二步，检查启动流程。

测试：

```text
连接
按激活键
输入错误密码
输入正确密码
输入 help
```

第三步，检查内置命令。

测试：

```text
help
?
help commands
help functions
help params
clear
logout
passwd ifly newpass
```

第四步，检查函数。

测试：

```text
func list
call status
func call status
call missing
```

第五步，检查参数。

测试：

```text
params
get sys.loop_hz
set sys.loop_hz 500
set sys.loop_hz invalid
set sys.uptime_ms 123
```

第六步，检查输入边界。

测试：

1. 空行。
2. 超长行。
3. 超过 10 个参数。
4. 带空格的双引号参数。
5. 退格键。
6. CR、LF、CRLF 三种换行。

第七步，检查断开重连。

测试：

1. 连接后进入命令状态。
2. 调用 `SetConnected(false)` 或让 `io_->IsConnected()` 返回 false。
3. 再重新连接。
4. 确认横幅和登录流程重新出现。

## 27. 一句话总结核心模型

这个 Shell 的核心模型是：

```text
字节输入
  -> ProcessByte()
  -> 输入行
  -> Tokenize()
  -> 内置命令
  -> 用户命令 / 函数 / 参数
  -> WriteBytes()
  -> 串口或输出回调
```

理解这条链路后，你再看 `shell.hpp` 和 `shell.cpp`，每个函数的位置就很清楚了。


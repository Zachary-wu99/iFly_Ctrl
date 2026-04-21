# `paramete` 工程参数模块文档

本文档说明 `iFly/app/paramete` 目录下当前这套“工程级参数中心”的设计、实现、使用方式和扩展方法。它对应的是新引入的 `ProjectParameters` / `ProjectParameterManager` 实现，不是旧的、只偏向 Shell/CLI 的 `ParameterManager`。

## 1. 模块定位

这套代码的目标不是做“单个命令行参数表”，而是做整个飞控工程的统一参数树。

它解决的问题主要有：

1. 把散落在各模块的默认参数统一收口到一棵结构化参数树中。
2. 允许模块直接按结构体访问参数，也允许通过字符串名字访问单个参数。
3. 在 MCU 环境下避免动态内存、避免复杂反射，保持实现可控。
4. 为 CLI、调参、后续 Flash 持久化、出厂恢复等场景提供统一入口。

当前实现明确做了这些事：

- 定义一棵 `ProjectParameters` 参数树。
- 定义一张静态绑定表，把参数名映射到参数树中的内存偏移。
- 在 `ProjectParameterManager` 中把这张表注册成可查询、可读写、可挂回调的参数目录。
- 让 `FlightCtrlCli` 通过这套参数中心完成 CLI 参数读写，并在参数变化后联动 PID 和密码配置。

当前实现没有做这些事：

- 没有参数持久化到 Flash / EEPROM。
- 没有线程安全或中断并发保护。
- 没有字符串型动态参数、变长参数、嵌套容器。
- 没有自动生成 Shell 参数，CLI 仍然是手工挑选映射。

## 2. 目录与文件职责

本目录目前有 4 个核心文件：

### `project_parameters.hpp`

职责：

- 定义参数分组结构体：
  - `SystemParameters`
  - `TaskParameters`
  - `CliParameters`
  - `ControlParameters`
  - `MotorParameters`
  - `DebugParameters`
- 定义根结构 `ProjectParameters`
- 定义绑定描述结构 `ProjectParameterBinding`
- 声明默认值工厂 `MakeDefaultProjectParameters()`
- 声明静态绑定表访问函数 `GetProjectParameterBindings()`

这是“参数长什么样”的定义层。

### `project_parameters.cpp`

职责：

- 实现默认参数工厂 `MakeDefaultProjectParameters()`
- 用 `offsetof(...)` 构建参数名到内存偏移的静态绑定表 `kBindings[]`
- 提供绑定表访问函数 `GetProjectParameterBindings()`

这是“参数名字如何指向内存”的映射层。

### `project_parameter_manager.hpp`

职责：

- 定义单例类 `ProjectParameterManager`
- 暴露读写接口、查询接口、回调接口
- 通过模板 `Read<T>()` / `Write<T>()` 提供类型化访问
- 通过 `EntryView` 暴露注册表的只读视图

这是对外 API 层。

### `project_parameter_manager.cpp`

职责：

- 实现单例
- 在构造阶段初始化默认参数和注册表
- 实现 `ReadRaw()` / `WriteRaw()` / `Find()` / `Contains()` / `SetChangeHandler()`
- 在写入后执行参数更新回调

这是运行时管理层。

## 3. 整体架构

整个模块可以理解成 4 层：

```text
ProjectParameters 参数树
        |
        v
ProjectParameterBinding 静态绑定表
        |
        v
ProjectParameterManager 注册表与读写接口
        |
        v
业务模块 / CLI / 后续持久化模块
```

数据流有两种典型路径：

### 路径 A：模块直接按结构体访问

```text
ProjectParameterManager::Data()
        ->
拿到 const ProjectParameters&
        ->
直接读取 parameters.control.rate_pid.kp
```

适合只读、高频、结构清晰的业务逻辑。

### 路径 B：模块按参数名访问

```text
"control.rate_pid.kp"
        ->
ProjectParameterManager::FindEntry()
        ->
找到注册表项与 storage 指针
        ->
ReadRaw / WriteRaw / Read<T> / Write<T>
```

适合 CLI、调参器、参数同步、参数存储加载等“名字驱动”的场景。

## 4. 当前参数树与默认值

### 4.1 参数分组总览

当前根结构 `ProjectParameters` 包含 6 个一级分组：

| 分组 | 结构体 | 作用 |
| --- | --- | --- |
| `system` | `SystemParameters` | 系统主循环和上锁状态 |
| `task` | `TaskParameters` | 主循环与 CLI 轮询节拍 |
| `cli` | `CliParameters` | CLI 队列、默认链路、密码 |
| `control` | `ControlParameters` | 控制器参数，当前主要是 `rate_pid` |
| `motor` | `MotorParameters` | 电机 PWM 输出边界 |
| `debug` | `DebugParameters` | CLI 开关、日志详细级别 |

### 4.2 当前默认值

#### `system`

| 参数路径 | 默认值 | 说明 |
| --- | --- | --- |
| `system.control_loop_hz` | `1000` | 主控制频率 |
| `system.arm_locked` | `true` | 默认上锁 |

#### `task`

| 参数路径 | 默认值 | 说明 |
| --- | --- | --- |
| `task.main_loop_delay_ms` | `1` | 主循环延时 |
| `task.cli_poll_period_ms` | `50` | CLI 轮询周期 |

#### `cli`

| 参数路径 | 默认值 | 说明 |
| --- | --- | --- |
| `cli.rx_queue_size` | `1024` | CLI 接收队列大小 |
| `cli.default_transport` | `"usb"` | 默认链路名称 |
| `cli.password` | `"ifly"` | CLI 密码，固定数组长度 8 |

#### `control.rate_pid`

| 参数路径 | 默认值 | 说明 |
| --- | --- | --- |
| `control.rate_pid.kp` | `0.8f` | 比例增益 |
| `control.rate_pid.ki` | `0.1f` | 积分增益 |
| `control.rate_pid.kd` | `0.02f` | 微分增益 |
| `control.rate_pid.kff` | `0.0f` | 前馈增益 |
| `control.rate_pid.integral_min` | `-100.0f` | 积分下限 |
| `control.rate_pid.integral_max` | `100.0f` | 积分上限 |
| `control.rate_pid.output_min` | `-500.0f` | 输出下限 |
| `control.rate_pid.output_max` | `500.0f` | 输出上限 |
| `control.rate_pid.derivative_cutoff_hz` | `30.0f` | 微分低通截止频率 |
| `control.rate_pid.dt_min_s` | `5.0e-4f` | 最小有效采样周期 |
| `control.rate_pid.dt_max_s` | `2.0e-2f` | 最大有效采样周期 |
| `control.rate_pid.derivative_mode` | `Pid::DerivativeMode::kOnMeasurement` | 微分模式 |

#### `motor`

| 参数路径 | 默认值 | 说明 |
| --- | --- | --- |
| `motor.min_pwm` | `1000` | 最小 PWM |
| `motor.idle_pwm` | `1050` | 怠速 PWM |
| `motor.max_pwm` | `2000` | 最大 PWM |

#### `debug`

| 参数路径 | 默认值 | 说明 |
| --- | --- | --- |
| `debug.enable_cli` | `true` | 允许 CLI |
| `debug.verbose_shell` | `false` | 是否输出详细 Shell 日志 |

### 4.3 当前绑定表规模

`project_parameters.cpp` 里的 `kBindings[]` 当前共有 **32** 项，包含两类绑定：

1. 整组绑定，例如：
   - `project`
   - `system`
   - `task`
   - `cli`
   - `control`
   - `control.rate_pid`
   - `motor`
   - `debug`
2. 叶子字段绑定，例如：
   - `system.control_loop_hz`
   - `control.rate_pid.kp`
   - `cli.password`
   - `motor.max_pwm`

这意味着同一个参数中心既支持“整组读写”，也支持“单字段读写”。

## 5. 核心实现详解

### 5.1 参数树定义：结构化收口

`ProjectParameters` 的设计思想很直接：所有工程参数都应该有统一归属，而不是散落成多个无来源的全局变量。

简化后的代码如下：

```cpp
struct ProjectParameters final {
  SystemParameters system {};
  TaskParameters task {};
  CliParameters cli {};
  ControlParameters control {};
  MotorParameters motor {};
  DebugParameters debug {};
};
```

这样做的好处：

1. 模块边界清晰，参数按业务归类。
2. 后续做参数导出、存储、恢复默认值时只需要处理一棵树。
3. 能同时支持整组配置对象和单字段配置对象。

### 5.2 静态绑定表：用偏移实现“名字 -> 内存”

这套实现没有用 RTTI，也没有用运行时反射，而是用编译期可确定的 `offsetof(...)` 来建立名字和内存的绑定。

例如 PID 参数的偏移计算：

```cpp
constexpr uint32_t OffsetOfRatePid(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(ProjectParameters, control) +
                               offsetof(ControlParameters, rate_pid) +
                               member_offset);
}
```

然后在绑定表里注册：

```cpp
{"control.rate_pid.kp",
 "rate PID proportional gain",
 OffsetOfRatePid(offsetof(Pid::Config, kp)),
 sizeof(Pid::Config {}.kp),
 false},
```

这段代码的意义是：

1. 参数名是字符串，例如 `control.rate_pid.kp`。
2. 参数值在 `ProjectParameters` 对象内部的具体位置，用“根结构偏移 + 子结构偏移 + 字段偏移”确定。
3. 参数大小由 `sizeof(...)` 给出，读写时可以做边界检查。
4. `read_only` 可提前声明权限，虽然当前绑定项都是可写的。

这种做法非常适合 MCU：

- 无动态分配。
- 无运行时反射元数据构建。
- 映射关系清晰、可审查、可静态分析。

### 5.3 参数中心注册表：把绑定描述转成运行时可访问表项

`ProjectParameterManager` 在构造函数里做两件事：

```cpp
ProjectParameterManager::ProjectParameterManager()
    : data_(MakeDefaultProjectParameters()) {
  (void)BuildDefaultRegistry();
}
```

含义是：

1. 先构造出一份默认参数树 `data_`。
2. 再根据静态绑定表，把每个绑定项注册成 `entries_[]` 中的一个运行时表项。

注册过程的关键逻辑如下：

```cpp
entry.view.storage =
    reinterpret_cast<const uint8_t *>(&data_) + binding.offset;
entry.view.size = binding.size;
entry.view.access =
    binding.read_only ? AccessMode::kReadOnly : AccessMode::kReadWrite;
```

这里的 `storage` 最终就是一个指向 `data_` 内部某块内存的指针。之后无论是 `ReadRaw()` 还是 `WriteRaw()`，都只是在这块内存上做受控的 `memcpy(...)`。

### 5.4 为什么 `entries_` 不是动态容器

`ProjectParameterManager` 定义了：

```cpp
static constexpr uint16_t kMaxEntryCount = 64U;
Entry entries_[kMaxEntryCount] {};
```

这样做的考虑很明确：

1. MCU 工程里通常不希望参数模块依赖堆内存。
2. 参数表容量是稳定的，固定数组更容易审查和评估 RAM 占用。
3. 当前绑定数是 32，小于 64，留有扩展余量。

代价也很明确：

1. 查找是线性扫描，复杂度 `O(n)`。
2. 超过 64 个绑定项后必须手工调大上限。

以当前飞控参数规模，这个代价是完全可接受的。

### 5.5 读写接口：原始二进制拷贝 + 类型化模板包装

底层接口是：

- `ReadRaw(const char *name, void *buffer, uint32_t bufferSize)`
- `WriteRaw(const char *name, const void *data, uint32_t dataSize)`

上层模板接口是：

- `Read<T>(const char *name, T *value)`
- `Write<T>(const char *name, const T &value)`

模板接口内部只是对 `sizeof(T)` 做封装，并通过 `static_assert` 限制类型必须是 `trivially_copyable`。

这条限制很重要，因为当前实现本质上是 `memcpy(...)` 语义：

```cpp
static_assert(std::is_trivially_copyable<T>::value,
              "ProjectParameterManager::Write only supports trivially copyable types.");
```

也就是说，这套参数中心天然适合：

- `uint32_t`
- `bool`
- `enum`
- 纯 POD 结构体
- 定长数组封装结构体

但不适合：

- `std::string`
- 含虚函数的对象
- 持有动态资源的复杂类型

### 5.6 写入后的回调机制

写入成功后，`WriteRaw()` 会立即调用：

```cpp
NotifyUpdated(*entry);
```

回调机制有三个特点：

1. 回调是针对“参数名”绑定的，不是针对类型绑定的。
2. 回调签名很轻量：`void (*)(const char *name, void *context)`。
3. 回调只在 `Write()` / `WriteRaw()` 成功时触发。

这里要特别注意两个不会触发回调的路径：

1. `ResetToDefaults()` 只重置 `data_`，不会逐项触发回调。
2. `MutableData()` 直接暴露可写引用，调用者自己改字段，也不会触发回调。

这意味着：

- 如果你需要“配置变更后自动同步业务对象”，优先走 `Write()` / `WriteRaw()`。
- 如果你直接改 `MutableData()`，就要自己负责后续同步。

### 5.7 访问权限位目前的作用

`ProjectParameterBinding` 带了一个 `read_only` 字段，对应运行时 `AccessMode`。

当前 `kBindings[]` 中所有项都是 `false`，即全部可写。但这个设计已经把只读参数的扩展位留好了，后续如果需要加入只读字段，只要在绑定表中声明即可，`WriteRaw()` 会自动拦截。

## 6. API 说明与推荐用法

### 6.1 获取单例

```cpp
ProjectParameterManager &pm = ProjectParameterManager::Instance();
```

这是全工程唯一参数中心入口。

### 6.2 整体读取

```cpp
const ProjectParameters &params = pm.Data();
uint32_t hz = params.system.control_loop_hz;
float kp = params.control.rate_pid.kp;
```

适合：

- 状态打印
- 初始化使用
- 高频只读路径

### 6.3 整体可写访问

```cpp
ProjectParameters &params = pm.MutableData();
params.system.arm_locked = false;
```

适合：

- 启动阶段初始化
- 需要批量改多个字段且不需要逐项通知的场景

不适合：

- 依赖回调联动的运行时动态调参

### 6.4 按名字读取单个参数

```cpp
float kp = 0.0f;
if (pm.Read("control.rate_pid.kp", &kp)) {
  // 使用 kp
}
```

### 6.5 按名字写入单个参数

```cpp
const bool ok = pm.Write("system.arm_locked", false);
```

### 6.6 按名字读写整组结构

```cpp
Pid::Config cfg {};
if (pm.Read("control.rate_pid", &cfg)) {
  cfg.kp = 1.2f;
  cfg.kd = 0.03f;
  (void)pm.Write("control.rate_pid", cfg);
}
```

这是当前设计很有价值的一点：不仅叶子节点可以读写，整块结构体也能读写。

### 6.7 原始接口的使用场景

当你处理固定长度字符数组时，原始接口有时更直接：

```cpp
char password[8] = "admin";
(void)pm.WriteRaw("cli.password", password, sizeof(password));
```

但要注意：

1. `WriteRaw()` 的 `dataSize` 必须和绑定项大小完全一致。
2. `cli.password` 是固定长度 8 字节，超长内容需要调用方自己处理截断和 `'\0'` 结尾。

### 6.8 注册回调

```cpp
static void OnParamChanged(const char *name, void *context) {
  (void)context;
  // 根据 name 做同步逻辑
}

(void)pm.SetChangeHandler("control.rate_pid.kp", &OnParamChanged, nullptr);
```

回调适合：

- 改参数后重配控制器
- 改密码后更新 Shell
- 改输出边界后刷新驱动层对象

## 7. 与 CLI 的接入关系

`ProjectParameterManager` 是底层参数中心，`FlightCtrlCli` 是它的一个使用者，不是它本身的一部分。

### 7.1 CLI 做了什么

`FlightCtrlCli::RegisterParameters()` 里有一层手工映射，把 Shell 参数名映射到工程参数名：

```cpp
register_managed_parameter("pid.kp",
                           "rate PID proportional gain",
                           "control.rate_pid.kp",
                           ManagedParameterType::kFloat,
                           0.0f, 1000.0f, 0U, 0U);
```

这说明：

1. Shell 里用户看到的是短名字，例如 `pid.kp`。
2. 参数中心里存储的是完整路径，例如 `control.rate_pid.kp`。
3. CLI 额外负责文本解析、范围检查和输出格式化。

### 7.2 当前暴露到 CLI 的参数

当前 CLI 手工注册了 11 个可写参数：

| Shell 参数名 | 工程参数名 | 类型 | 范围 |
| --- | --- | --- | --- |
| `pid.kp` | `control.rate_pid.kp` | `float` | `0 ~ 1000` |
| `pid.ki` | `control.rate_pid.ki` | `float` | `0 ~ 1000` |
| `pid.kd` | `control.rate_pid.kd` | `float` | `0 ~ 1000` |
| `pid.kff` | `control.rate_pid.kff` | `float` | `0 ~ 1000` |
| `pid.i_min` | `control.rate_pid.integral_min` | `float` | `-1000000 ~ 1000000` |
| `pid.i_max` | `control.rate_pid.integral_max` | `float` | `-1000000 ~ 1000000` |
| `pid.out_min` | `control.rate_pid.output_min` | `float` | `-1000000 ~ 1000000` |
| `pid.out_max` | `control.rate_pid.output_max` | `float` | `-1000000 ~ 1000000` |
| `pid.d_cutoff_hz` | `control.rate_pid.derivative_cutoff_hz` | `float` | `0 ~ 1000` |
| `sys.loop_hz` | `system.control_loop_hz` | `uint32_t` | `50 ~ 4000` |
| `sys.arm_locked` | `system.arm_locked` | `bool` | `true/false/on/off/yes/no/1/0` |

另外还有两个只读的 Shell 参数，它们不来自 `ProjectParameterManager`：

| Shell 参数名 | 来源 | 说明 |
| --- | --- | --- |
| `sys.transport` | `FlightCtrlCli::active_transport_name_` | 当前活动链路 |
| `sys.uptime_ms` | `tick::NowMs()` | 系统运行时间 |

### 7.3 CLI 命令格式

根据 `Shell` 实现，常用命令格式如下：

```text
list
get <name>
set <name> <value>
call <name> [args...]
```

对当前参数中心最常用的是：

```text
get pid.kp
set pid.kp 1.25
get sys.loop_hz
set sys.arm_locked false
call status
```

### 7.4 从 CLI 改 PID 到 PID 生效的完整链路

以 `set pid.kp 1.25` 为例，实际链路如下：

```text
Shell 命令
  -> FlightCtrlCli::SetManagedParameter()
  -> ParseFloat()
  -> ProjectParameterManager::Write("control.rate_pid.kp", parsed)
  -> WriteRaw() 写入 data_
  -> NotifyUpdated()
  -> FlightCtrlCli::OnProjectParameterUpdated()
  -> ApplyPidConfiguration()
  -> rate_pid_.Configure(...)
```

这条链路有两个重要细节：

1. CLI 先做字符串解析和范围检查，再交给参数中心写入。
2. PID 真正生效依赖回调，而不是单纯依赖参数树被改了。

### 7.5 PID 参数为什么会被“二次写回”

`FlightCtrlCli::ApplyPidConfiguration()` 在调用 `rate_pid_.Configure(...)` 后，会取回 `rate_pid_.GetConfig()`，如果发现控制器内部清洗后的配置和参数树中的值不一致，就把“清洗后的整组配置”重新写回参数树：

```cpp
const Pid::Config &sanitized = rate_pid_.GetConfig();
if (memcmp(&parameter_manager_.Data().control.rate_pid,
           &sanitized,
           sizeof(Pid::Config)) != 0) {
  (void)parameter_manager_.Write("control.rate_pid", sanitized);
}
```

这样做的意义是：

1. 参数树始终尽量与实际生效值一致。
2. 如果用户写入了异常值、反向范围或非法值，PID 内部清洗后，参数树也会被修正。
3. CLI 再次读取时看到的是“最终生效值”，不是“原始输入值”。

## 8. 代码片段解读

### 8.1 片段一：默认值工厂

```cpp
ProjectParameters MakeDefaultProjectParameters() {
  return ProjectParameters {};
}
```

这段代码看起来简单，但设计价值很高：

1. 默认值只需要在结构体成员初始化处定义一次。
2. “恢复默认参数”不需要复制一堆常量，只要重新构造一份根结构。
3. 后续如果从 Flash 加载失败，也可以直接用这份工厂结果兜底。

### 8.2 片段二：注册表构建

```cpp
bool ProjectParameterManager::BuildDefaultRegistry() {
  uint16_t binding_count = 0U;
  const ProjectParameterBinding *bindings = GetProjectParameterBindings(&binding_count);

  count_ = 0U;
  for (uint16_t index = 0U; index < kMaxEntryCount; ++index) {
    entries_[index] = Entry {};
  }

  for (uint16_t index = 0U; index < binding_count; ++index) {
    if (!Register(bindings[index])) {
      success = false;
    }
  }

  return success;
}
```

这一段体现了当前实现的运行时模型：

1. 绑定描述表是静态只读的。
2. 启动时把它转成运行时表项数组 `entries_[]`。
3. 每个 `Entry` 除了只读视图，还能挂变更回调。
4. 构建失败不会崩溃，但会返回 `false`，方便后续补充诊断。

### 8.3 片段三：安全写入

```cpp
bool ProjectParameterManager::WriteRaw(const char *name,
                                       const void *data,
                                       uint32_t dataSize) {
  Entry *entry = FindEntry(name);
  if ((entry == nullptr) || (data == nullptr) ||
      (entry->view.storage == nullptr) ||
      (entry->view.access != AccessMode::kReadWrite) ||
      (dataSize != entry->view.size)) {
    return false;
  }

  (void)memcpy(const_cast<void *>(entry->view.storage), data, entry->view.size);
  NotifyUpdated(*entry);
  return true;
}
```

值得注意的点：

1. 它拒绝部分写入，`dataSize` 必须完全相等。
2. 它会校验访问权限。
3. 它写完立刻通知，不需要调用者额外触发事件。
4. 它是基于原始内存写入的，所以类型约束放在模板包装层做。

### 8.4 片段四：CLI 的“参数桥接层”

```cpp
register_managed_parameter("sys.loop_hz",
                           "control loop frequency",
                           "system.control_loop_hz",
                           ManagedParameterType::kUint32,
                           0.0f, 0.0f, 50U, 4000U);
```

这一行背后实际上做了 4 件事：

1. 为 Shell 注册了一个对外可见的参数名 `sys.loop_hz`。
2. 指定它实际映射到参数中心中的 `system.control_loop_hz`。
3. 规定了解析类型是 `uint32_t`。
4. 增加了 CLI 侧的输入范围校验。

这层桥接的价值在于：参数中心保持中立，而 CLI 可以独立决定暴露哪些字段、用什么别名、做什么范围限制。

## 9. 典型用法示例

### 9.1 示例：业务模块读取系统频率

```cpp
ProjectParameterManager &pm = ProjectParameterManager::Instance();
const uint32_t loop_hz = pm.Data().system.control_loop_hz;
```

适合只读场景，开销最小。

### 9.2 示例：按名字更新上锁状态

```cpp
ProjectParameterManager &pm = ProjectParameterManager::Instance();
(void)pm.Write("system.arm_locked", false);
```

适合通用调参与脚本驱动。

### 9.3 示例：整组更新 PID 参数

```cpp
ProjectParameterManager &pm = ProjectParameterManager::Instance();

Pid::Config config = pm.Data().control.rate_pid;
config.kp = 1.0f;
config.ki = 0.15f;
config.kd = 0.025f;

(void)pm.Write("control.rate_pid", config);
```

这种方式适合一次性修改同一组中的多个字段。

### 9.4 示例：监听密码变化

```cpp
static void OnCliPasswordChanged(const char *name, void *context) {
  FlightCtrlCli *cli = reinterpret_cast<FlightCtrlCli *>(context);
  if ((cli != nullptr) && (strcmp(name, "cli.password") == 0)) {
    // 重新应用密码
  }
}

(void)pm.SetChangeHandler("cli.password", &OnCliPasswordChanged, cli);
```

### 9.5 示例：CLI 实际用法

```text
list
get pid.kp
set pid.kp 1.2
get sys.loop_hz
set sys.arm_locked true
call status
```

## 10. 扩展这套参数中心的标准步骤

后续如果要新增一个参数，推荐严格按下面步骤做。

### 步骤 1：把字段加到参数树里

例如新增 IMU 参数：

```cpp
struct ImuParameters final {
  float accel_lpf_hz = 80.0f;
  float gyro_lpf_hz = 120.0f;
};

struct ProjectParameters final {
  SystemParameters system {};
  TaskParameters task {};
  CliParameters cli {};
  ControlParameters control {};
  MotorParameters motor {};
  DebugParameters debug {};
  ImuParameters imu {};
};
```

### 步骤 2：在 `kBindings[]` 中补绑定

至少补这两类：

1. 整组绑定，例如 `imu`
2. 叶子绑定，例如 `imu.accel_lpf_hz`

示意：

```cpp
{"imu", "IMU parameter group",
 static_cast<uint32_t>(offsetof(ProjectParameters, imu)),
 sizeof(ImuParameters), false},

{"imu.accel_lpf_hz", "accelerometer LPF cutoff",
 static_cast<uint32_t>(offsetof(ProjectParameters, imu) +
                       offsetof(ImuParameters, accel_lpf_hz)),
 sizeof(ImuParameters {}.accel_lpf_hz), false},
```

### 步骤 3：如果需要 CLI 暴露，再到 `FlightCtrlCli::RegisterParameters()` 手工映射

例如：

```cpp
register_managed_parameter("imu.accel_lpf_hz",
                           "accelerometer LPF cutoff",
                           "imu.accel_lpf_hz",
                           ManagedParameterType::kFloat,
                           1.0f, 500.0f, 0U, 0U);
```

### 步骤 4：如果改参数后需要联动业务对象，再挂回调

例如：

```cpp
(void)parameter_manager_.SetChangeHandler("imu.accel_lpf_hz",
                                          &FlightCtrlCli::OnProjectParameterUpdated,
                                          this);
```

### 步骤 5：如果需要持久化，再在更高层补存储逻辑

当前参数中心不负责存储介质。未来要做 Flash 持久化，比较自然的接入点是：

1. 启动时：从 Flash 读整棵 `ProjectParameters`
2. 校验失败时：回退 `MakeDefaultProjectParameters()`
3. 参数修改后：按需保存整棵树或增量保存

## 11. 维护时要特别注意的点

### 11.1 `ResetToDefaults()` 不会触发回调

当前实现只是：

```cpp
void ProjectParameterManager::ResetToDefaults() {
  data_ = MakeDefaultProjectParameters();
}
```

所以如果某些业务对象依赖回调同步，调用完 `ResetToDefaults()` 后要手动重新应用，例如当前 CLI 就在 `Init()` 里又显式执行了：

- `shell_.SetPassword(...)`
- `ApplyPidConfiguration()`

### 11.2 `MutableData()` 会绕过通知系统

`MutableData()` 很方便，但它会绕开：

- 权限检查
- 大小检查
- 参数名访问
- 变更回调

因此它更适合初始化阶段，而不是运行时热更新。

### 11.3 `WriteRaw()` 必须精确匹配大小

这是故意设计的保护机制，用来避免“只写了一半结构体”这种破坏对象状态的问题。

对比来说：

- `ReadRaw()`：缓冲区可以更大，但不能更小。
- `WriteRaw()`：大小必须完全相等。

### 11.4 当前查找是线性扫描

`FindEntry()` 内部是从 `0` 扫到 `count_ - 1` 做 `strcmp(...)`。

这在 32 个参数时完全没问题，但如果以后参数规模显著增长，需要评估是否改成：

- 排序数组 + 二分查找
- 哈希
- 生成式查表

### 11.5 当前 CLI 容量也有上限

`Shell` 自己的参数上限是 `kMaxParameterCount = 24`，而当前 `FlightCtrlCli` 已经注册了：

- 11 个托管可写参数
- 2 个只读参数

也就是一共 13 个，仍有余量，但不是无限的。

### 11.6 `cli.password` 是固定长度数组

`char password[8]` 实际可用字符长度最多是 7，再加一个字符串结束符 `'\0'`。

如果未来密码要更长，不能只改 CLI 文本层，必须同时改：

1. `CliParameters::password`
2. 绑定表对应大小
3. 任何直接写入该字段的代码

## 12. 这套实现的优点与局限

### 优点

1. 结构清晰，工程参数统一归口。
2. 无动态内存，适合 STM32 这类 MCU 工程。
3. 既支持结构体整组访问，也支持字符串按名访问。
4. CLI、后续存储、上位机调参都能复用同一套底层参数接口。
5. 写入后可挂回调，便于联动业务对象。

### 局限

1. 还没有持久化层。
2. 没有并发保护。
3. 名字查找是线性的。
4. 没有统一的参数元信息系统，例如类型枚举、单位、步进、显示格式。
5. CLI 暴露层仍然是手工维护，不是自动从绑定表生成。

## 13. 建议的后续演进方向

如果这套参数中心后续会继续扩展，可以优先考虑以下方向：

1. 增加参数持久化层，把整棵 `ProjectParameters` 写入 Flash。
2. 给 `ProjectParameterBinding` 增加类型信息、单位、最小值、最大值、显示精度。
3. 让 CLI 可自动从绑定表生成参数列表，而不是每个都手工映射。
4. 对“整组写入”增加事务语义，避免多参数联动时出现中间态。
5. 对关键参数增加只读、启动期可写、运行期可写等更细粒度权限。

## 14. 一句话总结

`paramete` 目录下这套代码，本质上是在 MCU 项目里实现了一棵静态、可按名访问、支持回调联动的工程参数树。`ProjectParameters` 负责定义“参数长什么样”，`kBindings[]` 负责定义“名字如何映射到内存”，`ProjectParameterManager` 负责定义“如何安全读写与通知”，`FlightCtrlCli` 则把这套能力转成了实际可交互的调参入口。

# C# 脚本线程与引擎访问规范

> **适用对象**：在本项目（或任何使用本 fork 的 UnrealSharp 工程）里写 C# 玩法/工具代码的人。
> **一句话**：UE 的 `UObject` 世界是单线程的——**碰引擎必须在游戏线程上**；本规范规定"哪些算碰引擎""哪些 API 会把你带离游戏线程""违规怎么被发现"。
> **配套**：分析器规则（`US0015+`）、运行时安全网（`EngineCallGuard` / 原生拒绝）、实施计划见 `问题/2026-09-22-非游戏线程建对象崩溃/解决方案.md`。
> 缺陷的前因、机制、证据链见同目录 `原因与机制.md`；摘要入口为 `插件使用笔记.md` §七 第 4 条。

---

## 1. 为什么必须有这条规范

1. **物理约束**：`UObject` 的创建/修改/生命周期由引擎在游戏线程上独占管理；引擎 GC 还会锁住对象哈希表（`IsGarbageCollectingAndLockingUObjectHashTables()`），此时**即使游戏线程也不能建对象**（`UObjectGlobals.cpp:3484` 的裸 `checkf`）。
2. **失败是静默的**：引擎对绝大多数"非游戏线程碰引擎"没有断言。开发版偶尔崩，**玩家版 `DO_CHECK=0`，那句 `checkf` 连编译都不编进去** → 表现为内存破坏、随机崩溃、GC 漏收。
3. **C# 引入了 C++ 没有的隐式换线程**：`await` 的续体回哪条线程，由 await 那一刻捕获的 `SynchronizationContext`（UnrealSharp 默认不安装）或"完成任务的那条线程"决定。C++ 里换线程永远是显式的（`AsyncTask(thread, …)`），所以 C++ 里"业务代码不在 GC 期间"是结构保证；C# 里不是。
4. **反过来说**：玩法代码必须碰引擎，规范不是"少碰"，而是"**在正确的线程、正确的时机、对有效的对象**去碰"。

---

## 2. 什么算"碰引擎"（判定三问）

一次调用只要命中下面任意一条，就是"碰引擎"：

| 类别 | 具体 | 例子 |
|---|---|---|
| A. 读写引擎对象内存 | 访问 `[UProperty]` 属性（走原生 `FProperty` 偏移）；读写引擎结构体/容器 | `actor.ActorLocation`、`Health += 1` |
| B. 调用引擎/蓝图函数 | 走生成的 `Bind_*` 函数指针 | `SetActorLocation`、`UGameplayStatics.*`、`KismetMathLibrary.*` |
| C. 创建/销毁对象 | `NewObject<T>`、`SpawnActor`、`AddComponent*`、`CreateWidget`、`Destroy` | — |
| D. 元操作 | 类型注册（`[UClass]` 编译成 `UClass`）、解析原生字段指针（绑定类静态构造）、**懒加载程序集**、结构体原生内存分配 | 首次触碰任意引擎绑定类 |
| E. GC/句柄交互 | `IsValid`、`TStrongObjectPtr`、句柄表、`AddReferencedObjects` 路径 | — |

**不碰引擎**：纯托管字段/局部变量、LINQ、集合、`System.Text.Json`、你自己的 POCO/算法、文件与网络 I/O。
**注意**：日志（`Bind_FMsg.CallLog`）等少数设施是引擎做成线程安全的；**不要凭直觉假设**某个调用安全——不确定就当作"碰引擎"。

**判定三问**：① 这次调用最终会不会走 `Bind_*` / 访问 `NativeObject` / 解析 `NativeReflectionHelper`？② 会不会建对象、注册类型、加载程序集、动句柄？③ 读写的是 `[UProperty]`/引擎类型，还是普通 C# 成员？

---

## 3. 线程入口清单

| 在**游戏线程**上（可直接碰引擎） | 在**非游戏线程**上（碰引擎前必须派发） |
|---|---|
| `BeginPlay` / `Tick` / `EndPlay` / 生命周期回调 | `Task.Run` / `ThreadPool.QueueUserWorkItem` / `new Thread` |
| 蓝图事件、`[UFunction]` 被 BP 调用、委托广播（源自 GT） | `System.Threading.Timer` / `PeriodicTimer` |
| 输入、UMG/Slate 回调、`SetTimer`、latent action 完成 | `Parallel.*` / PLINQ `AsParallel()` |
| 引擎加载完成等以 GT 回调的委托 | `ContinueWith(...)`（默认调度器）、第三方库的异步回调（HTTP/WebSocket/消息总线/文件监听） |
| — | .NET **终结器线程**（`~Finalizer`）、`async` 未回 GT 的续体 |

**自查手段（开发版）**：`EngineCallGuard.IsEngineCallSafe`；违规时引擎侧会打 Error（线程 id + GC 状态 + 调用栈），版本内计数可用控制台命令查看（见方案 W1）。

---

## 4. 必须使用的模式

### 4.1 `await` 之后要碰引擎 → 显式回到游戏线程

```csharp
// ✅ 有 world context 时（玩法代码常规场景）：把"碰引擎的代码"放在 await 之后，但必须先回 GT
await SomeTask().ConfigureWithUnrealContext();
actor.SetActorLocation(FVector.Zero);

// ✅ 没有 world context 时（编辑器工具、启动期、PIE 之后、子系统/单例收尾）：
//    碰引擎的代码必须放在 RunAsync 的【同步委托内部】
await GameThreadDispatcher.RunAsync(() => BuildDependencyMap(projects));
```

> ⚠️ **两个容易踩的边界（务必记住）**
> 1. **`RunAsync` 只保证"传进去的那个同步委托内部"在游戏线程**。它是 `TaskCompletionSource(RunContinuationsAsynchronously)`，
>    所以 `await GameThreadDispatcher.RunAsync(...)` **之后**的调用方续体**不保证**在游戏线程：
>    ```csharp
>    await GameThreadDispatcher.RunAsync(() => ApplyToEngine(x));   // ✅ 委托内安全
>    actor.SetActorLocation(...);                                  // ❌ 这里不保证在 GT
>    ```
> 2. **禁止 `RunAsync(async () => …)`**：签名是 `Action`，async lambda 会变成 `async void`（异常与完成都不可观测，也不会被等待）。要异步就分成"后台 await + 边界派发"两步。
> ⚠️ **`ConfigureWithUnrealContext()` 不是万能的"切回游戏线程"**，它有几条硬限制：
> - 构造要求**有效 world**（编辑器启动早期/无 world 场景会抛异常）；world 中途失效会**静默丢弃**续体（任务永不完成）；
> - **已完成的 `ValueTask` 会直接返回，不切线程**（同步完成时"没换线程"）；
> - 它接受 `NamedThread` 参数，**不保证**目标一定是 GameThread；
> - 非泛型 Task 重载**不传播原任务的异常**。
> ⇒ 需要"只要回游戏线程、与具体 world 无关"时，用 `GameThreadDispatcher.RunAsync`（把工作放进同步委托）；需要"世界语义"时才用 `ConfigureWithUnrealContext()`，并且必须按"同步完成 / 异步完成 / 取消与异常"分别验证。

**选择标准**：需要"世界语义"（拿到当前 world / 世界相关 API）→ `ConfigureWithUnrealContext()`；只需要"回游戏线程"（与具体世界无关）→ `GameThreadDispatcher.RunAsync`（同步委托内部）。

> ✅ **`RunAsync` 的交接保证（已实现，可用于排错）**：它**不会**静默挂起。四条判定点：
> 1. 插件/队列已关机 ⇒ **不分配句柄**，Task 立即以失败完成；
> 2. 原生派发拒收（引擎正在退出）⇒ 托管**自己释放句柄**，Task 以失败完成；
> 3. 已接收但轮到执行时状态不安全（队列已关机、或不是 GT / 正在 GC）⇒ **不执行你的委托**，Task 以失败完成；
> 4. 正常执行 ⇒ Task 成功；`await` 之后仍**不保证**在 GT（见上一条 ⚠️）。
>
> 失败类型是 `UnrealSharp.Core.EngineCallRefusedException`（继承 `InvalidOperationException`）。诊断计数：`GameThreadDispatcher.Default`（`HandedToNative` / `RefusedBeforeSubmission` / `RefusedByNative` / `RefusedWhileRunning`）。

### 4.2 后台干重活，只在边界回 GT

```csharp
// ✅ 后台算，回 GT 应用
var cooked = await Task.Run(() => ComputeOffline(data)).ConfigureAwait(false);
await GameThreadDispatcher.RunAsync(() => ApplyToEngine(cooked));

// ❌ 每一步都回 GT：帧量化延迟（最多 ~16ms/次）且拖慢 GT
for (...) { await something.ConfigureWithUnrealContext(); HeavyManagedWork(); }
```

### 4.3 异步玩法逻辑：取消 + 有效性

```csharp
public async Task RunAsync(CancellationToken token)
{
    await Task.Delay(500, token).ConfigureWithUnrealContext();
    if (token.IsCancellationRequested || !IsValid(this) || IsDestroyed) return;  // ← 必查
    DoEngineWork();
}
```

PIE 结束、关卡卸载、对象被销毁之后，**继续持有并使用旧引用是 bug**；热重载后旧指针同样可能失效。

### 4.4 定时/延迟优先用引擎原生调度

`SetTimer` / latent action / async UFunction 都在游戏线程上，天然正确；`System.Threading.Timer` / `PeriodicTimer` 在池线程上，且不随 PIE/世界生命周期自动清理。

---

## 5. 受限与禁止清单

| 项 | 结论 | 替代 / 说明 |
|---|---|---|
| 在 `Task.Run` / Timer / `Parallel` / PLINQ 体内碰引擎 | **禁止** | 把碰引擎的代码移进 `GameThreadDispatcher.RunAsync` 的**同步委托内部**（注意：`await RunAsync(...)` **之后**的代码不保证在 GT） |
| `async void` | **禁止**（例外：引擎签名强制，如引擎事件重写、受控包装的事件处理器白名单；例外只豁免"返回类型"这一条） | 用 `async Task` + 显式错误处理；`async void` 的异常无法观测 |
| `RunAsync(async () => …)` | **禁止** | 签名是 `Action` ⇒ async lambda 会变成 `async void`；改为"后台 await + 边界派发"两步 |
| 把 `ConfigureWithUnrealContext()` 当作"必然切回 GT" | **禁止** | 同步完成的 `ValueTask` 不切线程、world 失效静默丢回调；"真实符号 + GameThread 参数"只是**必要条件，不是充分的 GT 证明**（见 4.1）；无法证明时分析器继续提示 |
| 在游戏线程上 `.Result` / `.Wait()` / `GetAwaiter().GetResult()` | **禁止** | `await`；装了上下文后这类写法是经典 self-deadlock |
| `await` 之后碰引擎但不回 GT | **禁止** | 见 4.1 |
| 对已销毁/跨 PIE、跨热重载的旧引用做操作 | **禁止** | 每次 await 后检查 `IsValid` / `IsDestroyed` / token |
| 依赖 `NewObject<T>()` 返回 `null` 来判断失败 | **禁止** | 现在失败会**抛异常**（见 §6） |
| 终止器（`~Finalizer`）里碰引擎 | **禁止** | 终结器线程不是游戏线程；改为显式 `Dispose`/清理路径 |
| 第三方库回调里碰引擎 | **禁止**（回调里只做纯托管处理） | 边界处派发回 GT |

---

## 6. 失败语义（必须知道）

- **原生拒绝**：非游戏线程 / GC 中调用"加载程序集、解析类型、创建对象"等入口 → 原生记 Error 并返回 `nullptr`，托管侧**抛可捕获异常**（不是拿空指针继续跑）。
- **副作用**：如果异常发生在某个类型的**静态构造**里，.NET 会永久标记该类型初始化失败 ⇒ **该类型在本进程内再也用不了**。所以违规的代价是"这块功能坏掉 + 日志报错"，比编辑器/游戏直接死掉好，但**不能靠它当恢复手段**。
- 因此：**违规必须在开发期被发现并改掉**，这正是分析器与开发版检查存在的原因。

---

## 7. 热重载相关

- 热重载会卸载/重建 ALC：**静态字段、缓存的委托、长生命周期的后台任务**都会跨代失效。
- 任何长时间后台任务必须**可取消**，并在回到 GT 后重新校验对象与 world 是否仍然有效。
- 不要缓存跨热重载的引擎指针；需要时重新解析。

---

## 8. Review checklist（每条都应在 CR 里被问一次）

1. 这段代码在哪条线程上执行？它从哪个回调进来？
2. 里面每一次 `await`，之后要碰引擎吗？碰之前回 GT 了吗（`ConfigureWithUnrealContext` / `GameThreadDispatcher.RunAsync`）？
3. 有没有 `Task.Run` / Timer / `Parallel` / PLINQ / 第三方回调？它们内部碰引擎了吗？
4. 有没有 `async void`？是否属于白名单？
5. 有没有在游戏线程上 `.Result`/`.Wait()`？
6. 每次 `await` 之后有没有校验 `IsValid`/`IsDestroyed`/`CancellationToken`？
7. 有没有依赖 `NewObject` 返回 `null`？
8. 有没有在终结器里碰引擎？
9. 长任务是否可取消、是否会在 PIE 结束后继续跑？
10. 新增的"线程安全例外"是否真的线程安全（本轮用"安全 API 白名单 + 分析器定向抑制（附理由）"，不引入 `[ThreadSafe]`）？
11. 碰引擎的代码是不是在 `RunAsync` 的**委托内部**（而不是 `await` 之后）？

---

## 9. 工具支持（与本规范的对应关系）

| 规范条目 | 静态检查（分析器 `US0015+`） | 运行时检查 |
|---|---|---|
| 4.1 / §5 await 后未回 GT | `US0015`（豁免只限 `RunAsync` 的**同步委托内部**） | 开发版边界检查（Tier B） |
| §5 后台线程碰引擎 | `US0016` | Tier B |
| §5 `async void` / `RunAsync(async …)` | `US0017` | — |
| §5 阻塞等待 | `US0018`（初期 Info） | — |
| §5 终结器碰引擎 | `US0019`（间接调用为已知漏报） | Tier B（若能覆盖） |
| 建对象 / 加载包 / 注册类型 / 解析字段 | 建对象**类型识别**可复用 `AnalyzerStatics`（注意：`US0007–US0011` 查的是"用 `new` 创建"这种**创建方式**，不是线程合法性） | **Tier A：始终开启（含 Shipping，不可关闭）** |

> ⚠️ **"不报 ≠ 安全"**：分析器是**局部、保守**的提示（不做全程序线程证明），间接调用、反射、动态委托、第三方库内部执行流都可能漏报；生成代码不报也**不代表**它已被 Tier B 覆盖。**受保护集合、首次受保护访问点与逐符号契约以 `解决方案.md` §9.6 / §10 为准。**

**运行时诊断命令（已实现）**

| 命令 / 开关 | 作用 |
|---|---|
| `unrealsharp.ThreadViolations` | 打印拒绝计数、边界捕获计数与最近 16 条记录（线程、状态位、原因），用于**事后诊断** |
| `unrealsharp.ThreadGuardSelfTest` | **确定性自检（原生半）**：核对原生守卫在游戏线程**不**拒绝、在线程池**必须**各拒一次且各计一次 |
| `unrealsharp.ThreadSelfTest=1`（改后需重启）或命令行 `-UnrealSharpThreadGuardSelfTest` | **确定性自检（托管半）**：核对托管检查点同上；默认关闭，因为它会**故意**写入拒绝计数 |
| `unrealsharp.ThreadChecks` | `0`=仅 Tier A、`1`=+日志、`2`=+Tier B 开发检查；**改后需重启**，**Tier A 与该开关无关**，永不被它关闭 |
| `-UnrealSharpThreadGuardFinalizerTest`（叠加在上一条上） | **终结器边界用例**：故意让 3 个对象的终结器在**非 GT** 上碰引擎，核对"拒绝被记录且**进程不退出**"。**必须单独跑一个进程**——协议若回归，这里是进程直接死，而不是自检失败 |

自检失败会在日志里以 `Thread guard self test: FAIL ...` 出现；无输出即为通过（原生半会打印 `PASS (0 failed check group(s))`）。

### 9.1 引用计数与清理的成对语义（Tier A 会拒绝，不只是记录）

碰引用计数/析构的入口打开时**成对拒绝**，你在非 GT 上不会得到"半做"的结果：

| 你的调用 | 被拒时的保证 |
|---|---|
| 取得引用（`FText` 构造、`TStrongObjectPtr` 构造、`AddRef`） | **一个引用都没取得**：构造直接抛，字段不会被写成一个"需要释放"的状态 |
| 释放/析构（`Dispose`、`Release`、`Destroy`、`FInstancedStructManager`） | **字段与引用都保留**（不会出现"字段清零、原生没释放"）：显式 `Dispose` 抛 `EngineCallRefusedException`；**终结器里只记 refusal + boundary catch，绝不抛**（终结器抛异常 = 进程终止） |
| 写共享句柄（`ManagedHandle.Store`）、写 `FText` 目标（`TextMarshaller.ToNative`） | **事务性**：在任何 `Release`/覆盖/`AddRef` 之前就拒绝，不产生"目标已释放但看起来还占着"或"引用已取得却没人指向" |
| delegate 注册（`WorldCleanup` / `PIE`） | **注册没发生**，并且回给你的句柄被清零 ⇒ 之后解绑是 no-op（不会拿垃圾句柄去解绑） |

⚠️ 被拒的**释放**是有界、可诊断的**泄漏**（记录在 `unrealsharp.ThreadViolations` 里），**不是**"可以接受的写法"：正确做法仍是先回游戏线程再释放（§4.1）。

---

## 10. 决策记录：为什么不做"默认回游戏线程"

曾被考虑并**明确放弃**（理由：帧量化延迟、后台重活被搬上 GT、一旦有上下文则"GT 上阻塞等待"变成经典 self-deadlock、对现存项目是行为变更，且它管不到 `Task.Run`/计时器/第三方回调/终结器等无上下文可捕获的路径）。
**结论**：以"**编译期提醒（分析器）+ 开发期响亮失败（注入检查）+ 边界始终拒绝（Tier A）**"替代"默认上下文"。相关决策与师傅复核意见见 `问题/2026-09-22-非游戏线程建对象崩溃/解决方案.md`。

---

## 11. 残余风险（**主动接受**，不是遗漏）

1. **状态快照不是锁**：`EngineCallGuard` 只报告调用那一刻的状态，**不承诺 TOCTOU 安全**；GC 位不可缓存。
2. **不能证明对象有效、也不能证明跨 ALC 指针有效**：回 GT 只解决线程；PIE 结束 / 关卡卸载 / 热重载后的对象有效性仍由业务代码自己检查。
3. **静态构造毒化**：类型初始化失败后该类型本进程不可用且不重试 —— 这是**止损**，不是恢复手段。
4. **结果通道局限**：原生 `void` 入口无法回传具体失败原因；`nullptr` 也可能表示"未找到"（靠托管前置检查消歧）⇒ 不承诺"任何原生拒绝都变成可捕获托管异常"。
5. **清理残余**：拒绝 delegate 解绑等场景可能留下**有界、可诊断**的残余；owner 与解绑时机见 `解决方案.md` §10.3，**不允许把"catch 后泄漏"当方案**。
6. **`Task.Run` / Timer / 第三方回调 / 终结器 / 自建线程是"执行来源"，不是"绕过守卫的机制"**：插件不提供这些线程上的**一般性**线程安全证明，但它们一旦调用受保护入口（Tier A / P0 / P1 / P2 / P3 任一），**仍会被拒绝或记录**。
7. **分析器只覆盖你的代码**：反射、动态调用、第三方库内部执行流、间接方法组可能漏报。
8. **Shipping 下 Tier B 关闭**：仅已明确覆盖的 Tier A 保持拒绝。

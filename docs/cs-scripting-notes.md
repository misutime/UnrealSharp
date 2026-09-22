# UnrealSharp C# 脚本机制速查（本引擎 5.8.2 实测）

> 这份笔记只记录**在本引擎上验证过**的机制与坑，供写 C# 脚本时快速对照。
> 每条标注 **实测**（运行期证据）或 **源码核**（读源码/生成产物得出）。
> ⚠️ 这是**我们项目的附加文档**，不属于 UnrealSharp 上游；上游更新子模块时留意别被覆盖。

---

## 一、覆写引擎事件：方法与命名

C# 里覆写的是**引擎的 `Receive*` 蓝图事件**，不是同名的 C++ 虚函数。生成绑定把二者对接：

| C# 里写 | 引擎侧对应 | 说明 |
|---|---|---|
| `override void BeginPlay()` | `ReceiveBeginPlay` | 不是 `AActor::BeginPlay`（那个 native 方法由引擎自己调） |
| `override void Tick(float deltaSeconds)` | `ReceiveTick` | 签名固定 `(float)` |
| `override void EndPlay(EEndPlayReason)` | `ReceiveEndPlay` | |

**哪个 C# 方法会被生成参与覆写**：由胶水生成器按 `ClassReflectionData->Overrides` 决定
——即**你在 C# 里真的覆写了它**，才会生成对应的 `Receive*`（源码核）。

---

## 二、Tick 可用性与开启方式

**`AActor` / `UActorComponent` 子类可以直接覆写 `Tick`，且不需要手动开启**（源码核，
`CSManagedClassCompiler::SetupDefaultTickSettings`）：

1. 只有 `AActor`（`PrimaryActorTick`）与 `UActorComponent`（`PrimaryComponentTick`）会被处理，
   **其它基类直接返回** ⇒ 非 Actor/非 ActorComponent 的 C# 类**不会 tick**；
2. 若父类已开启 tick，直接继承；否则**沿继承链找本类声明的 `ReceiveTick`**，
   找到就把 `bCanEverTick` 与 `bStartWithTickEnabled` 都置 **true**。

⇒ **不用写 `SetActorTickEnabled(true)`**。想关掉就用该 API 或组件上的
`ComponentTickEnabled = false`（**只写属性**，读要用 `IsComponentTickEnabled()`）。

---

## 三、`base.` 调用：语义与实测

### 结论（**实测**）

| 情形 | 不写 `base.` 的后果 |
|---|---|
| 直接继承 `AActor` / `UActorComponent`（父类**无** C# 实现） | 无差别 |
| 中间基类**有**同名覆写（如 `AMyBase : AActor` 有自己的 `BeginPlay`） | **父类那段脚本实现不会执行** |
| 子类**完全不覆写**某方法 | 继承父类的实现（引擎会调到父类那份） |

**`base.BeginPlay()` / `base.Tick()` 安全、不递归**（实测：两层类各打印一次、顺序正确）。
**建议写 `base.`** —— 理由是将来插入中间基类时不会静默丢逻辑，不是插件要求。

### 引擎侧派发独立于你的 `base.` 调用（源码核）

`AActor::BeginPlay` 自己做 `SetLifeSpan`、注册 tick、给所有组件派 `BeginPlay`，**之后**才调
`ReceiveBeginPlay()`；`AActor::Tick` 判断类标记后调 `ReceiveTick`。
⇒ **不写 `base.` 不会丢引擎功能**，只会丢父类的脚本实现。

### 为什么 `base.` 不递归（源码核：4 条已确证事实）

1. 脚本类的事件函数被标成 **`FUNC_Native`**，原生入口绑到 `InvokeManagedMethod`
   （`FinalizeFunctionSetup`）；
2. 托管入口用 **`Stack.CurrentNativeFunction`**（当前正执行的那个 UFunction）的句柄，
   不按名字重查；
3. 引擎 `ProcessEvent` 只对**传进来的 `Function` 指针**操作，不做名字重解析；
   且 `Function->Script.Num() == 0`（无字节码）时直接返回；
4. `base.BeginPlay()` 走到的是 **native 的 `AActor::ReceiveBeginPlay` 桩**
   （生成绑定用 `GetFirstNativeImplementationFromInstanceAndName` ⇒ 第一个 native 类的函数）。

**⚠️ 仍未锁死的一跳**：`FindFunctionChecked` 对脚本实例取到的是子类那份 `ReceiveBeginPlay`，
它按第 1 条是 `FUNC_Native`；这两条如何精确组合出"只调一次父类"，无决定性证据。
**行为已定论，机制最后一跳存疑**——不要在文档里写成绝对结论。

### `BeginPlay` 与 `Tick` 可能走**不同**生成分支（源码核）

引擎绑定导出器按 `FUNC_BlueprintCallable` 选择取哪个函数指针：
`ReceiveBeginPlay` 是 `0x08080802`（无 `BlueprintCallable`）→ `GetFirstNativeImplementation`；
另一份引擎产物里 `ReceiveTick` 是 `0x44020403`（**含 `FUNC_Native`**）。
⇒ **别从 `BeginPlay` 的行为外推 `Tick`**。

---

## 四、API 命名与直觉的差异（逐条查证，非记忆）

| 直觉 / 旧文档写法 | 实际可用 | 备注 |
|---|---|---|
| `GetActorLocation()` | **`ActorLocation`** 属性 | 大量 getter 被生成为属性 |
| `GetActorRotation()` | **`ActorRotation`** | |
| `GetOwner()`（组件上） | **`Owner`** | |
| `SetComponentTickEnabled(b)` | **`ComponentTickEnabled = b`** | **只写**；读用 `IsComponentTickEnabled()` |
| `KismetMathLibrary.*` | **`MathLibrary.*`** | **类名被改了** |
| `PrintString(...)` | 来自手写扩展（`UnrealSharp.CoreUObject`） | 生成绑定里没有；`AActor` 可直接调 |
| `OnX.Invoke(v)` | **`X.InnerDelegate.Invoke?.Invoke()`** | `Invoke` 在 `DelegateBase<T>` 上；官方示例的写法当前编不过 |
| `TMulticastDelegate<Action>` | **必须自定义 `delegate` 类型** | 否则运行期 `TypeLoadException`（找不到 `{T.Name}__DelegateSignature` 包装） |
| `TSoftObjectPtr<T>.Get()` | **`LoadSynchronous()`** | 与直觉命名不同，实测可用 |

**命名空间要点**（漏了报 `CS0246`）：
`TSoftObjectPtr<T>` / `TMulticastDelegate<T>` / `TSubclassOf<T>` 在 **`UnrealSharp` 根命名空间**，
不在 `UnrealSharp.CoreUObject` / `UnrealSharp.Engine`。
（漏 `using UnrealSharp;` 还会**连锁**触发源生成器报
`error USG001: Type ... is not supported in PropertyFactory`，别被它误导成"类型不支持"。）

---

## 五、组件与属性写法

```csharp
[UClass]
public partial class AMyActor : AActor
{
    // 声明即自动创建并挂到根组件，不需要 AddComponent / SetupAttachment
    [UProperty(DefaultComponent = true, RootComponent = true)]
    public partial UStaticMeshComponent Mesh { get; set; }

    [UProperty(PropertyFlags.EditAnywhere | PropertyFlags.BlueprintReadWrite)]
    public partial double Speed { get; set; }

    [UProperty(PropertyFlags.BlueprintReadOnly)]
    public partial int Count { get; set; }

    [UProperty(PropertyFlags.BlueprintAssignable)]
    public partial TMulticastDelegate<FMyEvent> OnSomething { get; set; }

    public AMyActor() { Speed = 90.0; }        // 默认值在构造函数里给（没有独立的 default 块）

    public override void BeginPlay() { base.BeginPlay(); }
    public override void Tick(float dt) { base.Tick(dt); }

    [UFunction(FunctionFlags.BlueprintCallable)]
    public void DoThing() { }
}

public delegate void FMyEvent();   // 给 TMulticastDelegate 用的委托类型
```

属性标记出自 `PropertyFlags`（`EditAnywhere` / `EditDefaultsOnly` / `BlueprintReadOnly` /
`BlueprintReadWrite` / `BlueprintAssignable` / `VisibleAnywhere` …）；
函数标记出自 `FunctionFlags`（`BlueprintCallable` / `BlueprintPure` / `BlueprintEvent` …）。

---

## 六、编译与热重载

- **必须走 C++ 构建编译**（UBT / IDE）。不要靠点 `.uproject` 让编辑器自己编
  （会留下过期二进制）；若这么做了，每次拉新版要删 `Binaries/` + `Intermediate/`。
- 构建过程中 UnrealSharp 会**自动生成绑定并编译**（日志里 `[UnrealSharp] Generating C# bindings...`）。
- **改 `.cs` 存盘即热重载**（实测：编译+换装约 **0.09 ~ 0.5 秒**）。
  判定要看日志里的事件序列，**不要盯装配文件时间戳**：
  ```
  LogUnrealSharpEditor: Processed dirty file '<X>.cs' ...
  LogUnrealSharpEditor: Starting C# Hot Reload...
  LogUnrealSharpEditor: Project '<X>' produced an assembly in ... seconds
  LogUnrealSharpPlugins: Unloading plugin <X>...
  LogUnrealSharpEditor: C# Hot Reload completed in ... seconds
  ```
- 热重载会**先卸载再重新加载**模块 ⇒ **`StartupModule` 会重跑**。

---

## 七、已知缺陷与坑（遇到先查这里）

1. **`record struct` 的 `ToString()` 导致 CS0111**
   上游生成器会为带 `BlueprintAutocast` 的 `Conv_*ToString` 生成
   `record struct` 内的 `override ToString()`，与 C# 自动合成的同名成员冲突。
   一次导出实测 **22 处**（`FInputActionValue` / `FGuid` / `FVector` / `FRotator` /
   `FTransform` / `FKey` / `FGameplayTagContainer` 等）。
   **本 fork 已修**（跳过生成；上游 PR #728 的 `override`→`new` 治不了这个）。
   引擎字符串表示仍可显式调 `Conv_*ToString`。
2. **向导建 C# 工程后编辑器假死（2026-09-22 定位；引擎侧已修，插件侧补了保险）**：
   主线程停住、日志零增长、无崩溃转储；历史上都伴随**大量 MSBuild 工作节点**（238 / 815 个，28 逻辑核）。

   **判定手段（可复用，不用调试器）**：按 `ParentProcessId` 找出属于编辑器、且卡住不退出的子进程，
   **只杀它**：编辑器 2 秒内恢复 ⇒ 主线程确实在等它（两组独立复现一致）。

   **触发因素（已独立复现）**：`GetDotnetPath.bat` 把 `PATH` / `DOTNET_ROOT` 指向引擎**自带** SDK
   （10.0.203），却没清掉**从调用方继承**的 MSBuild 重定向变量 —— 它们指向**系统** SDK（10.0.401）。
   自带与系统 SDK 混用会让 `dotnet msbuild <sln> -t:Scan`（UAT/UBT 依赖检查）的**并行节点**路径失败，
   并伴随 200~417 个 MSBuild 节点进程。**单变量结论**：`MSBUILD_EXE_PATH` 单独注入致败有**两套实验一致**的证据；
   其余三个**结果依实验条件而异**（本项目复现台里 `MSBuildSDKsPath` 单注入出现过一次 exit=1、且未捕获到错误关键字；
   另一套 `-m:2 -nr:false` 实验里三个都 exit=0）⇒ **按未定论记，别据此排除它们**。
   另外"到底哪一步失败"没有日志级证据，"节点复用握手失败"只是**推断**。⚠️ 方向别搞反：这些变量**不一定只来自 VS** —— UAT 自己的 `DotnetProcess`
   也会主动把其中三个设成所选系统 SDK（刻意的边界切换）；而 `MSBuild.bat`→`GetMSBuildPath`→VS
   MSBuild.exe、以及 `InvokeDotNet`（直起系统 dotnet）**都不经过** `GetDotnetPath.bat`。
   所以这条修复的作用域是"自带 dotnet 工具链"这一侧，不是"到处都清一遍"。

   **致命机制（与上面分开看）**：`InvokeCommand` 在**游戏线程**上
   `while (IsProcRunning) { ReadPipe; Sleep(10ms); }`，**没有超时也没有取消** ——
   子进程不退出，界面就永久假死。
   **"环境导致构建失败"与"同步等待没有上限"是两件事**，后者才是"假死"的直接原因。

   **修法**：① 引擎：`GetDotnetPath.bat` 在走自带 SDK 的分支里清掉那 4 个变量（显式
   `UE_USE_SYSTEM_DOTNET=1` 时保留，已实测）；② 插件：**不做总时长上限**，改为**输出静默告警**
   （`StalledOutputWarningSeconds`，默认 60 秒，0=关闭）：连续该秒数没有任何新输出就打一条 Warning，
   内容含 **pid + 生效阈值 + 已耗时**，启动日志也带 pid 与生效阈值。
   刻意不做总时长上限的理由：人不会真的等几十分钟（发现不对劲就去查了），
   而**自动杀进程会毁掉取证现场** —— 这次能查到 `WaitReason=Executive` 正是因为进程还活着。
   **刻意不给插件追加 `-nocompile` / `-nocompileuat`**：实测修好环境后原重路径在编辑器里同样能过
   （真实编辑器内 2.85 秒），而 `-nocompile` 会让 AutomationTool **拒绝编译任何脚本模块**，
   本插件的 `Build/Scripts` 在首次安装 / 脚本更新时正需要它。

   ⚠️ **改这个仓库的 `.bat` 时注释必须纯 ASCII**：`cmd.exe` 用 OEM 代码页（中文 Windows 是 GBK）
   解码批处理，UTF-8 的多字节序列会**吞掉换行**，把下一行的 `rem` 吃掉、后半句当命令执行
   （实测在 `GetDotnetPath.bat` 里写出过 `'CLI' 不是内部或外部命令` 之类垃圾行）。
   `.ps1` 有 BOM 规则，`.bat` 则是「别写非 ASCII」。

   **仍未定论**：并行扫描"具体哪一步握手失败"没有日志级证据；卡住那个 `cmd` 的**内核等待对象**
   也没拿到（只有 `WaitReason=Executive` 这个泛等待）。这两点都不影响上面的干预结论，
   但**不要写成已证实**。
3. **`ProcessEvent` 相关**：不要用"按名字找最派生实现"来推理 `base.` 行为——
   引擎不是那么做的（见 §三）。

---

## 八、C# / C++ / 蓝图 的分工（本项目语境）

- **蓝图**：资产接线、关卡摆放、可视化创作（AnimGraph / 材质 / Niagara / Sequencer），
  以及给 C# 类做子类 —— **蓝图是通往游戏资产的接口**。
- **C#**：玩法和系统逻辑。改 `.cs` 热重载 ~0.1–0.5 s，迭代够快；
  需要 **NuGet 生态 / .NET 类库 / 强类型重构 / 可测试性** 时更合适。
- **C++**：定义反射类型、**把新 API 暴露给反射**（C# 只能访问反射暴露面，限制与蓝图相同）、
  性能热点、改引擎本身；也可以用 C++ 扩展方法给已有的 C# 类加方法。
- **判据**：能暴露的就暴露，别在 C# 里绕；**不要为了"迭代快"去写 C++** —— C# 热重载已经够快。

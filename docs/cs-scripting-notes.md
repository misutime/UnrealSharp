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
| `TSoftObjectPtr<T>.Get()` | **`LoadSynchronous()`** | 比 AS 侧方便（AS 要手写 `LoadObject`） |

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

    public AMyActor() { Speed = 90.0; }        // 默认值在构造函数里给（没有 AS 那种 default 块）

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
2. **编辑器挂起（两次实测）**：主线程停住、日志零增长、无崩溃转储；
   两次都伴随**大量 idle 的 MSBuild 工作节点**（238 / 815 个，本机 28 逻辑核）。
   **因果未证实**；疑似与反复触发 C# 编译（尤其**失败**的那些）有关。
   缓解未验证：用 `MSBUILDDISABLENODEREUSE=1` 启动编辑器。
3. **`ProcessEvent` 相关**：不要用"按名字找最派生实现"来推理 `base.` 行为——
   引擎不是那么做的（见 §三）。

---

## 八、C# 与 AngelScript 的取舍（本项目语境）

- 改 `.cs` 热重载 ~0.1–0.5 s；改 `.as` 热重载 <1 ms（都很适合迭代，别为此写 C++）。
- 需要 **NuGet 生态 / .NET 类库 / 强类型重构** → C# 更合适。
- 需要**接入已有 AngelScript 资产管线**（`Script/` 下现成逻辑）→ 继续用 AS。
- **未验证**：两套脚本同时定义 `UCLASS`、往同一批资产上挂逻辑时是否互相干扰
  （已确认的只是"能同时加载、互不阻塞"）。

using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using UnrealSharp.Core;
using UnrealSharp.Core.Interop;
using UnrealSharp.Interop;

namespace UnrealSharp.StaticVars;

/// <summary>
/// A static variable that has the lifetime of a UWorld. When the world is destroyed, the value is destroyed.
/// For example when traveling between levels, the value is destroyed.
/// </summary>
public sealed class FWorldStaticVar<T> : FBaseStaticVar<T>
{
    [DebuggerBrowsable(DebuggerBrowsableState.Never)]
    private readonly Dictionary<IntPtr, T> _worldToValue = new Dictionary<IntPtr, T>();
    
    [DebuggerBrowsable(DebuggerBrowsableState.Never)]
    private readonly FDelegateHandle _onWorldCleanupHandle;
    
    public FWorldStaticVar()
    {
        FWorldDelegates.FWorldCleanupEvent onWorldCleanupDelegate = OnWorldCleanup;
        IntPtr onWorldCleanup = Marshal.GetFunctionPointerForDelegate(onWorldCleanupDelegate);
        Bind_FWorldDelegates.CallBindOnWorldCleanup(onWorldCleanup, out _onWorldCleanupHandle);
    }
    
    public FWorldStaticVar(T value) : this()
    {
        Value = value;
    }
    
    ~FWorldStaticVar()
    {
        // The finalizer runs off the game thread, where engine access is refused. Unbinding is not something that
        // may be dropped silently: a native record pointing at a managed callback would outlive the delegate. The
        // ALC unloading path below runs on the game thread and unbinds there; this is the safety net, and it
        // reports when even the safety net cannot run.
        if (!EngineCallGuard.TryBeginEngineCall($"{nameof(FWorldStaticVar<T>)}.~FWorldStaticVar"))
        {
            return;
        }

        Bind_FWorldDelegates.CallUnbindOnWorldCleanup(_onWorldCleanupHandle);
    }
    
    public override T? Value
    {
        get => GetWorldValue();
        set => SetWorldValue(value!);
    }
    
    private T? GetWorldValue()
    {
        IntPtr worldPtr = Bind_UCSManager.CallGetCurrentWorldPtr();
        return _worldToValue.GetValueOrDefault(worldPtr);
    }
    
    private void SetWorldValue(T value)
    {
        IntPtr worldPtr = Bind_UCSManager.CallGetCurrentWorldPtr();
        if (_worldToValue.TryAdd(worldPtr, value))
        {
            return;
        }
        
        _worldToValue[worldPtr] = value;
    }
    
    private void OnWorldCleanup(IntPtr world, NativeBool sessionEnded, NativeBool cleanupResources)
    {
        _worldToValue.Remove(world);
    }

#if WITH_EDITOR
    protected override void OnAlcUnloading(AssemblyLoadContext alc)
    {
        base.OnAlcUnloading(alc);
        Bind_FWorldDelegates.CallUnbindOnWorldCleanup(_onWorldCleanupHandle);
    }
#endif
}
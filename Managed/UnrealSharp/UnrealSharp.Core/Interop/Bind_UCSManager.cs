using UnrealSharp.Binds;

namespace UnrealSharp.Core.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_UCSManager
{
    [GameThreadEntry]
    public static delegate* unmanaged<IntPtr, IntPtr> FindManagedObject;
    [GameThreadEntry]
    public static delegate* unmanaged<IntPtr, IntPtr, IntPtr> FindOrCreateManagedInterfaceWrapper;
    public static delegate* unmanaged<IntPtr> GetCurrentWorldContext;
    public static delegate* unmanaged<IntPtr> GetCurrentWorldPtr;
    public static delegate* unmanaged<int> IsOnGameThread;
    public static delegate* unmanaged<int> IsCollectingGarbage;

    /// <summary>Packed engine-call state: bit0 = game thread, bit1 = garbage collection running, bit2 = GC locking the object hash tables.</summary>
    public static delegate* unmanaged<int> GetEngineCallState;
    /// <summary>Number of refusals recorded by the native guards (Tier A).</summary>
    public static delegate* unmanaged<int> GetThreadRefusalCount;
    /// <summary>Number of violations caught at boundaries where an escaping exception would kill the process.</summary>
    public static delegate* unmanaged<int> GetThreadBoundaryCatchCount;
    /// <summary>Non-throwing recording entry for finalizer / native callback boundaries.</summary>
    public static delegate* unmanaged<string, void> ReportBoundaryCatch;
    /// <summary>Refusal accounting for the managed guard, which refuses before any native guard can observe it.</summary>
    public static delegate* unmanaged<string, void> RecordManagedRefusal;
    /// <summary>Non-zero when the managed half of the thread guard self test should run once at startup.</summary>
    public static delegate* unmanaged<int> ShouldRunManagedThreadSelfTest;
    /// <summary>Non-zero when the self test may also collect objects whose finalizers touch engine state.</summary>
    public static delegate* unmanaged<int> ShouldRunFinalizerThreadSelfTest;
    /// <summary>Non-zero when the development-only (Tier B) checks are enabled. Read once, from the startup snapshot.</summary>
    public static delegate* unmanaged<int> IsTierBEnabled;
    
    public static UnrealSharpObject WorldContextObject
    {
        get
        {
            IntPtr worldContextObject = CallGetCurrentWorldContext();
            IntPtr handle = CallFindManagedObject(worldContextObject);
            return GCHandleUtilities.GetObjectFromHandlePtr<UnrealSharpObject>(handle)!;
        }
    }
}
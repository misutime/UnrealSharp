using UnrealSharp.Binds;

namespace UnrealSharp.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_Async
{
    public static delegate* unmanaged<WeakObjectData, int, IntPtr, void> RunOnThread;
    public static delegate* unmanaged<IntPtr, void> RunOnGameThread;
    /// <summary>Non-zero when the game thread took ownership of the delegate handle; zero when it refused.</summary>
    public static delegate* unmanaged<IntPtr, int> TryRunOnGameThread;
    public static delegate* unmanaged<int> GetCurrentNamedThread;
}
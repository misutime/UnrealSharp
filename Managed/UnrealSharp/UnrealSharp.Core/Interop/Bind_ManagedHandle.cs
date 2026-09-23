using UnrealSharp.Binds;

namespace UnrealSharp.Core.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_ManagedHandle
{
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, void> StoreManagedHandle;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr> LoadManagedHandle;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, int, void> StoreUnmanagedMemory;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, int, void> LoadUnmanagedMemory;
}

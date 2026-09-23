using UnrealSharp.Binds;

namespace UnrealSharp.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_UScriptStruct
{
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, int> GetNativeStructSize;

    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, IntPtr, bool> NativeCopy;
    
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, bool> NativeDestroy;

    [TierAGuarded]
    public static delegate* unmanaged<ref NativeStructHandleData, IntPtr, void> AllocateNativeStruct;

    [TierAGuarded]
    public static delegate* unmanaged<ref NativeStructHandleData, IntPtr, void> DeallocateNativeStruct;
    
    [TierAGuarded]
    public static delegate* unmanaged<NativeStructHandleData*, IntPtr, IntPtr> GetStructLocation;
    
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr> GetManagedStructType;
}

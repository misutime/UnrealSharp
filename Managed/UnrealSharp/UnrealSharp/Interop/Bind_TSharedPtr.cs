using UnrealSharp.Binds;

namespace UnrealSharp.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_TSharedPtr
{
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, void> AddSharedReference;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, void> ReleaseSharedReference;
}

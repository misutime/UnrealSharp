using UnrealSharp.Binds;

namespace UnrealSharp.Core.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_UCoreUObject
{
    [TierAGuarded]
    public static delegate* unmanaged<string, string?, string, byte, IntPtr> GetNativeField;
    [TierAGuarded]
    public static delegate* unmanaged<string, string?, string, IntPtr> GetNativeDelegate;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr> GetGeneratedClassFromSkeleton;
}

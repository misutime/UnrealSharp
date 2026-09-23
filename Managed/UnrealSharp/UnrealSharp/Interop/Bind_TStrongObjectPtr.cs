using UnrealSharp.Binds;

namespace UnrealSharp.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_TStrongObjectPtr
{
    [TierAGuarded]
    public static delegate* unmanaged<ref FStrongObjectPtr, IntPtr, void> ConstructStrongObjectPtr;
    [TierAGuarded]
    public static delegate* unmanaged<ref FStrongObjectPtr, void> DestroyStrongObjectPtr;
}

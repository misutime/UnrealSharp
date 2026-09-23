using UnrealSharp.Binds;
using UnrealSharp.Core;

namespace UnrealSharp.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_FMulticastDelegateProperty
{
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, IntPtr, string, void> AddDelegate;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, NativeBool> IsBound;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, ref UnmanagedArray, void> ToString;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, IntPtr, string, void> RemoveDelegate;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, void> ClearDelegate;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, IntPtr, void> BroadcastDelegate;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr> GetSignatureFunction;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, IntPtr, string, NativeBool> ContainsDelegate; 
}

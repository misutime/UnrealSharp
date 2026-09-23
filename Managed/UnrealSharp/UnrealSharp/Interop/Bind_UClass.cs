using UnrealSharp.Binds;
using UnrealSharp.Core;

namespace UnrealSharp.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_UClass
{
    [GameThreadEntry]
    public static delegate* unmanaged<IntPtr, string, IntPtr> GetNativeFunctionFromClassAndName;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, string, IntPtr> GetNativeFunctionFromInstanceAndName;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, string, IntPtr> GetFirstNativeImplementationFromInstanceAndName;
    [GameThreadEntry]
    public static delegate* unmanaged<IntPtr, IntPtr> GetDefault;
    [GameThreadEntry]
    public static delegate* unmanaged<IntPtr, IntPtr> GetDefaultFromInstance;
    
    public static delegate* unmanaged<IntPtr, IntPtr, NativeBool> IsChildOf;
}

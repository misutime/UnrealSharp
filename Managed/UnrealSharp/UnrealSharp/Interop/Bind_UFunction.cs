using UnrealSharp.Binds;
using UnrealSharp.Core;

namespace UnrealSharp.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_UFunction
{
    public static delegate* unmanaged<IntPtr, UInt16> GetNativeFunctionParamsSize;
    // A null result would be cached as "the specialization of this signature", and every later call would keep
    // handing out the null, so the wrapper refuses it instead of returning it.
    [GameThreadEntry(NoNullResult = true)]
    public static delegate* unmanaged<IntPtr, IntPtr, IntPtr, IntPtr> CreateNativeFunctionCustomStructSpecialization;
    public static delegate* unmanaged<IntPtr, IntPtr, void> InitializeFunctionParams;
}
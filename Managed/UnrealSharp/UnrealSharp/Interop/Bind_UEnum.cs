using UnrealSharp.Binds;
using UnrealSharp.Core;

namespace UnrealSharp.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_UEnum
{
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr> GetManagedEnumType;
}

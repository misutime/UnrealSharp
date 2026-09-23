using UnrealSharp.Binds;
using UnrealSharp.Core;

namespace UnrealSharp.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_UDataTable
{
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, FName, IntPtr> GetRow;
}

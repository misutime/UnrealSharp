using UnrealSharp.Binds;

namespace UnrealSharp.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_UAssetManager
{
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr> GetAssetManager;
}

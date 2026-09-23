using UnrealSharp.Binds;
using UnrealSharp.EnhancedInput;

namespace UnrealSharp.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_FSoftObjectPtr
{
    // A null result is legal here (nothing to load), so the wrapper only refuses; the caller keeps the
    // distinction because it asks for the state before it decides what a null means.
    [GameThreadEntry]
    public static delegate* unmanaged<ref FPersistentObjectPtrData<FSoftObjectPathUnsafe>, IntPtr> LoadSynchronous;
}
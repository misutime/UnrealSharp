using UnrealSharp.Binds;
using UnrealSharp.Core;

namespace UnrealSharp.Editor.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_FUnrealSharpEditorModule
{
    [TierAGuarded]
    public static delegate* unmanaged<FManagedUnrealSharpEditorCallbacks, void> InitializeUnrealSharpEditorCallbacks;
    public static delegate* unmanaged<out UnmanagedArray, void> GetProjectPaths;
    [GameThreadEntry]
    public static delegate* unmanaged<IntPtr, ECSTypeStructuralFlags, void> DirtyUnrealType;
    [GameThreadEntry]
    public static delegate* unmanaged<void> NotifyNewType;
}

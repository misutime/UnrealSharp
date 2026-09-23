using UnrealSharp.Binds;
using UnrealSharp.Core;

namespace UnrealSharp.Interop;

[NativeCallbacks] 
public static unsafe partial class Bind_FProperty
{
    [GameThreadEntry]
    public static delegate* unmanaged<IntPtr, string, IntPtr> GetNativePropertyFromName;
    [GameThreadEntry(NonNegativeResult = true)]
    public static delegate* unmanaged<IntPtr, int> GetPropertyOffset;
    [GameThreadEntry(NonNegativeResult = true)]
    public static delegate* unmanaged<IntPtr, int> GetSize;
    [GameThreadEntry(NonNegativeResult = true)]
    public static delegate* unmanaged<IntPtr, int> GetArrayDim;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, void> DestroyValue;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, void> DestroyValue_InContainer;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, void> InitializeValue;
    // Covered transitively: the first thing these do is call the guarded reflection lookup above, so they already
    // refuse in every configuration and must not get the development-only diagnostic on top of them.
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, string, int> GetPropertyOffsetFromName;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, string, int> GetPropertyArrayDimFromName;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, ref UnmanagedArray, void> GetInnerFields;
    public static delegate* unmanaged<IntPtr, IntPtr, IntPtr, NativeBool> Identical;
    public static delegate* unmanaged<IntPtr, IntPtr, uint> GetValueTypeHash;
    public static delegate* unmanaged<IntPtr, NativePropertyFlags, NativeBool> HasAnyPropertyFlags;
    public static delegate* unmanaged<IntPtr, NativePropertyFlags, NativeBool> HasAllPropertyFlags;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, IntPtr, void> CopySingleValue;
    public static delegate* unmanaged<IntPtr, IntPtr, IntPtr, void> GetValue_InContainer;
    public static delegate* unmanaged<IntPtr, IntPtr, IntPtr, void> SetValue_InContainer;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, string, byte> GetBoolPropertyFieldMaskFromName;
    [TierAGuarded]
    public static delegate* unmanaged<IntPtr, IntPtr, void> BroadcastFieldValueChanged;

}

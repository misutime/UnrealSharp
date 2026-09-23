using System.Runtime.InteropServices;
using UnrealSharp.Core;
using UnrealSharp.Core.Interop;
using UnrealSharp.UnrealSharpCore;

namespace UnrealSharp;

public class ManagedObjectMarshaller<T>
{
    public static void ToNative(IntPtr nativeBuffer, int arrayIndex, T? obj)
    {
        // Refused before the handle is allocated and before the previous owner of the target handle is
        // overwritten: both are state changes that a later refusal could not undo.
        EngineCallGuard.EnsureEngineCallAllowed(nameof(ToNative));

        GCHandle handle = obj is not null ? GCHandle.Alloc(obj, GCHandleType.Normal) : GCHandle.FromIntPtr(0);
        Bind_ManagedHandle.CallStoreManagedHandle(GCHandle.ToIntPtr(handle), nativeBuffer + arrayIndex * FSharedGCHandle.GetNativeDataSize());
    }
    
    public static T? FromNative(IntPtr nativeBuffer, int arrayIndex)
    {
        if (nativeBuffer == IntPtr.Zero)
        {
            return default!;
        }
        
        IntPtr handle = Bind_ManagedHandle.CallLoadManagedHandle(nativeBuffer + arrayIndex * FSharedGCHandle.GetNativeDataSize());
        return GCHandleUtilities.GetObjectFromHandlePtr<T>(handle);
    }
}
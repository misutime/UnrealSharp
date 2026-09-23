using UnrealSharp.Core.Interop;

namespace UnrealSharp.Core.Marshallers;

public static class ObjectMarshaller<T> where T : UnrealSharpObject
{
    public static void ToNative(IntPtr nativeBuffer, int arrayIndex, T? obj)
    {
        IntPtr nativeTObjectPtr = nativeBuffer + arrayIndex * IntPtr.Size;
        Bind_TObjectPtr.CallSetTObjectPtrPropertyValue(nativeTObjectPtr, obj?.NativeObject ?? IntPtr.Zero);
    }
    
    public static T FromNative(IntPtr nativeBuffer, int arrayIndex)
    {
        // Reading an object reference out of an engine property is a read of engine memory; the lookup that
        // follows is covered by the always-on boundary, the raw pointer read is not.
        TierBChecks.Check(nameof(ObjectMarshaller<T>), nameof(FromNative));

        IntPtr uObjectPointer = BlittableMarshaller<IntPtr>.FromNative(nativeBuffer, arrayIndex);
        
        if (uObjectPointer == IntPtr.Zero)
        {
            return null!;
        }
        
        IntPtr handle = Bind_UCSManager.CallFindManagedObject(uObjectPointer);
        return GCHandleUtilities.GetObjectFromHandlePtr<T>(handle)!;
    }
}

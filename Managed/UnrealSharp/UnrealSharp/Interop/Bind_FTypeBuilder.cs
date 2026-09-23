using System.Runtime.InteropServices;
using UnrealSharp.Binds;
using UnrealSharp.Core;

namespace UnrealSharp.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_FTypeBuilder
{
    [TierAGuarded]
    public static delegate* unmanaged<char*, char*, char*, char*, byte, IntPtr, void> RegisterManagedType_Native;
    
    public static void RegisterManagedType(Type type, string jsonString, byte fieldType)
    {
        // Refused before the strong handle is allocated: the native entry point has no result channel, so a
        // refused handoff would leave the handle without an owner. The thread state cannot change between this
        // check and the native guard on the same thread, so passing here means the native side accepts it too.
        EngineCallGuard.EnsureEngineCallAllowed(nameof(RegisterManagedType));

        IntPtr handlePtr = GCHandle.ToIntPtr(GCHandleUtilities.AllocateStrongPointer(type, type.Assembly));
        
        fixed (char* nTypeName = type.Name)
        fixed (char* nNamespace = type.Namespace)
        fixed (char* nAssemblyName = type.Assembly.GetName().Name)
        fixed (char* nJson = jsonString)
        {
            RegisterManagedType_Native(nTypeName, nNamespace, nAssemblyName, nJson, fieldType, handlePtr);
        }
    }
}

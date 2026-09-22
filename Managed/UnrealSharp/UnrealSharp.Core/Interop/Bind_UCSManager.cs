using UnrealSharp.Binds;

namespace UnrealSharp.Core.Interop;

[NativeCallbacks]
public static unsafe partial class Bind_UCSManager
{
    public static delegate* unmanaged<IntPtr, IntPtr> FindManagedObject;
    public static delegate* unmanaged<IntPtr, IntPtr, IntPtr> FindOrCreateManagedInterfaceWrapper;
    public static delegate* unmanaged<IntPtr> GetCurrentWorldContext;
    public static delegate* unmanaged<IntPtr> GetCurrentWorldPtr;
    public static delegate* unmanaged<int> IsOnGameThread;
    public static delegate* unmanaged<int> IsCollectingGarbage;
    
    public static UnrealSharpObject WorldContextObject
    {
        get
        {
            IntPtr worldContextObject = CallGetCurrentWorldContext();
            IntPtr handle = CallFindManagedObject(worldContextObject);
            return GCHandleUtilities.GetObjectFromHandlePtr<UnrealSharpObject>(handle)!;
        }
    }
}
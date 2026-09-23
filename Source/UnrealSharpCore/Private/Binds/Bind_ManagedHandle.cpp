#include "CSBindsRegistry.h"
#include "CSManagedGCHandle.h"
#include "CSThreadDiagnostics.h"
#include "CSUnmanagedDataStore.h"

DECLARE_UNREALSHARP_BINDER(Bind_ManagedHandle)
{
    // Storing over a shared handle releases the previous owner and can call back into an arbitrary IDisposable,
    // so a refusal must leave the destination untouched: a half overwritten shared handle has no owner.
    void StoreManagedHandle(const FGCHandleIntPtr Handle, FSharedGCHandle& Destination)
    {
        if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_ManagedHandle::StoreManagedHandle")))
        {
            return;
        }

        Destination = FSharedGCHandle(Handle);
    }

    // Reading shared storage is not automatically safe just because it creates nothing. A refusal answers with
    // the neutral handle so the caller cannot mistake the refusal for a valid handle.
    FGCHandleIntPtr LoadManagedHandle(const FSharedGCHandle& Source)
    {
        if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_ManagedHandle::LoadManagedHandle")))
        {
            return FGCHandleIntPtr{};
        }

        return Source.GetHandle();
    }

    // The unmanaged data store owns engine allocated storage, so both directions are engine access of their own:
    // writing may allocate or overwrite, reading may hand back bytes of a store that the game thread is changing.
    void StoreUnmanagedMemory(const void* Source, FUnmanagedDataStore& Destination, const int32 Size)
    {
        if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_ManagedHandle::StoreUnmanagedMemory")))
        {
            return;
        }

        check(Size > 0)
        Destination.CopyDataIn(Source, Size);
    }

    void LoadUnmanagedMemory(const FUnmanagedDataStore& Source, void* Destination, const int32 Size)
    {
        if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_ManagedHandle::LoadUnmanagedMemory")))
        {
            return;
        }

        check(Size > 0)
        Source.CopyDataOut(Destination, Size);
    }
    
    BIND_UNREALSHARP_FUNCTION(StoreManagedHandle)
    BIND_UNREALSHARP_FUNCTION(LoadManagedHandle)
    BIND_UNREALSHARP_FUNCTION(StoreUnmanagedMemory)
    BIND_UNREALSHARP_FUNCTION(LoadUnmanagedMemory)
}

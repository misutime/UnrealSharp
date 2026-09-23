#include "CSBindsRegistry.h"
#include "CSThreadDiagnostics.h"

DECLARE_UNREALSHARP_BINDER(Bind_TStrongObjectPtr)
{
    // Constructing adds a reference to the object and destroying releases it, so both follow the refcount rule:
    // a refusal changes nothing, and it is the managed owner that keeps the (never released) reference and its
    // responsibility, rather than the handle being left without an owner.
    void ConstructStrongObjectPtr(TStrongObjectPtr<UObject>* Ptr, UObject* Object)
    {
        if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_TStrongObjectPtr::ConstructStrongObjectPtr")))
        {
            return;
        }

        static_assert(sizeof(TStrongObjectPtr<UObject>) == sizeof(UObject*), "TStrongObjectPtr<UObject> must be the same size as UObject*");
        check(Ptr != nullptr);
        std::construct_at(Ptr, Object);
    }

    void DestroyStrongObjectPtr(TStrongObjectPtr<UObject>* Ptr)
    {
        if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_TStrongObjectPtr::DestroyStrongObjectPtr")))
        {
            return;
        }

        static_assert(sizeof(TStrongObjectPtr<UObject>) == sizeof(UObject*), "TStrongObjectPtr<UObject> must be the same size as UObject*");
        check(Ptr != nullptr);
        std::destroy_at(Ptr);
    }
    
    BIND_UNREALSHARP_FUNCTION(ConstructStrongObjectPtr)
    BIND_UNREALSHARP_FUNCTION(DestroyStrongObjectPtr)
}

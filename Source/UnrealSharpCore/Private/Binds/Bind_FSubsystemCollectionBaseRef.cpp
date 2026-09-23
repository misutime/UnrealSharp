#include "CSBindsRegistry.h"
#include "CSThreadDiagnostics.h"

DECLARE_UNREALSHARP_BINDER(Bind_FSubsystemCollectionBaseRef)
{
    // Initializing a dependency instantiates the subsystem, so it is a construction path.
    USubsystem* InitializeDependency(FSubsystemCollectionBase* Collection, UClass* SubsystemClass)
    {
        if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FSubsystemCollectionBaseRef::InitializeDependency")))
        {
            return nullptr;
        }

        if (Collection == nullptr || SubsystemClass == nullptr)
        {
            return nullptr;
        }

        return Collection->InitializeDependency(SubsystemClass);
    }
    
    BIND_UNREALSHARP_FUNCTION(InitializeDependency)
}

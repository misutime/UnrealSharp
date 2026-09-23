#include "CSBindsRegistry.h"
#include "CSManager.h"
#include "CSThreadDiagnostics.h"

DECLARE_UNREALSHARP_BINDER(Bind_FSoftObjectPtr)
{
	// A null pointer or a failed load is a legal result here, so the refusal check comes first to keep
	// "refused" distinguishable from "nothing to load".
	void* LoadSynchronous(const TSoftObjectPtr<UObject>* SoftObjectPtr)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FSoftObjectPtr::LoadSynchronous")))
		{
			return nullptr;
		}

		if (SoftObjectPtr == nullptr || SoftObjectPtr->IsNull())
		{
			return nullptr;
		}
	
		UObject* LoadedObject = SoftObjectPtr->LoadSynchronous();
		return UCSManager::Get().FindManagedObject(LoadedObject);
	}
	
	BIND_UNREALSHARP_FUNCTION(LoadSynchronous)
}



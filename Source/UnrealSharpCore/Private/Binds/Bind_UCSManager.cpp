#include "CSBindsRegistry.h"
#include "CSManager.h"
#include "UObject/UObjectGlobals.h"

DECLARE_UNREALSHARP_BINDER(Bind_UCSManager)
{
	// Authoritative answers for managed code. The managed thread check that goes through the task graph
	// (Bind_Async::GetCurrentNamedThread) cannot be relied on for deciding whether a thread may touch
	// engine state, so managed callers ask here instead.
	int IsOnGameThread()
	{
		return ::IsInGameThread() ? 1 : 0;
	}

	int IsCollectingGarbage()
	{
		return ::IsGarbageCollecting() ? 1 : 0;
	}

	void* FindManagedObject(UObject* Object)
	{
		return UCSManager::Get().FindManagedObject(Object);
	}

	void* FindOrCreateManagedInterfaceWrapper(UObject* Object, UClass* NativeClass)
	{
		return UCSManager::Get().FindManagedInterfaceWrapper(Object, NativeClass);
	}

	void* GetCurrentWorldContext()
	{
		void* WorldContext = UCSManager::Get().GetCurrentWorldContext();
		return WorldContext;
	}

	void* GetCurrentWorldPtr()
	{
		UObject* WorldContext = UCSManager::Get().GetCurrentWorldContext();
		return GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	}
	
	BIND_UNREALSHARP_FUNCTION(FindManagedObject)
	BIND_UNREALSHARP_FUNCTION(FindOrCreateManagedInterfaceWrapper)
	BIND_UNREALSHARP_FUNCTION(GetCurrentWorldContext)
	BIND_UNREALSHARP_FUNCTION(GetCurrentWorldPtr)
	BIND_UNREALSHARP_FUNCTION(IsOnGameThread)
	BIND_UNREALSHARP_FUNCTION(IsCollectingGarbage)
}

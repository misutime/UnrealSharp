#include "CSBindsRegistry.h"
#include "CSManager.h"
#include "CSThreadDiagnostics.h"
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

	// Single packed query for the managed guard: one native round trip instead of one per state bit.
	// Pure query: it does not touch the manager, does not create objects and does not resolve reflection, so it
	// stays valid while the plugin is still bootstrapping.
	int GetEngineCallState()
	{
		return UnrealSharp::ThreadDiagnostics::GetEngineCallState();
	}

	int GetThreadRefusalCount()
	{
		return UnrealSharp::ThreadDiagnostics::GetRefusalCount();
	}

	int GetThreadBoundaryCatchCount()
	{
		return UnrealSharp::ThreadDiagnostics::GetBoundaryCatchCount();
	}

	// Non-throwing recording entry for boundaries where an escaping exception would take the process down
	// (managed finalizers, native to managed callbacks). Logging stays on the engine log, never on the bridge.
	void ReportBoundaryCatch(const char* What)
	{
		UnrealSharp::ThreadDiagnostics::RecordBoundaryCatch(UTF8_TO_TCHAR(What != nullptr ? What : "unknown"));
	}

	// The managed guard refuses before it calls anything, so no native guard observes that refusal and the
	// refusal count would silently under-report. The managed side reports its own refusals here.
	void RecordManagedRefusal(const char* What)
	{
		UnrealSharp::ThreadDiagnostics::RecordRefusal(UTF8_TO_TCHAR(What != nullptr ? What : "unknown"));
	}

	int ShouldRunManagedThreadSelfTest()
	{
		return UnrealSharp::ThreadDiagnostics::ShouldRunManagedSelfTest() ? 1 : 0;
	}

	int ShouldRunFinalizerThreadSelfTest()
	{
		return UnrealSharp::ThreadDiagnostics::ShouldRunFinalizerSelfTest() ? 1 : 0;
	}

	// Development-only switch, read once from the startup snapshot. Tier A does not consult it.
	int IsTierBEnabled()
	{
		return UnrealSharp::ThreadDiagnostics::IsTierBEnabled() ? 1 : 0;
	}

	void* FindManagedObject(UObject* Object)
	{
		// Checked before any engine access: the manager dereferences the object, reads its unique id and
		// touches the shared object handle map before it can decide whether the cache hits. A cache hit is
		// not a proof of lifecycle or concurrency safety, so this cannot live behind the cache lookup.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UCSManager::FindManagedObject")))
		{
			return nullptr;
		}

		return UCSManager::Get().FindManagedObject(Object);
	}

	void* FindOrCreateManagedInterfaceWrapper(UObject* Object, UClass* NativeClass)
	{
		// Independent path from FindManagedObject (it creates its own interface wrapper), so it carries its
		// own check rather than inheriting the other entry point's safety by assumption.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UCSManager::FindOrCreateManagedInterfaceWrapper")))
		{
			return nullptr;
		}

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
	BIND_UNREALSHARP_FUNCTION(GetEngineCallState)
	BIND_UNREALSHARP_FUNCTION(GetThreadRefusalCount)
	BIND_UNREALSHARP_FUNCTION(GetThreadBoundaryCatchCount)
	BIND_UNREALSHARP_FUNCTION(ReportBoundaryCatch)
	BIND_UNREALSHARP_FUNCTION(RecordManagedRefusal)
	BIND_UNREALSHARP_FUNCTION(ShouldRunManagedThreadSelfTest)
	BIND_UNREALSHARP_FUNCTION(ShouldRunFinalizerThreadSelfTest)
	BIND_UNREALSHARP_FUNCTION(IsTierBEnabled)
}

#include "CSBindsRegistry.h"
#include "CSManagedDelegate.h"
#include "CSManagedGCHandle.h"
#include "CoreGlobals.h"
#include "Logging/StructuredLog.h"
#include "UObject/UObjectGlobals.h"

DECLARE_UNREALSHARP_BINDER(Bind_Async)
{
	void RunOnThread(TWeakObjectPtr<UObject> WorldContextObject, ENamedThreads::Type Thread, FGCHandleIntPtr DelegateHandle)
	{
		AsyncTask(Thread, [WorldContextObject, DelegateHandle]()
		{
			FCSManagedDelegate ManagedDelegate = FGCHandle(DelegateHandle);
		
			if (!WorldContextObject.IsValid())
			{
				ManagedDelegate.Dispose();
				return;
			}
		
			ManagedDelegate.Invoke(WorldContextObject.Get());
		});
	}

	int GetCurrentNamedThread()
	{
		return FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
	}

	// Runs a managed delegate on the game thread but without needing a world context object, so it
	// also works while the editor is still starting up. Managed code that resumes on a thread pool
	// thread must route engine-facing work through here: creating objects off the game thread is
	// illegal and the engine aborts when it happens while a garbage collection is running.
	// Being on the game thread is not enough by itself - a garbage collection can already be running
	// there (or be started from a callback), and the engine treats object creation during it as fatal,
	// so that case is queued as well. Note: the delegate is invoked with a null world context; it must
	// not assume the manager has a current world.
	void RunOnGameThread(FGCHandleIntPtr DelegateHandle)
	{
		if (::IsInGameThread() && !::IsGarbageCollecting())
		{
			FCSManagedDelegate ManagedDelegate = FGCHandle(DelegateHandle);
			ManagedDelegate.Invoke(nullptr);
			return;
		}

		AsyncTask(ENamedThreads::GameThread, [DelegateHandle]()
		{
			FCSManagedDelegate ManagedDelegate = FGCHandle(DelegateHandle);
			ManagedDelegate.Invoke(nullptr);
		});
	}

	// Same handoff as RunOnGameThread, but it answers instead of assuming: 1 means the game thread now owns the
	// delegate handle and will dispose it, 0 means the call was refused and the caller still owns the handle.
	// Managed code needs that answer to free the handle itself and to fail the awaiting task, instead of leaving
	// a task pending forever when the queued work will never run (the engine is already exiting).
	int TryRunOnGameThread(FGCHandleIntPtr DelegateHandle)
	{
		if (IsEngineExitRequested())
		{
			UE_LOGFMT(LogUnrealSharp, Warning, "Refusing to dispatch work to the game thread: the engine is exiting.");
			return 0;
		}

		if (::IsInGameThread() && !::IsGarbageCollecting())
		{
			FCSManagedDelegate ManagedDelegate = FGCHandle(DelegateHandle);
			ManagedDelegate.Invoke(nullptr);
			return 1;
		}

		AsyncTask(ENamedThreads::GameThread, [DelegateHandle]()
		{
			FCSManagedDelegate ManagedDelegate = FGCHandle(DelegateHandle);
			ManagedDelegate.Invoke(nullptr);
		});

		return 1;
	}

	BIND_UNREALSHARP_FUNCTION(RunOnThread)
	BIND_UNREALSHARP_FUNCTION(RunOnGameThread)
	BIND_UNREALSHARP_FUNCTION(TryRunOnGameThread)
	BIND_UNREALSHARP_FUNCTION(GetCurrentNamedThread)
}

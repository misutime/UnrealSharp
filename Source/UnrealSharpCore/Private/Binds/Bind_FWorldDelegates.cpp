#include "CSBindsRegistry.h"
#include "CSThreadDiagnostics.h"

DECLARE_UNREALSHARP_BINDER(Bind_FWorldDelegates)
{
	using FWorldCleanupEventDelegate = void(*)(UWorld*, bool, bool);
	
	void BindOnWorldCleanup(FWorldCleanupEventDelegate Delegate, FDelegateHandle* Handle)
	{
		// A registration made from the wrong thread would be one that managed code can never remove again, so
		// the refusal answers with an invalid handle instead of leaving the caller with a handle it can unbind.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FWorldDelegates::BindOnWorldCleanup")))
		{
			*Handle = FDelegateHandle();
			return;
		}

		*Handle = FWorldDelegates::OnWorldCleanup.AddLambda(Delegate);
	}

	// Refusing an unbind leaves a native record pointing at a managed callback, which is why the managed
	// finalizers do not rely on this path: they hand the unbind to the game thread instead, and only report when
	// even that is impossible.
	void UnbindOnWorldCleanup(const FDelegateHandle Handle)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FWorldDelegates::UnbindOnWorldCleanup")))
		{
			return;
		}

		FWorldDelegates::OnWorldCleanup.Remove(Handle);
	}
	
	BIND_UNREALSHARP_FUNCTION(BindOnWorldCleanup)
	BIND_UNREALSHARP_FUNCTION(UnbindOnWorldCleanup)
}

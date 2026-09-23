

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "CSBindsRegistry.h"
#include "CSThreadDiagnostics.h"

using FPIEEvent = void(*)(bool);

DECLARE_UNREALSHARP_BINDER(Bind_FEditorDelegates)
{
	// Same rule as the world cleanup delegates: a registration that cannot be removed again is worse than a
	// refused registration, so a refusal answers with an invalid handle.
	void BindEndPIE(FPIEEvent Delegate, FDelegateHandle* DelegateHandle)
	{
#if WITH_EDITOR
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FEditorDelegates::BindEndPIE")))
		{
			*DelegateHandle = FDelegateHandle();
			return;
		}

		*DelegateHandle = FEditorDelegates::EndPIE.AddLambda(Delegate);
#endif
	}

	void BindStartPIE(FPIEEvent Delegate, FDelegateHandle* DelegateHandle)
	{
#if WITH_EDITOR
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FEditorDelegates::BindStartPIE")))
		{
			*DelegateHandle = FDelegateHandle();
			return;
		}

		*DelegateHandle = FEditorDelegates::BeginPIE.AddLambda(Delegate);
#endif
	}

	void UnbindStartPIE(FDelegateHandle DelegateHandle)
	{
#if WITH_EDITOR
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FEditorDelegates::UnbindStartPIE")))
		{
			return;
		}

		FEditorDelegates::BeginPIE.Remove(DelegateHandle);
#endif
	}

	void UnbindEndPIE(FDelegateHandle DelegateHandle)
	{
#if WITH_EDITOR
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FEditorDelegates::UnbindEndPIE")))
		{
			return;
		}

		FEditorDelegates::EndPIE.Remove(DelegateHandle);
#endif
	}
	
	BIND_UNREALSHARP_FUNCTION(BindEndPIE)
	BIND_UNREALSHARP_FUNCTION(BindStartPIE)
	BIND_UNREALSHARP_FUNCTION(UnbindStartPIE)
	BIND_UNREALSHARP_FUNCTION(UnbindEndPIE)
}

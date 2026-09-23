#include "CSBindsRegistry.h"
#include "CSThreadDiagnostics.h"

DECLARE_UNREALSHARP_BINDER(Bind_TSharedPtr)
{
	// The shared counter is thread safe, but that only proves the counter operation: reaching zero calls the
	// payload's deleter, which is arbitrary engine code. Both directions therefore refuse together, so a refusal
	// can never produce an unpaired reference.
	void AddSharedReference(SharedPointerInternals::TReferenceControllerBase<ESPMode::ThreadSafe>* ReferenceController)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_TSharedPtr::AddSharedReference")))
		{
			return;
		}

		if (!ReferenceController)
		{
			return;
		}
	
		ReferenceController->AddSharedReference();
	}

	void ReleaseSharedReference(SharedPointerInternals::TReferenceControllerBase<ESPMode::ThreadSafe>* ReferenceController)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_TSharedPtr::ReleaseSharedReference")))
		{
			return;
		}

		if (!ReferenceController)
		{
			return;
		}

		ReferenceController->ReleaseSharedReference();
	}
	
	BIND_UNREALSHARP_FUNCTION(AddSharedReference)
	BIND_UNREALSHARP_FUNCTION(ReleaseSharedReference)
	
}

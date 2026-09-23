#include "CSBindsRegistry.h"
#include "CSThreadDiagnostics.h"

DECLARE_UNREALSHARP_BINDER(Bind_IRefCountedObject)
{
	// Refcounting is not exempt: the count itself is atomic, but reaching zero calls an arbitrary deleter, and a
	// refused call must not change the count either - an unbalanced pair would free shared data that is still in
	// use. Both directions refuse together, so a refusal can never produce an unpaired AddRef or Release.
	void AddRef(const IRefCountedObject* Object)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_IRefCountedObject::AddRef")))
		{
			return;
		}

		if (!Object)
		{
			return;

		}
		
		Object->AddRef();
	}

	void Release(const IRefCountedObject* Object)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_IRefCountedObject::Release")))
		{
			return;
		}

		if (!Object)
		{
			return;
		}
		
		Object->Release();
	}
	
	BIND_UNREALSHARP_FUNCTION(AddRef)
	BIND_UNREALSHARP_FUNCTION(Release)
}
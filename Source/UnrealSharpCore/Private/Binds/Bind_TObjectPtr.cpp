#include "CSBindsRegistry.h"
#include "CSThreadDiagnostics.h"

DECLARE_UNREALSHARP_BINDER(Bind_TObjectPtr)
{
	void SetTObjectPtrPropertyValue(TObjectPtr<UObject>* Object, UObject* NewValue)
	{
		// Writing an object pointer property is not a plain store: with the incremental collector a TObjectPtr
		// assignment takes part in the engine's reference tracking, and the target belongs to the engine object.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_TObjectPtr::SetTObjectPtrPropertyValue")))
		{
			return;
		}

		*Object = NewValue;
	}
	
	BIND_UNREALSHARP_FUNCTION(SetTObjectPtrPropertyValue)
}

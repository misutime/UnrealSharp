#include "CSBindsRegistry.h"
#include "CSThreadDiagnostics.h"
#include "UnrealSharpCore.h"
#include "Logging/StructuredLog.h"

#if ENGINE_MINOR_VERSION >= 8
#include "UObject/ScriptDelegateFwd.h"
#endif

DECLARE_UNREALSHARP_BINDER(Bind_FMulticastDelegateProperty)
{
	static FScriptDelegate MakeScriptDelegate(UObject* Target, const char* FunctionName)
	{
		FScriptDelegate NewDelegate;
		NewDelegate.BindUFunction(Target, FunctionName);

		if (!NewDelegate.IsBound())
		{
			UE_LOGFMT(LogUnrealSharp, Warning, "Failed to bind function {FunctionName} on target {TargetName}", FunctionName, *Target->GetName());
		}
		
		return NewDelegate;
	}
	
	const FMulticastScriptDelegate* TryGetSparseMulticastDelegate(FMulticastDelegateProperty* DelegateProperty, const FMulticastScriptDelegate* Delegate)
	{
		// If the delegate is a sparse delegate, we need to get the multicast delegate from FSparseDelegate wrapper.
		if (DelegateProperty->IsA<FMulticastSparseDelegateProperty>())
		{
			Delegate = DelegateProperty->GetMulticastDelegate(Delegate);
		}

		return Delegate;
	}
	
	// Delegate state is engine state: registering a callback from the wrong thread installs a native record
	// pointing at managed code that managed code can no longer remove on its own thread, and broadcasting from
	// there runs arbitrary listeners. Each entry carries its own check; the ones that only read the delegate
	// object do too, because the delegate can be a sparse wrapper owned by the object.
	void AddDelegate(FMulticastDelegateProperty* DelegateProperty, FMulticastScriptDelegate* Delegate, UObject* Target, const char* FunctionName)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FMulticastDelegateProperty::AddDelegate")))
		{
			return;
		}

		FScriptDelegate NewScriptDelegate = MakeScriptDelegate(Target, FunctionName);
		DelegateProperty->AddDelegate(NewScriptDelegate, nullptr, Delegate);
	}

	bool IsBound(FMulticastScriptDelegate* Delegate)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FMulticastDelegateProperty::IsBound")))
		{
			return false;
		}

		return Delegate->IsBound();
	}

	void ToString(FMulticastScriptDelegate* Delegate, FString* OutString)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FMulticastDelegateProperty::ToString")))
		{
			return;
		}

		*OutString = Delegate->ToString<UObject>();
	}

	void RemoveDelegate(FMulticastDelegateProperty* DelegateProperty, FMulticastScriptDelegate* Delegate, UObject* Target, const char* FunctionName)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FMulticastDelegateProperty::RemoveDelegate")))
		{
			return;
		}

		FScriptDelegate NewScriptDelegate = MakeScriptDelegate(Target, FunctionName);
		DelegateProperty->RemoveDelegate(NewScriptDelegate, nullptr, Delegate);
	}

	void ClearDelegate(FMulticastDelegateProperty* DelegateProperty, FMulticastScriptDelegate* Delegate)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FMulticastDelegateProperty::ClearDelegate")))
		{
			return;
		}

		DelegateProperty->ClearDelegate(nullptr, Delegate);
	}

	void BroadcastDelegate(FMulticastDelegateProperty* DelegateProperty, const FMulticastScriptDelegate* Delegate, void* Parameters)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FMulticastDelegateProperty::BroadcastDelegate")))
		{
			return;
		}

		Delegate = TryGetSparseMulticastDelegate(DelegateProperty, Delegate);
#if ENGINE_MINOR_VERSION >= 8
		Delegate->ProcessDelegate<UObject>(Parameters);
#else
		Delegate->ProcessMulticastDelegate<UObject>(Parameters);
#endif
	}

	bool ContainsDelegate(FMulticastDelegateProperty* DelegateProperty, const FMulticastScriptDelegate* Delegate, UObject* Target, const char* FunctionName)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FMulticastDelegateProperty::ContainsDelegate")))
		{
			return false;
		}

		FScriptDelegate NewScriptDelegate = MakeScriptDelegate(Target, FunctionName);
		Delegate = TryGetSparseMulticastDelegate(DelegateProperty, Delegate);
		return Delegate->Contains(NewScriptDelegate);
	}

	void* GetSignatureFunction(FMulticastDelegateProperty* DelegateProperty)
	{
		// Returns reflection metadata (the delegate signature function) that the managed side caches, so it is
		// guarded like the other reflection lookups. The sibling add/remove/broadcast entries work on delegate
		// state and are contracted separately.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FMulticastDelegateProperty::GetSignatureFunction")))
		{
			return nullptr;
		}

		return DelegateProperty->SignatureFunction;
	}
	
	BIND_UNREALSHARP_FUNCTION(AddDelegate)
	BIND_UNREALSHARP_FUNCTION(IsBound)
	BIND_UNREALSHARP_FUNCTION(ToString)
	BIND_UNREALSHARP_FUNCTION(RemoveDelegate)
	BIND_UNREALSHARP_FUNCTION(ClearDelegate)
	BIND_UNREALSHARP_FUNCTION(BroadcastDelegate)
	BIND_UNREALSHARP_FUNCTION(ContainsDelegate)
	BIND_UNREALSHARP_FUNCTION(GetSignatureFunction)
}

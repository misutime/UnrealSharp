#include "CSBindsRegistry.h"
#include "CSManagedAssembly.h"
#include "CSManager.h"
#include "CSThreadDiagnostics.h"
#include "UnrealSharpCore.h"

DECLARE_UNREALSHARP_BINDER(Bind_UClass)
{
	UFunction* GetNativeFunctionFromClassAndName(const UClass* Class, const char* FunctionName)
	{
		// Reflection lookup on an already loaded class: it resolves the class' function map, so it carries its
		// own check instead of relying on the field resolution that produced the class.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UClass::GetNativeFunctionFromClassAndName")))
		{
			return nullptr;
		}

		if (!IsValid(Class))
		{
			UE_LOGFMT(LogUnrealSharp, Warning, "Failed to get NativeFunction for class. Class is not valid. FunctionName: {0}", FunctionName);
			return nullptr;
		}
		
		UFunction* Function = Class->FindFunctionByName(FunctionName);
		
		if (!IsValid(Function))
		{
			UE_LOGFMT(LogUnrealSharp, Warning, "Failed to get NativeFunction. Class: {0}, FunctionName: {1}", *Class->GetName(), FunctionName);
			return nullptr;
		}

		return Function;
	}

	UFunction* GetNativeFunctionFromInstanceAndName(const UObject* NativeObject, const char* FunctionName)
	{
		// Reflection lookup on the instance's class; it carries its own check rather than borrowing the one from
		// the class based variant.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UClass::GetNativeFunctionFromInstanceAndName")))
		{
			return nullptr;
		}

		if (!IsValid(NativeObject))
		{
			UE_LOGFMT(LogUnrealSharp, Warning, "Failed to get NativeFunction. NativeObject is not valid. ObjectName: {0}", *NativeObject->GetName());
			return nullptr;
		}
		
		return NativeObject->FindFunctionChecked(FunctionName);
	}

	UFunction* GetFirstNativeImplementationFromInstanceAndName(const UObject* NativeObject, const char* FunctionName)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UClass::GetFirstNativeImplementationFromInstanceAndName")))
		{
			return nullptr;
		}

		if (!IsValid(NativeObject))
		{
			UE_LOGFMT(LogUnrealSharp, Warning, "Failed to get NativeFunction. NativeObject is not valid. ObjectName: {0}", *NativeObject->GetName());
			return nullptr;
		}
		
		const UClass* FirstNativeClass = FCSClassUtilities::GetFirstNativeClass(NativeObject->GetClass());
		return FirstNativeClass->FindFunctionByName(FunctionName);
	}

	void* GetDefault(UClass* Class)
	{
		// Checked before GetDefaultObject(), which lazily creates the class default object and is therefore a
		// construction path, not a query.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UClass::GetDefault")))
		{
			return nullptr;
		}

		if (!IsValid(Class))
		{
			return nullptr;
		}

		return UCSManager::Get().FindManagedObject(Class->GetDefaultObject());
	}

	void* GetDefaultFromInstance(UObject* Object)
	{
		// Same reason as GetDefault: either branch below can construct a class default object. The check has to
		// happen before IsValid, because validity itself reads engine object state.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UClass::GetDefaultFromInstance")))
		{
			return nullptr;
		}

		if (!IsValid(Object))
		{
			return nullptr;
		}

		UObject* CDO;
		if (UClass* Class = Cast<UClass>(Object))
		{
			CDO = Class->GetDefaultObject();
		}
		else
		{
			CDO = Object->GetClass()->GetDefaultObject();
		}
		
		return UCSManager::Get().FindManagedObject(CDO);
	}

	#if WITH_EDITOR
	UClass* RedirectClassIfNeeded(UClass* Class)
	{
		if (UCSSkeletonClass* ManagedClass = Cast<UCSSkeletonClass>(Class))
		{
			return ManagedClass->GetGeneratedClass();
		}

		return Class;
	}
	#endif

	bool IsChildOf(UClass* ChildClass, UClass* ParentClass)
	{
		 if (!IsValid(ChildClass) || !IsValid(ParentClass))
		 {
			 return false;
		 }
		
	#if WITH_EDITOR
		ChildClass = RedirectClassIfNeeded(ChildClass);
		ParentClass = RedirectClassIfNeeded(ParentClass);
	#endif
		
		 return ChildClass->IsChildOf(ParentClass);
	}
	
	BIND_UNREALSHARP_FUNCTION(GetNativeFunctionFromClassAndName)
	BIND_UNREALSHARP_FUNCTION(GetNativeFunctionFromInstanceAndName)
	BIND_UNREALSHARP_FUNCTION(GetFirstNativeImplementationFromInstanceAndName)
	BIND_UNREALSHARP_FUNCTION(GetDefault)
	BIND_UNREALSHARP_FUNCTION(GetDefaultFromInstance)
	BIND_UNREALSHARP_FUNCTION(IsChildOf)
}

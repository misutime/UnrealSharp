#include "CSBindsRegistry.h"
#include "CSManager.h"
#include "CSThreadDiagnostics.h"
#include "Types/CSEnum.h"

DECLARE_UNREALSHARP_BINDER(Bind_UEnum)
{
	FGCHandleIntPtr GetManagedEnumType(UEnum* ScriptEnum)
	{
		// Type definition resolution touches the manager's registry, so it is refused before the lookup rather
		// than answering with a handle the caller cannot tell apart from "no managed type".
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UEnum::GetManagedEnumType")))
		{
			return FGCHandleIntPtr();
		}

		if (const UCSEnum* CSEnum = Cast<UCSEnum>(ScriptEnum); CSEnum != nullptr)
		{
			return CSEnum->GetManagedTypeDefinition()->GetTypeGCHandle()->GetHandle();
		}

		const UCSManagedAssembly* Assembly = UCSManager::Get().FindOwningAssembly(ScriptEnum);
		if (Assembly == nullptr)
		{
			return FGCHandleIntPtr();
		}

		const FCSFieldName FieldName = FCSFieldName::FromNativeBase(ScriptEnum);
		const TSharedPtr<FCSManagedTypeDefinition> Info = Assembly->FindManagedTypeDefinition(FieldName);
		if (!Info.IsValid())
		{
			return FGCHandleIntPtr();
		}

		return Info->GetTypeGCHandle()->GetHandle();   
	}
	
	BIND_UNREALSHARP_FUNCTION(GetManagedEnumType)
}

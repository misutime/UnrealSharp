#include "CSBindsRegistry.h"
#include "CSProjectUtilities.h"
#include "CSThreadDiagnostics.h"
#include "HotReload/CSHotReloadSubsystem.h"
#include "Logging/StructuredLog.h"
#include "Types/CSManagedTypeInterface.h"
#include "UnrealSharpEditor.h"

DECLARE_UNREALSHARP_BINDER(Bind_FUnrealSharpEditorModule)
{
	void InitializeUnrealSharpEditorCallbacks(FCSManagedEditorCallbacks Callbacks)
	{
		// Wires the native to managed editor callback table: doing that from the wrong thread would install a
		// callback contract that the engine then calls from a thread the managed side does not expect.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FUnrealSharpEditorModule::InitializeUnrealSharpEditorCallbacks")))
		{
			return;
		}

		FUnrealSharpEditorModule::Get().InitializeManagedEditorCallbacks(Callbacks);
	}

	void GetProjectPaths(TArray<FString>* Paths)
	{
		UnrealSharp::Project::GetAllProjectPaths(*Paths);
	}

	// Refused before the cast and before the dirty flags are written, so a refusal never leaves a type marked
	// dirty for a reload that will not happen.
	void DirtyUnrealType(UField* Field, ECSTypeStructuralFlags Flags)
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FUnrealSharpEditorModule::DirtyUnrealType")))
		{
			return;
		}

		if (Field == nullptr)
		{
			return;
		}

		ICSManagedTypeInterface* ManagedTypeInterface = Cast<ICSManagedTypeInterface>(Field);

		if (ManagedTypeInterface == nullptr)
		{
			UE_LOGFMT(LogUnrealSharpEditor, Warning, "Cannot mark {0} dirty: it is not a managed type", *Field->GetName());
			return;
		}

		if (!ManagedTypeInterface->HasManagedTypeDefinition())
		{
			UE_LOGFMT(LogUnrealSharpEditor, Warning, "Cannot mark {0} dirty: it has no managed type definition", *Field->GetName());
			return;
		}

		ManagedTypeInterface->GetManagedTypeDefinition()->SetDirtyFlags(Flags);
	}
	
	void NotifyNewType()
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FUnrealSharpEditorModule::NotifyNewType")))
		{
			return;
		}

		UCSHotReloadSubsystem* HotReloadSubsystem = UCSHotReloadSubsystem::Get();

		if (HotReloadSubsystem == nullptr)
		{
			UE_LOGFMT(LogUnrealSharpEditor, Warning, "Cannot notify a new type: the hot reload subsystem is not available");
			return;
		}

		HotReloadSubsystem->NotifyNewType();
	}
	
	BIND_UNREALSHARP_FUNCTION(InitializeUnrealSharpEditorCallbacks)
	BIND_UNREALSHARP_FUNCTION(GetProjectPaths)
	BIND_UNREALSHARP_FUNCTION(DirtyUnrealType)
	BIND_UNREALSHARP_FUNCTION(NotifyNewType)
}

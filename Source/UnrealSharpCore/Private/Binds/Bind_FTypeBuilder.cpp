#include "CSBindsRegistry.h"
#include "CSManager.h"
#include "CSThreadDiagnostics.h"
#include "Logging/StructuredLog.h"
#include "UnrealSharpCore.h"

DECLARE_UNREALSHARP_BINDER(Bind_FTypeBuilder)
{
	void RegisterManagedType_Native(TCHAR* InFieldName, TCHAR* InNamespace, TCHAR* InAssemblyName, TCHAR* NewJsonReflectionData, ECSFieldType FieldType, uint8* TypeHandle)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UFTypeBuilderExporter::RegisterManagedType_Native);

		// Refused before any state change. This entry point has no result channel, so the caller keeps ownership
		// of the strong handle it already allocated and must release it when the handoff does not happen.
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_FTypeBuilder::RegisterManagedType_Native")))
		{
			return;
		}

		UCSManagedAssembly* Assembly = UCSManager::Get().FindAssembly(InAssemblyName);

		// An unresolved assembly is a real failure: the reflection data has nowhere to be registered.
		if (Assembly == nullptr)
		{
			UE_LOGFMT(LogUnrealSharp, Error, "Cannot register managed type {0}: owning assembly {1} is not loaded", InFieldName, InAssemblyName);
			return;
		}

		Assembly->RegisterManagedType(InFieldName, InNamespace, FieldType, TypeHandle, NewJsonReflectionData);
	}
	
	BIND_UNREALSHARP_FUNCTION(RegisterManagedType_Native)
}

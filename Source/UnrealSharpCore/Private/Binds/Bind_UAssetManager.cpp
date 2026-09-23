#include "CSBindsRegistry.h"
#include "CSManager.h"
#include "CSThreadDiagnostics.h"
#include "Engine/AssetManager.h"

DECLARE_UNREALSHARP_BINDER(Bind_UAssetManager)
{
	void* GetAssetManager()
	{
		if (UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(TEXT("Bind_UAssetManager::GetAssetManager")))
		{
			return nullptr;
		}

		// Answers null while the singleton does not exist yet instead of taking the engine's fatal path; the
		// managed side reports that as a managed exception.
		UAssetManager* AssetManager = UAssetManager::GetIfInitialized();

		if (AssetManager == nullptr)
		{
			return nullptr;
		}

		return UCSManager::Get().FindManagedObject(AssetManager);
	}
	
	BIND_UNREALSHARP_FUNCTION(GetAssetManager)
}

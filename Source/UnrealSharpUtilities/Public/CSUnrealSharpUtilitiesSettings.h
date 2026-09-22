#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CSUnrealSharpUtilitiesSettings.generated.h"

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "UnrealSharp Proc Helper Settings"))
class UCSUnrealSharpUtilitiesSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	
	UCSUnrealSharpUtilitiesSettings()
	{
		CategoryName = "Plugins";
	}
	
	/**
	* Whether to show build warnings in the build error dialog.
	* Only affects the full dotnet build (editor startup / manual rebuild), not the incremental hot reload compiler.
	*/
	UPROPERTY(EditDefaultsOnly, config, Category = "UnrealSharp | Build Output")
	bool bShowBuildWarnings = false;

	/**
	* How long an external command (UAT / dotnet) may produce no output before a warning is logged, naming the
	* child process id so it can be inspected externally. These commands are waited on synchronously (usually on
	* the game thread), so an unnoticed stall is what actually hurts. This only logs and never kills: keeping the
	* stalled process alive is what makes it diagnosable. A cold build may legitimately be silent for a while.
	* Set to 0 to disable.
	*/
	UPROPERTY(EditDefaultsOnly, config, Category = "UnrealSharp | Build", meta = (ClampMin = "0"))
	int32 StalledOutputWarningSeconds = 60;
};

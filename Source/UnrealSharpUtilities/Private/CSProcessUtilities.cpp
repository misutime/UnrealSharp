#include "CSProcessUtilities.h"

#include "CSDotnetUtilties.h"
#include "CSPathsUtilities.h"
#include "CSUnrealSharpUtilitiesSettings.h"
#include "UnrealSharpUtilities.h"
#include "Logging/StructuredLog.h"

bool UnrealSharp::Process::InvokeCommand(const FString& ProgramPath, const FString& Arguments, int32& OutReturnCode, FString& Output, const FString* InWorkingDirectory, const FCSCommandError& OnError)
{
	const double StartTime = FPlatformTime::Seconds();
	const FString ProgramName = FPaths::GetBaseFilename(ProgramPath);
	const FString WorkingDirectory = InWorkingDirectory ? *InWorkingDirectory : FPaths::GetPath(ProgramPath);

	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
    
	if (!FPlatformProcess::CreatePipe(ReadPipe, WritePipe))
	{
		const FString FullError = FString::Printf(TEXT("%s: failed to create pipe."), *ProgramName);
		UE_LOGFMT(LogUnrealSharpUtilities, Error, "{0}", FullError);
        
		if (OnError.IsBound())
		{
			OnError.Execute(FullError);
		}
        
		return false;
	}

	const double StallWarningSeconds = GetDefault<UCSUnrealSharpUtilitiesSettings>()->StalledOutputWarningSeconds;

	uint32 ProcessID = 0;
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(*ProgramPath, 
		*Arguments, 
		false, 
		true, 
		true, 
		&ProcessID,
		0, 
		*WorkingDirectory, 
		WritePipe, 
		nullptr);

	if (!ProcHandle.IsValid())
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
        
		const FString FullError = FString::Printf(TEXT("%s: failed to launch process."), *ProgramName);
		UE_LOGFMT(LogUnrealSharpUtilities, Error, "{0}", FullError);
        
		if (OnError.IsBound())
		{
			OnError.Execute(FullError);
		}
        
		return false;
	}

	// 启动信息里带上 pid 与生效的静默阈值：真卡住时日志自身就给出"该去查哪个进程"的入口，
	// 不必再从外部反查父进程；阈值一起打出来，省得怀疑"配置到底有没有生效"。
	// 处置时只结束这一个子进程即可让调用方（通常是游戏线程）恢复。
	UE_LOGFMT(LogUnrealSharpUtilities, Display, "Running {0} (pid {1}, stall warning after {2}s) in \"{3}\": {4}",
		ProgramName, ProcessID, int32(StallWarningSeconds), WorkingDirectory, Arguments);

	Output.Reset();

	// 输出静默检测：真卡住时人不会等几十分钟才反应，而是在觉得不对劲的那一刻就想知道
	// "该去查哪个进程"。所以这里只**记录**、绝不结束进程 —— 保留现场才谈得上取证。
	// 冷构建（例如从零编译 UBT/UAT）本来就可能长时间没有输出，所以定级是告警而非错误。
	double LastOutputTime = FPlatformTime::Seconds();
	double LastStallLogTime = LastOutputTime;

	while (FPlatformProcess::IsProcRunning(ProcHandle))
	{
		const FString Chunk = FPlatformProcess::ReadPipe(ReadPipe);
		if (!Chunk.IsEmpty())
		{
			Output += Chunk;
			LastOutputTime = FPlatformTime::Seconds();
		}
		FPlatformProcess::Sleep(0.01f);

		if (StallWarningSeconds > 0.0)
		{
			const double Now = FPlatformTime::Seconds();
			if ((Now - LastOutputTime) >= StallWarningSeconds && (Now - LastStallLogTime) >= StallWarningSeconds)
			{
				LastStallLogTime = Now;
				UE_LOGFMT(LogUnrealSharpUtilities, Warning,
					"{0} (pid {1}) has produced no output for {2}s (elapsed {3}s). If this is not a cold build, inspect that pid before killing it.",
					ProgramName, ProcessID, int32(Now - LastOutputTime), int32(Now - StartTime));
			}
		}
	}
	Output += FPlatformProcess::ReadPipe(ReadPipe);

	FPlatformProcess::GetProcReturnCode(ProcHandle, &OutReturnCode);
	FPlatformProcess::CloseProc(ProcHandle);
	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

	if (OutReturnCode != 0)
	{
		const FString FullError = FString::Printf(TEXT("%s task failed:\n%s"), *ProgramName, *Output);
		UE_LOGFMT(LogUnrealSharpUtilities, Error, "{0}", FullError);
        
		if (OnError.IsBound())
		{
			OnError.Execute(FullError);
		}
        
		return false;
	}

	const double ElapsedTime = FPlatformTime::Seconds() - StartTime;
	UE_LOGFMT(LogUnrealSharpUtilities, Display, "{0} task completed in {1} seconds.", ProgramName, ElapsedTime);
	return true;
}

bool UnrealSharp::Process::InvokeDotNet(const FString& Arguments, const FString* InWorkingDirectory, const FCSCommandError& OnError)
{
	FString Output;
	int32 OutReturnCode = 0;
	return InvokeCommand(DotNetUtilities::GetDotNetExecutablePath(), Arguments, OutReturnCode, Output, InWorkingDirectory, OnError);
}

bool UnrealSharp::Process::InvokeDotNetBuild(const FString& RootFolder, const FString& AdditionalArguments, const FCSCommandError& OnError)
{
	const FString Args = FString::Printf(TEXT("build \"%s\" %s"), *RootFolder, *AdditionalArguments);
	return InvokeDotNet(Args, nullptr, OnError);
}

bool UnrealSharp::Process::InvokeDotNetBuild(const FCSCommandError& OnError)
{
	return InvokeDotNetBuild(Paths::GetScriptFolderDirectory(), {}, OnError);
}

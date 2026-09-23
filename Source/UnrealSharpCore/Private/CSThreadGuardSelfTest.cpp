#include "CSBindsRegistry.h"
#include "CSThreadDiagnostics.h"
#include "UnrealSharpCore.h"

#include "Async/Async.h"
#include "HAL/Event.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTLS.h"
#include "Logging/StructuredLog.h"

#if !UE_BUILD_SHIPPING

/**
 * Deterministic verification of the thread guard.
 *
 * Every probe below is answered by the guard before it can touch its argument, and every probe is also safe to
 * reach without the guard, so the test never has to crash an editor to prove the guard works. What it asserts is
 * the observable difference: a refused call increments the refusal counter exactly once, a legal call does not,
 * and the counters are the same ones the managed side reads.
 *
 * The probes go through the binder registry, so they call the same function pointers the managed wrappers call
 * rather than a copy of the guard.
 */
namespace
{
	using FGetDefaultFn = void* (*)(void*);
	using FStaticLoadClassFn = void* (*)(void*, void*, const char*);
	using FAddSharedReferenceFn = void (*)(void*);
	using FRecordManagedRefusalFn = void (*)(const char*);

	struct FProbeArguments
	{
		FGetDefaultFn GetDefault = nullptr;
		FStaticLoadClassFn StaticLoadClass = nullptr;
		FAddSharedReferenceFn AddSharedReference = nullptr;
		FRecordManagedRefusalFn RecordManagedRefusal = nullptr;
	};

	struct FProbeOutcome
	{
		int32 GameThreadBit = 0;
		bool bGetDefaultReturnedNull = false;
		bool bStaticLoadClassReturnedNull = false;
		int32 GetDefaultRefusals = 0;
		int32 StaticLoadClassRefusals = 0;
		int32 AddSharedReferenceRefusals = 0;
		int32 ManagedRefusalRefusals = 0;
	};

	FProbeOutcome RunProbes(const FProbeArguments& Arguments)
	{
		FProbeOutcome Outcome;
		Outcome.GameThreadBit = UnrealSharp::ThreadDiagnostics::GetEngineCallState() & 1;

		int32 Before = UnrealSharp::ThreadDiagnostics::GetRefusalCount();
		Outcome.bGetDefaultReturnedNull = Arguments.GetDefault(nullptr) == nullptr;
		Outcome.GetDefaultRefusals = UnrealSharp::ThreadDiagnostics::GetRefusalCount() - Before;

		Before = UnrealSharp::ThreadDiagnostics::GetRefusalCount();
		Outcome.bStaticLoadClassReturnedNull = Arguments.StaticLoadClass(nullptr, nullptr, nullptr) == nullptr;
		Outcome.StaticLoadClassRefusals = UnrealSharp::ThreadDiagnostics::GetRefusalCount() - Before;

		Before = UnrealSharp::ThreadDiagnostics::GetRefusalCount();
		Arguments.AddSharedReference(nullptr);
		Outcome.AddSharedReferenceRefusals = UnrealSharp::ThreadDiagnostics::GetRefusalCount() - Before;

		Before = UnrealSharp::ThreadDiagnostics::GetRefusalCount();
		Arguments.RecordManagedRefusal("self test managed refusal");
		Outcome.ManagedRefusalRefusals = UnrealSharp::ThreadDiagnostics::GetRefusalCount() - Before;

		return Outcome;
	}

	bool ResolveProbes(FProbeArguments& OutArguments, FOutputDevice& Out)
	{
		// Parameter sizes match the sums the binder registration computes from the signatures.
		OutArguments.GetDefault = reinterpret_cast<FGetDefaultFn>(FCSBindsRegistry::GetBoundFunction(TEXT("Bind_UClass"), TEXT("GetDefault"), sizeof(void*) + sizeof(void*)));
		OutArguments.StaticLoadClass = reinterpret_cast<FStaticLoadClassFn>(FCSBindsRegistry::GetBoundFunction(TEXT("Bind_UObject"), TEXT("StaticLoadClass"), sizeof(void*) + sizeof(void*) + sizeof(void*) + sizeof(char*)));
		OutArguments.AddSharedReference = reinterpret_cast<FAddSharedReferenceFn>(FCSBindsRegistry::GetBoundFunction(TEXT("Bind_TSharedPtr"), TEXT("AddSharedReference"), sizeof(void*)));
		OutArguments.RecordManagedRefusal = reinterpret_cast<FRecordManagedRefusalFn>(FCSBindsRegistry::GetBoundFunction(TEXT("Bind_UCSManager"), TEXT("RecordManagedRefusal"), sizeof(char*)));

		const bool bResolved = OutArguments.GetDefault != nullptr
			&& OutArguments.StaticLoadClass != nullptr
			&& OutArguments.AddSharedReference != nullptr
			&& OutArguments.RecordManagedRefusal != nullptr;

		if (!bResolved)
		{
			Out.Logf(TEXT("  FAIL  the binder registry does not expose every probe entry point"));
			UE_LOGFMT(LogUnrealSharp, Error, "Thread guard self test cannot run: a probe entry point is not registered.");
		}

		return bResolved;
	}

	void ReportOutcome(FOutputDevice& Out, int32& FailureCount, const TCHAR* Phase, const FProbeOutcome& Outcome, bool bExpectRefusal)
	{
		const bool bExpectGameThreadBit = !bExpectRefusal;
		const int32 ExpectedRefusals = bExpectRefusal ? 1 : 0;

		if (Outcome.GameThreadBit != (bExpectGameThreadBit ? 1 : 0))
		{
			++FailureCount;
			Out.Logf(TEXT("  FAIL  %s: game thread bit is %d"), Phase, Outcome.GameThreadBit);
		}

		if (!Outcome.bGetDefaultReturnedNull || !Outcome.bStaticLoadClassReturnedNull)
		{
			++FailureCount;
			Out.Logf(TEXT("  FAIL  %s: a refused probe answered with a pointer"), Phase);
		}

		if (Outcome.GetDefaultRefusals != ExpectedRefusals
			|| Outcome.StaticLoadClassRefusals != ExpectedRefusals
			|| Outcome.AddSharedReferenceRefusals != ExpectedRefusals)
		{
			++FailureCount;
			Out.Logf(TEXT("  FAIL  %s: guard refusals per call were %d, %d and %d, expected %d"),
				Phase, Outcome.GetDefaultRefusals, Outcome.StaticLoadClassRefusals, Outcome.AddSharedReferenceRefusals, ExpectedRefusals);
		}

		// The managed reporting entry is called directly, so it must always count exactly one refusal: it is the
		// channel the managed guard uses when it refuses before any native guard can see the call.
		if (Outcome.ManagedRefusalRefusals != 1)
		{
			++FailureCount;
			Out.Logf(TEXT("  FAIL  %s: the managed refusal channel counted %d, expected 1"),
				Phase, Outcome.ManagedRefusalRefusals);
		}

		Out.Logf(TEXT("  %s  %s: gameThreadBit=%d refusalsPerProbe=%d/%d/%d managedChannel=%d"),
			FailureCount == 0 ? TEXT("PASS") : TEXT("INFO"), Phase, Outcome.GameThreadBit,
			Outcome.GetDefaultRefusals, Outcome.StaticLoadClassRefusals, Outcome.AddSharedReferenceRefusals, Outcome.ManagedRefusalRefusals);
	}

	void RunThreadGuardSelfTest(FOutputDevice& Out)
	{
		FProbeArguments Arguments;

		if (!ResolveProbes(Arguments, Out))
		{
			return;
		}

		Out.Logf(TEXT("UnrealSharp thread guard self test (game thread %u)"), FPlatformTLS::GetCurrentThreadId());

		int32 FailureCount = 0;
		const FProbeOutcome GameThreadOutcome = RunProbes(Arguments);
		ReportOutcome(Out, FailureCount, TEXT("game thread"), GameThreadOutcome, false);

		FProbeOutcome OffThreadOutcome;
		FEvent* ProbesDone = FPlatformProcess::GetSynchEventFromPool(true);
		Async(EAsyncExecution::ThreadPool, [&Arguments, &OffThreadOutcome, ProbesDone]()
		{
			OffThreadOutcome = RunProbes(Arguments);
			ProbesDone->Trigger();
		});
		ProbesDone->Wait();
		FPlatformProcess::ReturnSynchEventToPool(ProbesDone);

		ReportOutcome(Out, FailureCount, TEXT("thread pool"), OffThreadOutcome, true);

		Out.Logf(TEXT("UnrealSharp thread guard self test result: %s (%d failed check group(s))"),
			FailureCount == 0 ? TEXT("PASS") : TEXT("FAIL"), FailureCount);

		if (FailureCount != 0)
		{
			UE_LOGFMT(LogUnrealSharp, Error, "Thread guard self test failed with {0} failing check group(s).", FailureCount);
		}
	}

	FAutoConsoleCommandWithOutputDevice CmdThreadGuardSelfTest(
		TEXT("unrealsharp.ThreadGuardSelfTest"),
		TEXT("Deterministically verifies that the native engine-call guard refuses off-thread calls and counts each refusal once. The managed half runs at startup when unrealsharp.ThreadSelfTest is set or -UnrealSharpThreadGuardSelfTest is passed."),
		FConsoleCommandWithOutputDeviceDelegate::CreateStatic(&RunThreadGuardSelfTest));
}

#endif

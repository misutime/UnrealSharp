#include "CSThreadDiagnostics.h"

#include "UnrealSharpCore.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTLS.h"
#include "HAL/ThreadSafeCounter.h"
#include "Logging/StructuredLog.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	/** Bounded storage: thread ids and reason strings are unbounded inputs, the record set must not grow with them. */
	constexpr int32 MaxStoredRecords = 16;
	constexpr int32 MaxReasonLength = 64;

	struct FThreadDiagnosticRecord
	{
		TCHAR Reason[MaxReasonLength];
		uint32 ThreadId = 0;
		int32 State = 0;
		double TimeSeconds = 0.0;
	};

	FThreadDiagnosticRecord GRecords[MaxStoredRecords];
	int32 GRecordCount = 0;
	int32 GNextRecordIndex = 0;
	FCriticalSection GRecordsLock;

	FThreadSafeCounter GRefusalCount;
	FThreadSafeCounter GBoundaryCatchCount;

	/**
	 * 0 = Tier A only (record refusals), 1 = Tier A + minimal logs, 2 = Tier A + Tier B development checks.
	 * Read once at startup into the check sites that cache it, so changing it requires a restart; Tier A is never
	 * affected by this switch.
	 */
	static int32 GThreadChecks =
#if UE_BUILD_SHIPPING
		1;
#else
		2;
#endif

	static FAutoConsoleVariableRef CVarThreadChecks(
		TEXT("unrealsharp.ThreadChecks"),
		GThreadChecks,
		TEXT("0 = Tier A only (always on, records refusals), 1 = Tier A + minimal logs, 2 = Tier A + Tier B development checks. Requires a restart to take effect; Tier A is never disabled."),
		ECVF_Default);

	/**
	 * Off by default: the self test refuses calls on purpose, so a permanently enabled test would make the
	 * refusal counters unreadable. Setting it (or passing the command line switch) runs the managed half once
	 * at managed startup.
	 */
	static int32 GThreadSelfTest = 0;

	static FAutoConsoleVariableRef CVarThreadSelfTest(
		TEXT("unrealsharp.ThreadSelfTest"),
		GThreadSelfTest,
		TEXT("1 = run the managed thread guard self test once at startup. Off by default; the test records refusals on purpose. Requires a restart to take effect."),
		ECVF_Default);

	/**
	 * The development switch is read once into this snapshot: if each Bind class sampled the console variable
	 * on first touch, changing it at runtime would leave cold and hot classes configured differently. The
	 * snapshot is what the check sites read; the variable itself stays visible for the operator.
	 */
	static int32 GThreadChecksSnapshot = -1;
	static int32 GTierBEnabledSnapshot = 0;

	void EnsureStartupSnapshot()
	{
		if (GThreadChecksSnapshot >= 0)
		{
			return;
		}

		GThreadChecksSnapshot = GThreadChecks;
		GTierBEnabledSnapshot = GThreadChecks >= 2 ? 1 : 0;

		UE_LOGFMT(LogUnrealSharp, Display, "Thread checks snapshot: ThreadChecks={0} (Tier A always on, Tier B {1}).",
			GThreadChecksSnapshot, GTierBEnabledSnapshot != 0 ? TEXT("enabled") : TEXT("disabled"));
	}

	void StoreRecord(const TCHAR* What, int32 State)
	{
		FThreadDiagnosticRecord Record;
		FCString::Strncpy(Record.Reason, What != nullptr ? What : TEXT("unknown"), MaxReasonLength);
		Record.ThreadId = FPlatformTLS::GetCurrentThreadId();
		Record.State = State;
		Record.TimeSeconds = FPlatformTime::Seconds();

		FScopeLock Lock(&GRecordsLock);
		GRecords[GNextRecordIndex] = Record;
		GNextRecordIndex = (GNextRecordIndex + 1) % MaxStoredRecords;
		GRecordCount = FMath::Min(GRecordCount + 1, MaxStoredRecords);
	}

	void DumpThreadDiagnostics(FOutputDevice& Out)
	{
		Out.Logf(TEXT("unrealsharp.ThreadChecks = %d (0 = Tier A only, 1 = +logs, 2 = +Tier B; requires restart; Tier A is never disabled)"), GThreadChecks);
		UnrealSharp::ThreadDiagnostics::DumpRecentRecords(Out, MaxStoredRecords);
	}

	static FAutoConsoleCommandWithOutputDevice CmdThreadViolations(
		TEXT("unrealsharp.ThreadViolations"),
		TEXT("Prints the thread-affinity violation counters and the most recent records (bounded to the last 16)."),
		FConsoleCommandWithOutputDeviceDelegate::CreateStatic(&DumpThreadDiagnostics));
}

int32 UnrealSharp::ThreadDiagnostics::GetEngineCallState()
{
	int32 State = 0;

	if (::IsInGameThread())
	{
		State |= 1;
	}

	if (::IsGarbageCollecting())
	{
		State |= 2;
	}

	if (::IsGarbageCollectingAndLockingUObjectHashTables())
	{
		State |= 4;
	}

	return State;
}

bool UnrealSharp::ThreadDiagnostics::ShouldRefuseEngineCall(const TCHAR* What)
{
	const bool bOnGameThread = ::IsInGameThread();
	const bool bCollectingGarbage = ::IsGarbageCollecting();

	if (bOnGameThread && !bCollectingGarbage)
	{
		return false;
	}

	RecordRefusal(What);

	// Logging goes through the engine log only: it never re-enters the managed bridge, so a refusal cannot
	// recurse into the guard that produced it.
	UE_LOGFMT(LogUnrealSharp, Error, "Refusing {0}: called from thread {1} (IsInGameThread={2}, IsGarbageCollecting={3}).",
		What, FPlatformTLS::GetCurrentThreadId(), bOnGameThread, bCollectingGarbage);
	FDebug::DumpStackTraceToLog(ELogVerbosity::Error);

	return true;
}

void UnrealSharp::ThreadDiagnostics::RecordRefusal(const TCHAR* What)
{
	GRefusalCount.Increment();
	StoreRecord(What, GetEngineCallState());
}

void UnrealSharp::ThreadDiagnostics::RecordBoundaryCatch(const TCHAR* What)
{
	GBoundaryCatchCount.Increment();
	StoreRecord(What, GetEngineCallState());
}

bool UnrealSharp::ThreadDiagnostics::ShouldRunManagedSelfTest()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return GThreadSelfTest != 0 || FParse::Param(FCommandLine::Get(), TEXT("UnrealSharpThreadGuardSelfTest"));
#endif
}

bool UnrealSharp::ThreadDiagnostics::ShouldRunFinalizerSelfTest()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return ShouldRunManagedSelfTest()
		&& FParse::Param(FCommandLine::Get(), TEXT("UnrealSharpThreadGuardFinalizerTest"));
#endif
}

void UnrealSharp::ThreadDiagnostics::CaptureStartupSnapshot()
{
	EnsureStartupSnapshot();
}

int32 UnrealSharp::ThreadDiagnostics::GetThreadChecksSnapshot()
{
	EnsureStartupSnapshot();
	return GThreadChecksSnapshot;
}

bool UnrealSharp::ThreadDiagnostics::IsTierBEnabled()
{
	EnsureStartupSnapshot();
	return GTierBEnabledSnapshot != 0;
}

int32 UnrealSharp::ThreadDiagnostics::GetRefusalCount()
{
	return GRefusalCount.GetValue();
}

int32 UnrealSharp::ThreadDiagnostics::GetBoundaryCatchCount()
{
	return GBoundaryCatchCount.GetValue();
}

void UnrealSharp::ThreadDiagnostics::DumpRecentRecords(FOutputDevice& Out, int32 MaxCount)
{
	const int32 Refusals = UnrealSharp::ThreadDiagnostics::GetRefusalCount();
	const int32 BoundaryCatches = UnrealSharp::ThreadDiagnostics::GetBoundaryCatchCount();
	Out.Logf(TEXT("UnrealSharp thread diagnostics: refusals = %d, boundary catches = %d"), Refusals, BoundaryCatches);

	FScopeLock Lock(&GRecordsLock);
	const int32 Count = FMath::Clamp(MaxCount, 0, GRecordCount);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 RecordIndex = (GNextRecordIndex - 1 - Index + MaxStoredRecords * 2) % MaxStoredRecords;
		const FThreadDiagnosticRecord& Record = GRecords[RecordIndex];

		Out.Logf(TEXT("  [%d] %.3f thread=%u state=%d (inGameThread=%d, collectingGarbage=%d, lockingHashTables=%d) reason=%s"),
			Index,
			Record.TimeSeconds,
			Record.ThreadId,
			Record.State,
			(Record.State & 1) != 0,
			(Record.State & 2) != 0,
			(Record.State & 4) != 0,
			Record.Reason);
	}
}

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

/**
 * Thread-affinity diagnostics for the managed bridge.
 *
 * Deliberately independent of UCSManager: the state query must remain callable while the manager itself is not
 * initialized yet (bootstrap, editor startup) and must never create objects, resolve reflection or re-enter the
 * engine. Recording is thread safe, bounded and safe to call from a boundary where exceptions must not escape.
 */
namespace UnrealSharp
{
	namespace ThreadDiagnostics
	{
		/**
		 * Packed snapshot of the engine-call state.
		 *   bit0: executing on the game thread
		 *   bit1: a garbage collection is running
		 *   bit2: a garbage collection is locking the UObject hash tables (the fatal condition for object creation)
		 *
		 * This is a snapshot, not a lock: it reports what was true at the moment of the call and makes no promise
		 * about the state a moment later. Never cache the garbage collection bits.
		 */
		UNREALSHARPCORE_API int32 GetEngineCallState();

		/**
		 * Records one refusal: the call was rejected because of the current thread / garbage collection state.
		 */
		UNREALSHARPCORE_API void RecordRefusal(const TCHAR* What);

		/**
		 * Single entry point for the native guards: returns true when the caller must be refused, and records the
		 * refusal exactly once (counter + bounded record + one Error log with thread / GC state / stack).
		 * Callers only have to return their neutral value (nullptr / no state change) when this returns true.
		 */
		UNREALSHARPCORE_API bool ShouldRefuseEngineCall(const TCHAR* What);

		/**
		 * Records one violation observed inside a boundary where exceptions must not escape (managed finalizer,
		 * native to managed callback). Must not throw and must not log through the managed bridge.
		 */
		UNREALSHARPCORE_API void RecordBoundaryCatch(const TCHAR* What);

		/**
		 * True when the managed half of the guard self test should run at startup. Gated behind an explicit
		 * switch because the self test records refusals on purpose, which would otherwise pollute the counters
		 * an operator reads; it is compiled out of shipping builds.
		 */
		UNREALSHARPCORE_API bool ShouldRunManagedSelfTest();

		/**
		 * True when the self test may also collect objects whose finalizers touch engine state. This is a
		 * separate switch because that check can terminate the process if the non-escaping finalizer protocol is
		 * broken, so it is reserved for a dedicated run in its own process.
		 */
		UNREALSHARPCORE_API bool ShouldRunFinalizerSelfTest();

		UNREALSHARPCORE_API int32 GetRefusalCount();
		UNREALSHARPCORE_API int32 GetBoundaryCatchCount();

		/**
		 * Freezes the development switch once, so every check site reads the same value for the whole process.
		 * Called from module startup; the accessors below take the snapshot themselves if that has not happened
		 * yet, so there is never more than one capture.
		 */
		UNREALSHARPCORE_API void CaptureStartupSnapshot();
		UNREALSHARPCORE_API int32 GetThreadChecksSnapshot();
		UNREALSHARPCORE_API bool IsTierBEnabled();

		/** Appends the most recent records (bounded ring) to Out, newest first. */
		UNREALSHARPCORE_API void DumpRecentRecords(FOutputDevice& Out, int32 MaxCount);
	}
}
